// Autumn Nippert
// 5/18/2024
// CA*FE: Concurrent A* for Fast Expansions

#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"
#include <atomic>
#include <thread>

template <class D> struct Naive_CAFE : public SearchAlgorithm<D> { // what is a template? (https://www.geeksforgeeks.org/templates-cpp/)

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;
	
	struct Node {
		ClosedEntry<Node, D> closedent; // what is a closed entry?
		int openind; // Open Index
		Node *parent; // Parent Node
		std::atomic<int> isWorking; // 0 if not touched, 1 if being expanded, 2 if expanded
		vector<Node*> children; // Vector of children nodes
		PackedState state;
		Oper op, pop;
		Cost f, g;

		Node() : openind(-1) {}

		static ClosedEntry<Node, D> &closedentry(Node *n) {
			return n->closedent;
		}

		static PackedState &key(Node *n) {
			return n->state;
		}

		static void setind(Node *n, int i) {
			n->openind = i;
		}

		static int getind(const Node *n) {
			return n->openind;
		}
	
		static bool pred(Node *a, Node *b) {
			if (a->f == b->f)
				return a->g > b->g;
			return a->f < b->f;
		}

		static Cost prio(Node *n) {
			return n->f;
		}

		static Cost tieprio(Node *n) {
			return n->g;
		}
	};

	Naive_CAFE(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		nodes = new Pool<Node>();
	}

	~Naive_CAFE() {
		delete nodes;
	}

	void search(D &d, typename D::State &s0) { // Main While Loop
		this->start();
		closed.init(d);

		Node *n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {
			Node *n = open.pop();
			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				break;
			}
			if(n->isWorking == 0){ // If not expanded, expand in the main thread
				n->isWorking = 1;
				expand(d, n, state);
			}
			while(n->isWorking == 1){
				// busy wait as the node is currently being expanded by a child
			}
			// for each child node, spawn a thread to expand it
			for(size_t i = 0; i < n->children.size(); i++){
				Node *child = n->children[i];
				child->isWorking = 1;
				// if the child is not in the closed list, add it to the closed list and the open list
				unsigned long hash = child->state.hash(&d);
				if(!closed.find(child->state, hash)){
					closed.add(child);
					open.push(child);
				} // Duplicates should not appear so if they are in the closed list, they should not be re-expanded
				// start and detach a thread to expand the child node
				std::thread t(&Naive_CAFE::expand, this, std::ref(d), child, std::ref(d.unpack(buf, child->state)));
				t.detach();
			}
		}
		this->finish();
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
	void expand(D &d, Node *n, State &state) { // counts generated and expanded nodes and calls considerkid on each possible option for a child
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop) 
				continue;
			SearchAlgorithm<D>::res.gend++;

			Node *parent = n;
			Oper op = ops[i];

			Node *kid = nodes->construct(); // gets child node of operation
			assert (kid);
			typename D::Edge e(d, state, op);
			kid->g = parent->g + e.cost;
			d.pack(kid->state, e.state); // packs the state of the child node

			kid->f = kid->g + d.h(e.state);
			kid->parent = parent;
			kid->op = op;
			kid->pop = e.revop;
			n->children.push_back(kid);
		}
		n->isWorking = 2; // sets parent node to done
	}
	
	Node *init(D &d, State &s0) {
		Node *n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		return n0;
	}

	OpenList<Node, Node, Cost> open;
 	ClosedList<Node, Node, D> closed;
	Pool<Node> *nodes;
};
