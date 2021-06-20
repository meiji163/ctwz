#include "ctw.hpp"
#include <fstream>
#define HALF 0x8000
#define QTR  0x4000
#define THREE_QTR 0xc000

void write(bool b, int n, std::queue<bool> &buf){
	buf.push(b); 
	for(int i = 0; i<n; ++i){
		buf.push(!b);
	}
}

void encode_char(double p, double cum_p, std::queue<bool> &obuf, bool eot=false){
	static int high = 0xffff;
	static int low = 0;
	static int count = 0;
	int range = high-low+1;
	if(eot){
		count++;
		if( low < QTR){
			write(0, count, obuf);
		}else{
			write(1, count, obuf);
		}
		return;
	}
	//std::cout << cd_prob << " " << cum_prob << std::endl;
	//std::cout << high << " " << low << std::endl;
	low += cum_p*range; 
	high = low + range*p; 

	// rescaling
	for(;;){
		if( low >= HALF ){
			low -= HALF;
			high -= HALF; 
			write(1, count, obuf);
			count = 0;
		}else if( high < HALF){
			write(0, count, obuf);
			count = 0;
		}else if( QTR <= low && high < THREE_QTR){
			count++;
			low -= QTR;
			high -= QTR;
		}else{
			break;
		}
		low *= 2; 
		high = 2*high+1; 
	}
}

char decode_char(int code, DTree &T, std::deque<bool> &ibuf ){
	static int high = 0xffff;
	static int low = 0;
	int range = high-low+1;

	char c = T.decode( ((double) code - low+1) / ((double) range));
	double p = T.predict(c);
	double cum_p = T.cum_prob(c);
	T.update(c);

	low += cum_p*range; 
	high = low + range*p; 
	for(;;){
		if (low >= HALF ){
			low -= HALF;
			high -= HALF; 
			code -= HALF;
		}else if (high < HALF){
		}else if (low >= QTR && high < THREE_QTR){
			code -= QTR;
			low -= QTR;
			high -= QTR;
		}else{
			break;
		}
		low *= 2;
		high = 2*high + 1;
		code *= 2;
		if(!ibuf.empty()){
			code += ibuf.front();
			ibuf.pop_front();
		}
	}
	return c;
}

char output_byte(std::queue<bool> &buf){
	int c = 0; 
	for( int i =0; i<8; ++i){
		c *= 2;
		if(!buf.empty()){
			c += buf.front();
			buf.pop();
		}
	}
	return (char)c;
}

void input_byte(char c, std::queue<bool> &buf){
	auto b = std::bitset<8>(c); 
	for(int i=7; i>=0 ; --i){
		std::cout << b[i];
		buf.push(b[i]);
	}
	std::cout << std::endl;
}

int main(int argc, char** argv){
	bool enc = true;
	if(argc < 3){
		return 1;
	}
	int depth = atoi(argv[1]);
	std::ifstream file(argv[2], std::ios::in | std::ios::binary);
	if(!file.is_open()){
		throw std::runtime_error(std::string() + "Can't open file " + argv[2]);
		return 1;
	}

	if(enc){
		char c;
		DTree T(depth);
		std::queue<bool> obuf;
		std::deque<char> ctx;
		for(int i=0; i<depth; ++i){
			file.get(c);
			ctx.push_front(c);
		}
		T.load_context(ctx);
		long long int eb = 0, rb =0;
		while(file.get(c)){
			++rb;
			double p = T.predict(c);
			double cp = T.cum_prob(c);
			encode_char(p,cp,obuf);
			T.update(c);
			if(obuf.size() >= 8){
				++eb;
				output_byte(obuf);
			}
			if(rb% 0x2000 == 0){
				std::cout << "\r\e[K (encoded) " << (eb >> 10) << " KiB"
				<< " | " << (rb >> 10) << " KiB (read)" <<  std::flush;
			}
		}
		std::cout << std::endl << eb << " B" << std::endl;
	}
	return 0;
}

