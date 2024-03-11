// Copyright © 2020 the Search Authors under the MIT license. See AUTHORS for the list of authors.                                                             
#pragma once                                                                    
#include "../search/search.hpp"                                                 
#include "../utils/pool.hpp"
#include <list>
                                                                                
template <class D> struct RectangleBeadSearch : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;                                
	typedef typename D::Cost Cost;                                              
	typedef typename D::Oper Oper;
  

	struct Node {
		int openind;
		Node *parent;
		PackedState state;
		Oper op, pop;
		int d, depth;
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
			if (a->d == b->d)
				return a->g > b->g;
			return a->d < b->d;
		}

		/* Priority of node. */
		static Cost prio(Node *n) {
			return n->d;
		}

		/* Priority for tie breaking. */
		static Cost tieprio(Node *n) {
			return n->g;
		}

    private:
		ClosedEntry<Node, D> closedent;
    
	};

	struct RingNode {
	  RingNode *next;
	  RingNode *prev;
	  OpenList<Node, Node, double> *list;
	  RingNode(OpenList<Node, Node, double> *l) {
		next = NULL;
		prev = NULL;
		list = l;
	  }
	  ~RingNode() {
		delete list;
	  }
	};
  
	struct Ring {
	  RingNode *begin;
	  RingNode *end;
	  int maxsize;
	  int size;
	  int removed;
	  int reused;

	  Ring() {
		begin = new RingNode(NULL);
		end = new RingNode(NULL);
		begin->next = end;
		begin->prev = end;
		end->next = begin;
		end->prev = begin;
		maxsize = 0;
		size = 0;
		removed = 0;
		reused = 0;
	  }

	  ~Ring() {
		while(begin->next != end) {
		  RingNode *temp = begin;
		  begin = begin->next;
		  delete temp;
		}
		delete begin;
		delete end;
	  }

	  void move_after(RingNode *n, RingNode *b) {
		n->prev->next = n->next;
		n->next->prev = n->prev;
		b->next->prev = n;
		n->next = b->next;
		n->prev = b;
		b->next = n;
	  }

	  void add() {
		if(begin->prev == end) {
		  RingNode *n = new RingNode(new OpenList<Node, Node, double>());
		  n->prev = end->prev;
		  n->next = end;
		  end->prev->next = n;
		  end->prev = n;
		  maxsize++;
		  size++;
		} else {
		  move_after(begin->prev, end->prev);
		  size++;
		  reused++;
		}
	  }

	  void remove_rest(RingNode *from) {
		RingNode *a = from->next;
		RingNode *b = end->prev;

		end->prev = from;
		from->next = end;
		a->prev = end;
		b->next = end->next;
		end->next->prev = b;
		end->next = a;
	  }

	  void remove() {
	    move_after(begin->next, end);
		size--;
		removed++;
	  }
	};
  

	RectangleBeadSearch(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		dropdups = false;
		dump = false;
		exponential_h = false;
		delta_height = 1;
		delta_base = 1;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-dropdups") == 0)
				dropdups = true;
			if (i < argc - 1 && (strcmp(argv[i], "-dH") == 0 ||
								 strcmp(argv[i], "-aspect") == 0))
				delta_height = strtod(argv[++i], NULL);
			if (i < argc - 1 && strcmp(argv[i], "-dB") == 0)
				delta_base = strtod(argv[++i], NULL);
			if (strcmp(argv[i], "-dump") == 0)
				dump = true;
			if (strcmp(argv[i], "-expo") == 0)
				exponential_h = true;
		}
    
		nodes = new Pool<Node>();
	}

	~RectangleBeadSearch() {
		delete nodes;
	}

	Node *dedup(D &d, Node *n) {

	  if(cand && n->f >= cand->g) {
		nodes->destruct(n);
		return NULL;
	  }
	  
	  unsigned long hash = n->state.hash(&d);
	  Node *dup = closed.find(n->state, hash);
	  if(!dup) {
		closed.add(n, hash);
	  } else {
		SearchAlgorithm<D>::res.dups++;
		if(!dropdups && n->g < dup->g) {
		  SearchAlgorithm<D>::res.reopnd++;
					
		  dup->f = dup->f - dup->g + n->g;
		  dup->g = n->g;
		  dup->d = n->d;
		  dup->parent = n->parent;
		  dup->op = n->op;
		  dup->pop = n->pop;
		} else {
		  nodes->destruct(n);
		  return NULL;
		}
	  }

	  return n;
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		closed.init(d);

		Node *n0 = init(d, s0);
		closed.add(n0);

		openlists.add();
		auto open_it = openlists.end->prev;
		open = open_it->list;
		auto last_filled = open_it;
		
		if(dump) {
		  fprintf(stderr, "depth,expnum,state,g\n");
		  fprintf(stderr, "0,%lu,", SearchAlgorithm<D>::res.expd);
		  State buf, &state = d.unpack(buf, n0->state);
		  d.dumpstate(stderr, state);
		  fprintf(stderr, ",%f\n", (float)n0->g);
		}

		open_count = 0;
		expand(d, n0, s0, 0);
		
		int width_inc = int(delta_base);
		int depth_todo = int(delta_height);
		int n_iter = 0;
		int exp_todo = width_inc;

		sol_count = 0;
		depth = 1;

		dfrowhdr(stdout, "incumbent", 5, "num", "nodes expanded",
			"nodes generated", "solution cost", "wall time");

		bool done = false;
    
		while (!done && !SearchAlgorithm<D>::limit()) {
		  done = true;
		  depth++;
		  n_iter++;

		  open_it = openlists.begin->next;
		  open = open_it->list;

		  openlists.add();
		  depth_todo = int(delta_height);
		  
		  if (exponential_h){
			delta_height = delta_height*2;
		  }
		  
		  int curr_depth = openlists.removed;
		  
		  // loop through all open lists, adding more at the end if needed
		  while(open_it->next != openlists.end) {
			curr_depth++;
			
			if(open_it->next->next != openlists.end) {
			  exp_todo = width_inc;
			} else {
			  exp_todo = n_iter * width_inc;
			  
			  // create new open lists for this iteration
			  if(depth_todo > 0 && !open->empty()) {
				openlists.add();
				depth_todo--;
			  } else {
				break;
			  }
			}
			exp_todo = std::min((int)open->size(), exp_todo);
			
			Node *arr[exp_todo];
			bool some_exp = false;
			for(int i = 0; i < exp_todo; i++) {
			  Node *n = NULL;
				  
			  while(!n && !open->empty()) {
				n = dedup(d, open->pop());
				open_count--;
			  }
			  
			  arr[i] = n;

			  if(n)
				some_exp = true;
			}

			// move to next open list
		    open_it = open_it->next;
			open = open_it->list;

			// record the last filled open list for pruning
			if(some_exp)
			  last_filled = open_it;

			// prune if this is the shallowest depth open list and it is empty
			if(!some_exp && done) {
				openlists.remove();
				continue;
			}

			// expand one or more nodes, based on slope
			for(int i = 0; i < exp_todo; i++) {
			  Node *n = arr[i];
			  if(!n)
				continue;
			  
			  State buf, &state = d.unpack(buf, n->state);
			  if(dump) {
				fprintf(stderr, "%d,%lu,", curr_depth,
						SearchAlgorithm<D>::res.expd);
				d.dumpstate(stderr, state);
				fprintf(stderr, ",%f\n", (float)n->g);
			  }
			  expand(d, n, state, curr_depth);
			}
			
			done = false;
		  }

		  // prune sequences of empty open lists from end
		  if(last_filled != openlists.end->prev) {
			openlists.remove_rest(last_filled);
		  }
		}

		if(cand) {
		  solpath<D, Node>(d, cand, this->res);
		  done = true;
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open->clear();
		closed.clear();
		delete nodes;
		nodes = new Pool<Node>();
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		closed.prstats(stdout, "closed ");
		dfpair(stdout, "open lists created", "%d", openlists.maxsize);
		dfpair(stdout, "open list type", "%s", open->kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}


private:

  void expand(D &d, Node *n, State &state, int curr_depth) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;
			considerkid(d, n, state, ops[i], curr_depth);
		}
	}

  void considerkid(D &d, Node *parent, State &state, Oper op, int curr_depth) {
		Node *kid = nodes->construct();
		assert (kid);
		typename D::Edge e(d, state, op);
		kid->g = parent->g + e.cost;
		d.pack(kid->state, e.state);

		kid->f = kid->g + d.h(e.state);
		kid->d = d.d(e.state);
		kid->parent = parent;
		kid->op = op;
		kid->pop = e.revop;
		
		State buf, &kstate = d.unpack(buf, kid->state);
		if (d.isgoal(kstate) && (!cand || kid->g < cand->g)) {
		  
		  if(dump) {
			fprintf(stderr, "%d,%lu,", curr_depth,
					SearchAlgorithm<D>::res.expd);
			d.dumpstate(stderr, kstate);
			fprintf(stderr, ",%f\n", (float)kid->g);
		  }
		
		  cand = kid;
		  sol_count++;
		  dfrow(stdout, "incumbent", "uuugg", sol_count, this->res.expd,
				this->res.gend, (float)cand->g,
				walltime() - this->res.wallstart);
		  return;
		} else if(cand && cand->g <= kid->f) {
		  nodes->destruct(kid);
		  return;
		}

		open_count++;
		open->push(kid);
	}

	Node *init(D &d, State &s0) {
		Node *n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->d = d.d(s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		cand = NULL;
		return n0;
	}

    bool dropdups;
    bool dump;
	bool exponential_h;
    Ring openlists;
 	OpenList<Node, Node, double> *open;
 	ClosedList<Node, Node, D> closed;
	Pool<Node> *nodes;
	Node *cand;
	int width;
	int depth;
	int open_count;
	int sol_count;
	int delta_height;
	int delta_base;
  
};
