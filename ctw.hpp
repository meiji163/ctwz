#include <unordered_map>
#include <deque> 
#include <queue> 
#include <memory> 
#include <string>
#include <utility> 
#include <cstdint> 
#include <cfloat>
#include <cmath>
#include <iostream>
#include <vector>

#pragma once

/* Context tree takes character contexts and does binary predictions */
class ContextTree{
	struct Node{
		typedef std::unique_ptr<Node> uptr;
		double beta;
		uint8_t a,b;
		std::unordered_map<char, uptr> children;
		Node() : a(0), b(0), beta(1.0){}
		bool has_child(char c);
		bool is_leaf();
		Node* get_child(char c);
	};

	public:
		unsigned int _depth;
		ContextTree(unsigned int depth);
		void update(bool obs, const std::deque<char> &ctx);
		void reupdate(bool obs);
		double predict(bool obs);

	private:
		Node::uptr _root;
		std::vector<double> _probs;
		std::vector<double> _betas;
		std::vector<Node*> _path;
};

/* Decomposition Context Tree Weighting (DE-CTW) model converts character prediction to binary predictions.
 * Leaves of the decomposition tree are characters and internal vertices have a context tree. */
class DTree {
	struct Node{
		typedef std::unique_ptr<Node> uptr;
		std::unordered_map<bool,uptr> children;
		std::unique_ptr<ContextTree> ctx_tree;
		Node(){}
		bool is_leaf();
		bool has_child(bool c);
		Node* get_child(bool c);
	};

	public:
		DTree(unsigned int depth);
		unsigned int _depth;
		void load_context(const std::deque<char> &init_ctx);
		void update(char c);
		double predict(char c);
		double cum_prob(char c);
		char decode(double cum_prob);
		void print_ctx();
	private: 
		Node::uptr _root;	
		std::deque<char> _ctx;
		double _cum_prob;
		double _prob;
		bool _cached;
		void _spawn(Node* n, int depth);
};


