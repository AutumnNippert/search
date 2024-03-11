// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#include "../search/search.hpp"
#include "../utils/pool.hpp"
#include "float.h"

void fatal(const char*, ...);	// utils.hpp

template <class D> struct AnytimeEES : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;

	struct Node {

		ClosedEntry<Node, D> closedent;

		// values for tracking location in focal, open, and f-ordered list
		bool open;
		int focalind;
		int cleanupind;

		Node *parent;
		PackedState state;
		Oper op, pop;
		Cost f, g, h;
		int depth, d;
		double hhat, fhat, dhat;

		Node() : open(false), focalind(-1), cleanupind(-1) {
		}

		static ClosedEntry<Node, D> &closedentry(Node *n) {
			return n->closedent;
		}

		static PackedState &key(Node *n) {
			return n->state;
		}
	};

	struct FOps {
		static void setind(Node *n, int i) {
			n->cleanupind = i;
		}

		static int getind(const Node *n) {
			return n->cleanupind;
		}
	
		static bool pred(Node *a, Node *b) {
			if (a->f == b->f) {
				if (a->d == b->d)
					return a->g > b->g;
				return a->d < b->d;
			}
			return a->f < b->f;
		}	
	};
  
	struct DHatOps {
		static void setind(Node *n, int i) {
			n->focalind = i;
		}

		static int getind(const Node *n) {
			return n->focalind;
		}
	
		static bool pred(Node *a, Node *b) {
			if (a->dhat == b->dhat) {
				if (a->fhat == b->fhat)
					return a->g > b->g;
				return a->fhat < b->fhat;
			}
			return a->dhat < b->dhat;
		}	
	};

	struct FHatOps {

		static double getvalue(const Node *n) {
			return n->fhat;
		}
	
		static bool pred(Node *a, Node *b) {
			/*
			if (a->fhat == b->fhat) {
				if (a->d == b->d)
					return a->g > b->g;
				return a->d < b->d;
			}
			*/
			return a->fhat < b->fhat;
		}	
	};

	AnytimeEES(int argc, const char *argv[]) :
			SearchAlgorithm<D>(argc, argv), herror(0), derror(0), 
			dropdups(false), closed(30000001) {
		dump = false;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-dump") == 0)
				dump = true;
			if (strcmp(argv[i], "-dropdups") == 0)
				dropdups = true;
		}

		wt = FLT_MAX;

		nodes = new Pool<Node>();
	}

	~AnytimeEES() {
		delete nodes;
	}

	Node *select_node() {
	  Node *bestDHat = NULL;
	  if(!focal.empty()) {
		bestDHat = *focal.front();
	  }
	  Node *bestFHat = open.front();
	  Node *bestF = *cleanup.front();

	  if(bestDHat && bestDHat->fhat <= wt*bestF->f) {
			focal.remove(bestDHat);
			open.remove(bestDHat);
			cleanup.remove(bestDHat);
			return bestDHat;
	  }

	  if(bestFHat->fhat <= wt*bestF->f) {
			open.remove(bestFHat);
			cleanup.remove(bestFHat);
			if(bestFHat->focalind >= 0) {
				focal.remove(bestFHat);
			}
			return bestFHat;
	  }

	  cleanup.remove(bestF);
	  open.remove(bestF);
	  if(bestF->focalind >= 0) {
			focal.remove(bestF);
	  }
	  return bestF;

	}

	void goalfound(Node *n) {
		if(!cand || n->g < cand->g) {
			cand = n;
			sol_count++;
			if(cleanup.empty()) {
				  wt = 1.0;
			} else {
			  wt = (double)cand->g / (*cleanup.front())->f;
			}
			dfrow(stdout, "incumbent", "uuuggg", sol_count, this->res.expd,
				  this->res.gend, wt, (float)cand->g,
				  walltime() - this->res.wallstart);
		}
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		closed.init(d);

		Node *n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);
		focal.push(n0);
		cleanup.push(n0);

		fhatmin = n0->fhat;
		
		if(dump) {
		  fprintf(stderr, "depth,expnum,state,g\n");
		}

		sol_count = 0;
		dfrowhdr(stdout, "incumbent", 6, "num", "nodes expanded",
				 "nodes generated", "solution bound", "solution cost",
				 "wall time");

		bool isIncrease;
		// Dummy node to represent weighted n0.
		Node *dummy = new Node();
		dummy->d = n0->d;
		dummy->g = n0->g;
		dummy->h = n0->h;
		dummy->f = n0->g + n0->h;
		dummy->dhat = wt * dummy->d;
		dummy->hhat = wt * dummy->h;
		dummy->fhat = wt * dummy->f;

		open.updateCursor(dummy, isIncrease);

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {
			Node *n = select_node();

			if(cand && n->f >= cand->f) {
				continue;
			}
			
			State buf, &state = d.unpack(buf, n->state);

			
			if(dump) {
			  fprintf(stderr, "%d,%lu,", n->depth,
					  SearchAlgorithm<D>::res.expd);
			  d.dumpstate(stderr, state);
			  fprintf(stderr, ",%f\n", (float)n->g);
			}

			if(d.isgoal(state)) {
				goalfound(n);
				continue;
			}
			
			expand(d, n, state);
		}

		if(cand) {
			solpath<D, Node>(d, cand, this->res);
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		focal.clear();
		cleanup.clear();
		closed.clear();
		delete nodes;
		nodes = new Pool<Node>();
		herror = 0;
		derror = 0;
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", "binary heap");
		dfpair(stdout, "node size", "%u", sizeof(Node));
		dfpair(stdout, "weight", "%lg", wt);
		dfpair(out, "h error last", "%g", herror);
		dfpair(out, "d error last", "%g", derror);
	}

private:

	void expand(D &d, Node *n, State &state) {
		SearchAlgorithm<D>::res.expd++;

        double herrnext = 0;
		double derrnext = 0;

        Node *bestkid = NULL;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;

			Node *kid = nodes->construct();
			typename D::Edge e(d, state, ops[i]);
			kid->depth = n->depth + 1;
			kid->g = n->g + e.cost;
			typename D::Cost h = d.h(e.state);
			kid->h = h;
			kid->f = kid->g + h;
			kid->d = d.d(e.state);
			d.pack(kid->state, e.state);

			if(cand && kid->f >= cand->g) {
				nodes->destruct(kid);
				continue;
			}

			unsigned long hash = kid->state.hash(&d);
			Node *dup = static_cast<Node*>(closed.find(kid->state, hash));
			if (dup) {
				this->res.dups++;
				if (dropdups || kid->g >= dup->g) {
					nodes->destruct(kid);
					continue;
				}
				if (dup->cleanupind >= 0) {
					this->res.reopnd++;
					open.remove(dup);
				}
				dup->f = dup->f - dup->g + kid->g;
				dup->g = kid->g;
				dup->depth = kid->depth;
				double dhat = dup->d / (1 - derror);
				double hhat = dup->h + (herror * dhat);
				dup->hhat = hhat;
				dup->dhat = dhat;
				dup->fhat = dup->g + dup->hhat;
				dup->parent = n;
				dup->op = ops[i];
				dup->pop = e.revop;
				open.push(dup);
				cleanup.pushupdate(dup, dup->cleanupind);
				if(dup->fhat <= wt * fhatmin) {
					focal.pushupdate(dup, dup->focalind);
				} else if(dup->focalind >= 0) {
					focal.remove(dup);
				}
				nodes->destruct(kid);
 
				if (!bestkid || dup->f < bestkid->f)
					bestkid = dup;
			} else {
				typename D::Cost h = d.h(e.state);
				kid->h = h;
				kid->f = kid->g + h;
				kid->d = d.d(e.state);
				double dhat = kid->d / (1 - derror);
				double hhat = kid->h + (herror * dhat);
				kid->hhat = hhat;
				kid->dhat = dhat;
				kid->fhat = kid->g + kid->hhat;
				kid->parent = n;
				kid->op = ops[i];
				kid->pop = e.revop;
				if(!d.isgoal(state)) {
					closed.add(kid, hash);
				}
				open.push(kid);
				cleanup.push(kid);
				if(kid->fhat <= wt * fhatmin) {
					focal.push(kid);
				}
 
				if (!bestkid || kid->f < bestkid->f)
					bestkid = kid;
			}
		}

		if (bestkid) {
		  double herr =  bestkid->f - n->f;
		  if (herr < 0)
		  	herr = 0;
		  double pastErrSum = herror * ((SearchAlgorithm<D>::res.expd)+imExp-1);
		  herrnext = (herr + pastErrSum)/((SearchAlgorithm<D>::res.expd)+imExp);
		  // imagine imExp of expansions with no error
		  // regulates error change in beginning of search

		  double derr = (bestkid->d+1) - n->d;
		  if (derr < 0)
			derr = 0;
		  if (derr >= 1)
			derr = 1 - geom2d::Threshold;
		  double pastDSum = derror * ((SearchAlgorithm<D>::res.expd)+imExp-1);
		  derrnext = (derr + pastDSum)/((SearchAlgorithm<D>::res.expd)+imExp);
		  // imagine imExp of expansions with no error
		  // regulates error change in beginning of search
		  
		  herror = herrnext;
		  derror = derrnext;
		}

		if(cleanup.empty())
		  return;

		Node *bestFHat = open.front();

		if(bestFHat->fhat != fhatmin) {
			fhatmin = bestFHat->fhat;
		    
			
			// Dummy node to represent weighted fhat min.
			Node *dummy = new Node();
			dummy->g = bestFHat->g;
			dummy->h = bestFHat->h;
			dummy->d = bestFHat->d;
			dummy->f = bestFHat->f;
			dummy->dhat = wt * bestFHat->dhat;
			dummy->hhat = wt * bestFHat->hhat;
			dummy->fhat = wt * dummy->g + dummy->hhat;

			bool isIncrease;
			auto itemsNeedUpdate = open.updateCursor(dummy, isIncrease);

			for (Node *item : itemsNeedUpdate) {
				if (isIncrease && item->focalind == -1) {
					focal.push(item);
				} else if(item->focalind != -1) {
					focal.remove(item);
				}
			}
		}
	}

	Node *init(D &d, State &s0) {
		Node *n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->h = d.h(s0);
		n0->depth = 0;
		n0->g = Cost(0);
		n0->f = n0->h + n0->g;
		n0->dhat = n0->d;
		n0->hhat = n0->h;
		n0->fhat = n0->f;
		n0->d = d.d(s0);
		n0->op = n0->pop = D::Nop;
		n0->parent = NULL;
		cand = NULL;
		return n0;
	}

	double herror;
	double derror;

	bool dropdups;
    bool dump;
	double wt;
	RBTree<FHatOps, Node*> open;
	BinHeap<DHatOps, Node*> focal;
	BinHeap<FOps, Node*> cleanup;
 	ClosedList<Node, Node, D> closed;
	Pool<Node> *nodes;
	double fhatmin;
	Node *cand;
	int sol_count;

    int imExp = 10;
};
