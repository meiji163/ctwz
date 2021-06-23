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

#include <unordered_map>
#include <deque> 
#include <queue> 
#include <memory> 
#include <utility> 
#include <cstdint> 
#include <cfloat>
#include <cmath>
#include <iostream>
#include <vector>

#pragma once

/* ContextTree takes character contexts and does binary predictions
 * using the Context Tree Weighting algorithm (rf. Willems, Shtarkov, Tjalkens)*/
class ContextTree{
	public:
		struct Node{
			typedef std::unique_ptr<Node> uptr;
			double beta;
			uint8_t a,b; // 0-1 counters
			std::unordered_map<char, uptr> children;
			Node() : a(0), b(0), beta(1.0){}
			bool has_child(char c);
			bool is_leaf();
			Node* get_child(char c);
		};
		uint8_t _depth;
		ContextTree(std::size_t depth);
		void update(bool b, const std::deque<char> &ctx,
				double _probs[], double _betas[], Node* _path[]);
		void reupdate(bool b, 
				double _probs[], double _betas[], Node* _path[]);
	private:
		Node::uptr _root;
};

/* Simple ASCII decomposition tree has context trees in internal nodes and characters in the leaves. */
class AsciiTree {
	public:
		struct Node{
			typedef std::unique_ptr<Node> uptr;
			std::unordered_map<bool,uptr> children;
			std::unique_ptr<ContextTree> ctx_tree;
			Node(){}
			bool is_leaf();
			bool has_child(bool c);
			Node* get_child(bool c);
		};

		AsciiTree(std::size_t depth);
		~AsciiTree();
		void load_context(const std::deque<char> &init_ctx);
		void update(char c);
		double predict(char c);
		double cum_prob(char c);
		char decode(double cum_prob);
		double predict_bit(Node* n, int i);
		Node* get_root();
	private: 
		std::size_t _depth;
		Node::uptr _root;	
		std::deque<char> _ctx;
		double _cum_prob;
		double _prob;
		bool _cached;
		void _spawn(Node* n, int depth);
		double* _probs[8];
		double* _betas[8];
		ContextTree::Node** _path[8];
};

