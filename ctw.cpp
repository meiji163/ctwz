#include "ctw.hpp"
const double MAX = DBL_MAX/16;

double KT_estimator(bool obs, uint8_t a, uint8_t b){
	static std::unordered_map<int,double> _cache;
	int c = obs? b : a;
	int h = 0x200*((int)a + (int)b) + c; 
	if( _cache.count(h) == 0){
		_cache[h] = ((double)c+0.0625)/((double)a+(double)b+0.125);
	}
	return _cache[h];
}

ContextTree::ContextTree(unsigned int depth):_depth(depth){
	_root = std::make_unique<Node>(); 
	_probs = std::vector<double>(2*_depth, 1.0);
	_betas = std::vector<double>(2*_depth, 1.0);
	_path = std::vector<Node*>(_depth, nullptr); 
}

void ContextTree::update(bool obs, const std::deque<char> &ctx){
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
		}else if(n->beta < DBL_MAX/16){ 
			//beta = KT_estimate(x_1,...x_n)/ product over children P(x_1,...x_n)
			n->beta /= n->a + n->b + 0.125;
			_betas[2*i] = (n->beta)*(n->a + 0.0625);
			_betas[2*i+1] = (n->beta)*(n->b + 1);
			double eta_0 = _betas[2*i] + _probs[2*(i+1)];
			double eta_1 = _betas[2*i+1] + _probs[2*(i+1)+1];
			_probs[2*i] = eta_0/(eta_0 + eta_1);
			_probs[2*i+1] = eta_1/(eta_0 + eta_1);
			n->beta = _betas[2*i+obs]/(_probs[2*(i+1)+obs]);
		}else{
			n->beta = 1.0; 
		}
		if(obs){ ++(n->b);}else{++(n->a); }
		if( n->a >= 255 || n->b >= 255){
			n->a /= 2;
			n->b /= 2;
		} 
	}
}

void ContextTree::reupdate(bool obs){
	//correct dummy update
	int i = 0;
	while(i < _path.size() && _path[i] != nullptr){
		if(obs){
			--(_path[i]->a);
			++(_path[i]->b);
		}else{
			--(_path[i]->b);
			++(_path[i]->a);
		}
		if(!_path[i]->is_leaf()){
			_path[i]->beta = _betas[2*i+obs]/_probs[2*(i+1)+obs];
		}else{
			break;
		}
		++i;
	}
}
		
double ContextTree::predict(bool obs){
	return _probs[obs];
}

bool ContextTree::Node::has_child(char c){ 
	return children.count(c)>0;
}

bool ContextTree::Node::is_leaf(){ 
	return children.empty();
}

ContextTree::Node* ContextTree::Node::get_child(char c){
	if (!has_child(c)){
		children[c] = std::make_unique<Node>();
	}
	return children[c].get();
}

bool DTree::Node::is_leaf(){
	return children.empty();
}

bool DTree::Node::has_child(bool c){
	return children.count(c)>0;
}

DTree::Node* DTree::Node::get_child(bool c){
	if(!has_child(c)){
		children[c] = std::make_unique<Node>();
	}
	return children[c].get();
}

DTree::DTree(unsigned int depth) :
	_depth(depth), _cached(false){
	//build ASCII tree
	_root = std::make_unique<Node>();
	_spawn(_root.get(), 0);
}

void DTree::_spawn(Node* n, int depth){
	for( int i=0; i<2; ++i){ 
		n->children[i] = std::make_unique<Node>();
		n->ctx_tree = std::make_unique<ContextTree>(_depth);
		if(depth < 8){
			_spawn(n->get_child(i), depth+1);
		}
	}
}

void DTree::update(char c){  
	Node* n = _root.get();
	for(int i = 7; i>=0; --i){
		//correct dummy update
		if((c >> i)&1){
			(n->ctx_tree)->reupdate(1);
			n = n->get_child(1);
		}else{
			n = n->get_child(0);
		}
	}
	_ctx.push_front(c);
	_ctx.pop_back();
	_cached = false;
}

double DTree::predict(char c){ 
	if(_cached){
		return _prob;
	}
	Node* n = _root.get();
	double p = 1.0;
	double cp = 1.0; 
	for(int i = 7; i >= 0; --i){
		// dummy update
		(n->ctx_tree)->update(0, _ctx);
		bool b = (c >> i)&1;
		if(!b){
			cp *= (n->ctx_tree)->predict(0);
			p *= (n->ctx_tree)->predict(0);
			n = n->get_child(0);
		}else{
			p *= (n->ctx_tree)->predict(1);
			n = n->get_child(1);
		}
	}
	_cum_prob = cp - p;
	_prob = p;
	_cached = true;
	return p; 
}

double DTree::cum_prob(char c){ 
	if(!_cached){
		predict(c);
	}
	return _cum_prob;
}

char DTree::decode(double cum_prob){
	Node* n = _root.get();
	unsigned char out = 0;
	double p = 1.0;
	double cp = 1.0; 
	while(!n->is_leaf()){
		(n->ctx_tree)->update(0, _ctx);
		if(cum_prob < cp*((n->ctx_tree)->predict(0)) ){
			out = 2*out;
			cp *= (n->ctx_tree)->predict(0);
			p *= (n->ctx_tree)->predict(0);
			n = n->get_child(0);
		}else{
			out = 2*out +1;
			p *= (n->ctx_tree)->predict(1);
			n = n->get_child(1);
		}
	}
	_prob = p;
	_cum_prob = cp - p;
	_cached = true;
	return out; 
}

void DTree::load_context(const std::deque<char> &init_ctx){
	if(init_ctx.size() != _depth){
		std::cerr << "Context must be " << _depth << " chars" << std::endl;
		return;
	}
	_ctx = init_ctx; 
}

void DTree::print_ctx(){
	std::cout << "context: ";
	for( auto rit = _ctx.rbegin(); rit != _ctx.rend(); ++rit){
		std::cout << *rit;
	}
	std::cout << std::endl; 
}

