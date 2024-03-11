// Copyright © 2020 the Search Authors under the MIT license. See AUTHORS for the list of authors.                                                             
#pragma once                                                                    
#include "../search/search.hpp"                                                 
#include "../utils/pool.hpp"
                                                                                
template <class D> struct CABSearch : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;                                
	typedef typename D::Cost Cost;                                              
	typedef typename D::Oper Oper;

	struct Node {
		int openind;
		Node *parent;
		PackedState state;
		Oper op, pop;
		Cost f, g;

		Node() : openind(-1) {
		}

		static ClosedEntry<Node, D> &closedentry(Node *n) {
			return n->closedent;
		}

		static PackedState &key(Node *n) {
			return n->state;
		}

		/* Set index of node on open list. */
		static void setind(Node *n, int i) {
			n->openind = i;
		}

		/* Get index of node on open list. */
		static int getind(const Node *n) {
			return n->openind;
		}

		/* Indicates whether Node a has better value than Node b. */
		static bool pred(Node *a, Node *b) {
			if (a->f == b->f)
				return a->g > b->g;
			return a->f < b->f;
		}

		/* Priority of node. */
		static Cost prio(Node *n) {
			return n->f;
		}

		/* Priority for tie breaking. */
		static Cost tieprio(Node *n) {
			return n->g;
		}

    private:
		ClosedEntry<Node, D> closedent;
    
	};

	CABSearch(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		dropdups = false;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-dropdups") == 0)
				dropdups = true;
		}
    
		nodes = new Pool<Node>();
	}

	~CABSearch() {
		delete nodes;
	}

	bool beam_search(D &d, Node *n0, int width) {

		open.push(n0);

		bool done = false;
		bool none_pruned = true;
		
		Node **beam = new Node*[width];

		while (!open.empty() && !done && !SearchAlgorithm<D>::limit()) {
			int c = 0;
			while(c < width && !open.empty()) {
				Node *n = open.pop();

				unsigned long hash = n->state.hash(&d);
				Node *dup = closed.find(n->state, hash);
				if(!dup) {
					// prune nodes with f worse than incumbent cost
					if (!cand || n->f < cand->g) {
						closed.add(n, hash);
					} else {
						continue;
					}
				} else {
					SearchAlgorithm<D>::res.dups++;
					if(!dropdups && n->g < dup->g) {
						SearchAlgorithm<D>::res.reopnd++;
					
						dup->f = dup->f - dup->g + n->g;
						dup->g = n->g;
						dup->parent = n->parent;
						dup->op = n->op;
						dup->pop = n->pop;
					} else {
						continue;
					}
				}
				
				beam[c] = n;
				c++;
			}

			if (c == 0) {
				done = true;
			}

			if (!open.empty()) {
				none_pruned = false;
			}

			while(!open.empty())
				nodes->destruct(open.pop());
      
			for(int i=0; i < c && !done && !SearchAlgorithm<D>::limit(); i++) {
				Node *n = beam[i];
				State buf, &state = d.unpack(buf, n->state);
				
				if (expand(d, n, state)) {
					// print incumbent info
					row(num_sols, width, cand->g);
				}

				beam[i] = NULL;
			}
		}

		while(!open.empty())
		  nodes->destruct(open.pop());
		closed.clear();

		return none_pruned;
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		closed.init(d);
		
		Node *n0 = init(d, s0);

		unsigned int width = 1;
		num_sols = 0;

		rowhdr();
    
		while (!beam_search(d, n0, width) && !SearchAlgorithm<D>::limit()) {
		  width = width * 2;
		}

		if (cand) {
		  solpath<D, Node>(d, cand, this->res);
		}
		
		this->finish();
	}
  
	// rowhdr outputs the incumbent solution row header line.
	void rowhdr() {
		dfrowhdr(stdout, "incumbent", 6, "num", "nodes expanded",
			"nodes generated", "beam width", "solution cost",
			"wall time");
	}

	// row outputs an incumbent solution row.
	void row(unsigned long n, unsigned long width, Cost cost) {
		dfrow(stdout, "incumbent", "uuuugg", n, this->res.expd,
			  this->res.gend, width, (float)cost,
			  walltime() - this->res.wallstart);
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		closed.clear();
		delete nodes;
		nodes = new Pool<Node>();
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}


private:

	bool expand(D &d, Node *n, State &state) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		bool solfound = false;
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;
			solfound = solfound || considerkid(d, n, state, ops[i]);
		}

		return solfound;
	}

	bool considerkid(D &d, Node *parent, State &state, Oper op) {
		Node *kid = nodes->construct();
		assert (kid);
		typename D::Edge e(d, state, op);
		kid->g = parent->g + e.cost;
		d.pack(kid->state, e.state);

		kid->f = kid->g + d.h(e.state);
		kid->parent = parent;
		kid->op = op;
		kid->pop = e.revop;
		
		State buf, &kstate = d.unpack(buf, kid->state);
		if (d.isgoal(kstate) && (!cand || kid->g < cand->g)) {
		  cand = kid;
		  num_sols++;
		  return true;
		}
		
		open.push(kid);
		return false;
	}

	Node *init(D &d, State &s0) {
		Node *n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		cand = NULL;
		return n0;
	}

    bool dropdups;
	OpenList<Node, Node, Cost> open;
 	ClosedList<Node, Node, D> closed;
	Pool<Node> *nodes;
	Node *cand;
	unsigned int num_sols;
  
};
