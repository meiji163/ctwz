/*
* Copyright (c) 2021 Meijke Balay 
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ctw.hpp"
#include <fstream>
#include <filesystem>
#include <cassert>
#include <stdio.h>

#define VERSION "CTWZ-0.1"
#define HIGH 0xffffff
#define HALF 0x800000
#define QTR 0x400000
#define THREE_QTR 0xc00000

void write(bool b, int n, std::queue<bool> &buf){
	buf.push(b); 
	for(int i = 0; i<n; ++i){
		buf.push(!b);
	}
}

void encode_bit(bool b, double p0, std::queue<bool> &out, bool eot=false){
	static int low=0, high = HIGH;
	static int count=0;
	int range = high-low+1;
	if(eot){
		++count;
		if(low<QTR){
			write(0,count,out);
		}else{
			write(1,count,out);
		}
		return;
	}
	if(b){
		low += p0*range;
	}else{
		high = low + range*p0;
	}
	while(true){
		if( high < HALF){
			write(0,count,out);
			count = 0;
		}else if( low >= HALF){
			low -= HALF;
			high -= HALF;
			write(1,count,out);
			count = 0;
		}else if ( QTR <= low && high < THREE_QTR){
			++count;
			low -= QTR;
			high -= QTR;
		}else{ break; }
		low *= 2;
		high = 2*high +1;
	}
}

bool decode_bit(double p0, int &code, std::queue<bool> &bits){
	static int low=0, high=HIGH;
	int range = high-low+1;
	double d = (code-low+1)/((double)range);
	bool b = (d<p0)?0:1;  
	if(b){
		low += p0*range;
	}else{
		high = low+range*p0;
	}
	while(true){ 
		if( high< HALF){
		}else if( low >= HALF){
			low -= HALF;
			high -= HALF;
			code -= HALF;
		}else if( QTR <= low && high < THREE_QTR){ 
			code -= QTR;
			low -= QTR;
			high -= QTR;
		}else{ break;}
		low *= 2;
		high = 2*high+1;
		code = 2*code + bits.front();
		bits.pop();
	}
	return b;
}

void encode_char(AsciiTree &T, char c, std::queue<bool> &obuf){ 
	AsciiTree::Node* n = T.get_root();
	bool b;
	double p0;
	for(int i=7; i>=0; --i){ 
		b = (c>>i)&1;
		p0 = T.predict_bit(n,i);
		n = n->get_child(b);
		encode_bit(b,p0,obuf);
		//std::cout << " " << p0 << std::endl;
	}
	T.update(c);
}

char decode_char(AsciiTree &T, int &code, std::queue<bool> &bits){ 
	AsciiTree::Node* n = T.get_root();
	bool b;
	double p0;
	int c = 0;
	for(int i=7; i>=0; --i){
		p0 = T.predict_bit(n,i);
		b = decode_bit(p0,code,bits);
		n = n->get_child(b);
		c = 2*c+b; 
	}
	T.update((char)c);
	return (char)c;
}

char output_byte(std::queue<bool> &buf){
	int c = 0; 
	for( int i =0; i<8; ++i){
		if(buf.empty()){
			c *= 2;
		}else{
			c = 2*c + buf.front();
			buf.pop();
		}
	}
	return (char)c;
}

void input_byte(char c, std::queue<bool> &buf){
	for(int i=7; i>=0 ; --i){
		bool b = (c >> i)&1;
		buf.push(b);
	}
}

void encode_file(char* fname, int depth){
	std::ifstream file(fname, std::ios::in | std::ios::binary);
	if(!file.is_open()){
		throw std::runtime_error(std::string() + "Can't open file " + fname); 
	}
	std::ofstream of(std::string() + fname + ".cz", std::ios::out | std::ios::binary);
	std::filesystem::path path{fname};
	int bytes = std::filesystem::file_size(path); 
	of << VERSION << std::endl << path.filename() 
		<< " " << depth << " "<< bytes << std::endl; 

	//load context
	char c;
	std::queue<bool> obuf;
	std::deque<char> ctx;
	for(int i=0; i<depth; ++i){
		file.get(c);
		of.put(c);
		ctx.push_front(c);
	}
	AsciiTree T(depth);
	T.load_context(ctx);

	long long eb = 0, rb =0;
	while(file.get(c)){
		++rb;
		encode_char(T,c,obuf);
		if(obuf.size() >= 8){
			++eb;
			of.put(output_byte(obuf));
		}
		if(rb% 0x4000 == 0){
			std::cout << "\r\e[K (encoded) " << (eb >> 10) << " KiB"
			<< " | " << (rb >> 10) << " KiB (read)" <<  std::flush;
		}
	}
	//end transmission
	encode_bit(0,0,obuf,true); 
	while(!obuf.empty()){
		of.put(output_byte(obuf));
	}
	file.close();
	std::cout << std::endl << eb << " bytes" << std::endl;
}

bool ask_replace(const std::string &file){
	std::cout << file << " already exists. Replace it? (y/n)\t"; 
	char rep;
	std::cin >> rep;
	while(true){
		if(rep == 'n'){ 
			return 0;
		}else if(rep == 'y'){
			return 1;
		}else{
			std::cout << std::endl;
			std::cin >> rep;
		}
	}
}

void decode_file(char* fname){
	std::ifstream file(fname, std::ios::in | std::ios::binary);
	if(!file.is_open()){
		throw std::runtime_error(std::string() + "Can't open file " + fname);
		return;
	}
	//read header
	std::string of_name, ver;
	file >> ver, assert(ver == VERSION);
	char c;
	int bytes, depth;
	file >> of_name >> depth >> bytes;
	file.get(c);
	assert(c == '\n');
	of_name.erase(
		remove( of_name.begin(), of_name.end(), '\"' ),
		of_name.end()
	);
	std::cout << of_name << " " << depth << " "<< bytes << std::endl;
	if(std::filesystem::exists(of_name)){
		if(!ask_replace(of_name)){
			return;
		}
	}
	std::ofstream of(of_name, std::ios::out | std::ios::binary);

	//load context
	std::deque<char> ctx;
	std::queue<bool> ibuf;
	for(int i=0; i<depth; ++i){
		file.get(c);
		ctx.push_front(c);
		of.put(c);
		--bytes;
	}
	AsciiTree T(depth);
	T.load_context(ctx);

	//initialize 3 bytes of encoded data
	for(int i=0; i<3; ++i){
		file.get(c);
		input_byte(c,ibuf);
	}
	int code = 0;
	for(int i=0; i<24; ++i){
		code = 2*code + ibuf.front();
		ibuf.pop();
	}
	char d;
	while(bytes--){
		if(ibuf.size() < 64){  
			for(int i=0; i<8; ++i){
				if(file.get(c)){
					input_byte(c, ibuf);
				}else{
					input_byte(0, ibuf);
				}
			}
		}
		d = decode_char(T,code,ibuf);
		of.put(d);
	}
	file.close();
	of.close();
}

void usage(){
	std::cout << "ctwz:\n"
		<< "\tContext tree weighting compressor\n"
		<< "\tauthor: Meijke Balay <mysatellite99@gmail.com>\n"
		<< "usage:\n" 
		<< "\tencode: ctwz [-d depth] file\n" 
		<< "\tdecode: ctwz -x file" << std::endl;
	exit(0);
}

int main(int argc, char* argv[]){
	int depth = 8;
	bool decode = false;
	if(argc < 2){
		usage();
	}
	for(int i=1; i<argc-1; ++i){
		if(strcmp(argv[i],"-d")==0){
			if(i+2<argc && atoi(argv[i+1])>0 && atoi(argv[i+1])<16){
				depth = atoi(argv[i+1]); 
				++i;
			}else{
				usage();
			}
		}else if(strcmp(argv[i],"-x")==0){
			decode=true;
		}else if(strcmp(argv[i],"-h")==0){
			usage();
		}else{
			usage();
		}
	}
	if(decode){
		decode_file(argv[argc-1]);
	}else{
		encode_file(argv[argc-1], depth);
	}
	return 0;
}
