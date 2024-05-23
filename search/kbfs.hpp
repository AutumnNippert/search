// Autumn Nippert
// KBFS: K-Best First Search
// https://link.springer.com/article/10.1023/A:1024452529781

#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"
#include <atomic>
#include <thread>

template <class D> struct KBFS : public SearchAlgorithm<D> { // what is a template? (https://www.geeksforgeeks.org/templates-cpp/)

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

	KBFS(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		nodes = new Pool<Node>();
	}

	~KBFS() {
		delete nodes;
	}

	void search(D &d, typename D::State &s0) { // Main While Loop
		this->start();
		closed.init(d);
		int k = 5;

		Node *n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);

		// create vector of k threads
		vector<thread> threads;
		vector<Node*> executingNodes;
		bool goalFound = false;

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {
			for (int i = 0; i < k; i++) {
				if (open.empty())
					break;
				Node *n = open.pop();
				State buf, &state = d.unpack(buf, n->state);

				if (d.isgoal(state)) { // while getting k nodes, if goal is found, break out of the for loop
					solpath<D, Node>(d, n, this->res);
					goalFound = true;
					break;
				}

				executingNodes.push_back(n);
				std::thread t(&KBFS::expand, this, std::ref(d), n, std::ref(state));
				threads.push_back(std::move(t));
			}

			if (goalFound) // if goal is found, break out of while loop
				break;

			for (size_t i = 0; i < threads.size(); i++) {
				threads[i].join();
			}
			threads.clear();
			for (size_t i = 0; i < executingNodes.size(); i++) {
				Node *n = executingNodes[i];
				for (size_t j = 0; j < n->children.size(); j++) {
					Node *child = n->children[j];
					unsigned long hash = child->state.hash(&d);
					if(!closed.find(child->state, hash)){
						closed.add(child);
						open.push(child);
					}
				}
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
			if (ops[i] == n->pop) // what is pop?
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
