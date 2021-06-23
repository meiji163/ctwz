#include "ctw.hpp"
const double BETA_MAX = DBL_MAX/16;

double KT_estimator(bool obs, int n0, int n1){
	static std::unordered_map<int,double> _cache;
	int c = obs? n1 : n0;
	int h = ((n0 + n1)<<8) + c; 
	if( _cache.count(h) == 0){
		_cache[h] = ((double)c+0.0625)/((double)n0+n1+0.125);
	}
	return _cache[h];
}

ContextTree::ContextTree(std::size_t depth):_depth(depth){
	_root = std::make_unique<Node>(); 
}

void ContextTree::update(bool b, const std::deque<char> &ctx,
		double _probs[], double _betas[], Node* _path[]){
	Node* n = _root.get();
	for(int i=0; i<_depth; ++i){
		_path[i] = n;
		n = n->get_child(ctx[i]);
	}
	//calculate probabilites
	for(int i = _depth-1; i>=0; --i){
		n = _path[i];
		if(i == _depth-1){
			_probs[2*i] = KT_estimator(0,n->a,n->b);
			_probs[2*i+1] = KT_estimator(1,n->a,n->b); 
		}else if(n->beta < BETA_MAX){ 
			/* beta = KT_estimate(b|x_1..x_n) divided by 
			* product over children's probs Pr(b|x_1,...x_n)*/
			n->beta /= n->a + n->b + 0.125;
			_betas[2*i] = (n->beta)*(n->a + 0.0625);
			_betas[2*i+1] = (n->beta)*(n->b + 1);
			double eta_0 = _betas[2*i] + _probs[2*(i+1)];
			double eta_1 = _betas[2*i+1] + _probs[2*(i+1)+1];
			_probs[2*i] = eta_0/(eta_0 + eta_1);
			_probs[2*i+1] = eta_1/(eta_0 + eta_1);
			n->beta = _betas[2*i+b]/(_probs[2*(i+1)+b]);
		}else{
			n->beta = 1.0; 
		}
		if(b){ ++(n->b); }else{ ++(n->a); }
		if( ((n->a)>=128 && (n->b)>=128) 
			|| (n->a) >= 255 
			|| (n->b)>=255){
			n->a /= 2;
			n->b /= 2;
		} 
	}
}

void ContextTree::reupdate(bool b, 
		double _probs[], double _betas[], Node* _path[]){
	//correct dummy update
	int i = 0;
	while(i < _depth && _path[i] != nullptr){
		if(b){
			--(_path[i]->a);
			++(_path[i]->b);
		}else{
			--(_path[i]->b);
			++(_path[i]->a);
		}
		if(!_path[i]->is_leaf()){
			_path[i]->beta = _betas[2*i+b]/_probs[2*(i+1)+b];
		}else{
			break;
		}
		++i;
	}
}

bool ContextTree::Node::has_child(char c){ 
	return children.count(c)>0;
}

bool ContextTree::Node::is_leaf(){ 
	return children.empty();
}

ContextTree::Node* ContextTree::Node::get_child(char c){
	if(!has_child(c)){
		children[c] = std::make_unique<Node>();
	}
	return children[c].get();
}

bool AsciiTree::Node::is_leaf(){
	return children.empty();
}

bool AsciiTree::Node::has_child(bool c){
	return children.count(c)>0;
}

AsciiTree::Node* AsciiTree::Node::get_child(bool b){
	if(!has_child(b)){
		children[b] = std::make_unique<Node>();
	}
	return children[b].get();
}

AsciiTree::AsciiTree(std::size_t depth) :
	_depth(depth), _cached(false){
	_root = std::make_unique<Node>();
	_spawn(_root.get(), 0);
	for(int i=0; i<8; ++i){
		_probs[i] = new double[2*_depth];
		_betas[i] = new double[2*_depth];
		_path[i] = new ContextTree::Node*[_depth];
		for(int j=0; j<_depth; ++j){
			_path[i][j] = nullptr;
			_probs[i][2*j] = 1.0, _probs[i][2*j+1] = 1.0;
			_betas[i][2*j] = 1.0, _betas[i][2*j+1] = 1.0;
		}
	}
}

AsciiTree::~AsciiTree(){
	for(int i=0; i<8; ++i){
		delete[] _probs[i];
		delete[] _betas[i];
		delete[] _path[i]; 
	}
}

void AsciiTree::_spawn(Node* n, int depth){
	for( int i=0; i<2; ++i){ 
		n->children[i] = std::make_unique<Node>();
		n->ctx_tree = std::make_unique<ContextTree>(_depth);
		if(depth < 8){
			_spawn(n->get_child(i), depth+1);
		}
	}
}

void AsciiTree::update(char c){  
	Node* n = _root.get();
	for(int i = 7; i>=0; --i){
		//correct dummy update
		if((c >> i)&1){
			(n->ctx_tree)->reupdate(1, _probs[i], _betas[i], _path[i]);
			n = n->get_child(1);
		}else{
			n = n->get_child(0);
		}
	}
	_ctx.push_front(c);
	_ctx.pop_back();
	_cached = false;
}

double AsciiTree::predict_bit(Node* n, int i){
	(n->ctx_tree)->update(0, _ctx, _probs[i], _betas[i], _path[i]);
	return _probs[i][0]; //(n->ctx_tree)-> Pr(0)
}

double AsciiTree::predict(char c){ 
	if(_cached){
		return _prob;
	}
	Node* n = _root.get();
	bool b;
	double p = 1.0;
	double cp = 0.0; 
	for(int i = 7; i >= 0; --i){
		// dummy update
		(n->ctx_tree)->update(0, _ctx, _probs[i], _betas[i], _path[i]);
		b = (c >> i)&1;
		if(b){
			cp += p*_probs[i][0]; 
			p *= _probs[i][1];
			n = n->get_child(1);
		}else{
			p *= _probs[i][0];
			n = n->get_child(0);
		}
	}
	_cum_prob = cp; 
	_prob = p;
	_cached = true;
	return p; 
}

double AsciiTree::cum_prob(char c){ 
	if(!_cached){
		predict(c);
	}
	return _cum_prob;
}

char AsciiTree::decode(double cum_prob){
	Node* n = _root.get();
	int out = 0;
	double d;
	double p = 1.0;
	double cp = 0.0; 
	for(int i =7; i>=0; --i){
		(n->ctx_tree)->update(0, _ctx, _probs[i], _betas[i], _path[i]);
		d = p*_probs[i][0]; 
		if(cum_prob < cp+d){
			out = 2*out;
			p = d; 
			n = n->get_child(0);
		}else{
			out = 2*out +1;
			cp += d; 
			p *= _probs[i][1]; 
			n = n->get_child(1);
		}
	}
	_prob = p;
	_cum_prob = cp;
	_cached = true;
	return (char)out; 
}

void AsciiTree::load_context(const std::deque<char> &init_ctx){
	if(init_ctx.size() != _depth){
		std::cerr << "Context must be " << _depth << " chars" << std::endl;
		return;
	}
	_ctx = init_ctx; 
}

AsciiTree::Node* AsciiTree::get_root(){
	return _root.get();
}
