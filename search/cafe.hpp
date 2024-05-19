// Autumn Nippert
// 5/18/2024
// CA*FE: Concurrent A* for Fast Expansions

#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"
#include <atomic>
#include <thread>

template <class D> struct CAFE : public SearchAlgorithm<D> { // what is a template? (https://www.geeksforgeeks.org/templates-cpp/)

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;
	
	struct Node {
		ClosedEntry<Node, D> closedent; // what is a closed entry?
		int openind; // Open Index
		Node *parent; // Parent Node
		std::atomic<bool> isWorking; // if the node is being worked on by a thread
		vector<Node> children; // Vector of children nodes
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

	CAFE(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		nodes = new Pool<Node>();
	}

	~CAFE() {
		delete nodes;
	}

	void threadsearch(D &d, State &state){
		Node *tempArray[100];
		bool selected = false;
		while(true){
            Node *nodeToExpand = NULL;
			while(!selected){
				// Select a node from an array (for now just called tempArray)
				// atomically check if the node is working and set it to working if it is not
				// if it is working, continue
				for(int i = 0; i < tempArray.size; i++){
					if(&tempArray[i]->isWorking.atomic_compare_exchange_strong(false, true)){
						nodeToExpand = tempArray[i];
						selected = true;
						break;
					}
				}
			}
			// Expand the node
			expand(d, nodeToExpand, state);
		}
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
			if(n.isWorking.compare_exchange_strong(0, 1)) {
				threadexpand(d, n, state); // expand manually
				n.isWorking = 2;
			} 
			while(n.isWorking != 2) { // waits for thread to finish (if its done, it just continues)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			closed.add(n);
			for (unsigned int i = 0; i < n.children.size(); i++) {
				Node *kid = n.children[i];
				if (closed.mem(kid))
					continue;
				if (open.mem(kid)) {
					if (kid->g < open[kid].g) {
						open.update(kid);
					}
					continue;
				}
				open.push(kid);
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
	/**
	 * @brief expands a node given a domain and a state into children that are stored within the node
	 * 
	 * @param d domain
	 * @param n node to expand
	 * @param state state of the node
	 */
	void threadexpand(D &d, Node *n, State &state) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;
			// considerkid code but slightly modified to not add to open list (it also doesn't need to worry about the closed list since the main thread will handle that)
			Node *kid = nodes->construct(); // nodes->construct() is a function that returns a new node from the pool?
			assert (kid);
            Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state); // expands the nodes?

			// unsigned long hash = kid->state.hash(&d); // ill hash later in the main thread
			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;
			this.children.push_back(kid);
		}
	}

	void expand(D &d, Node *n, State &state) { // counts generated and expanded nodes and calls considerkid on each possible option for a child
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;
			considerkid(d, n, state, ops[i]);
		}
	}

	void considerkid(D &d, Node *parent, State &state, Oper op) { // considers possible children and adds them to the open list if they are not already in the closed list
		Node *kid = nodes->construct();
		assert (kid);
		typename D::Edge e(d, state, op);
		kid->g = parent->g + e.cost;
		d.pack(kid->state, e.state);

		unsigned long hash = kid->state.hash(&d);
		Node *dup = closed.find(kid->state, hash);
		if (dup) {
			this->res.dups++;
			if (kid->g >= dup->g) {
				nodes->destruct(kid);
				return;
			}
			bool isopen = open.mem(dup);
			if (isopen)
				open.pre_update(dup);
			dup->f = dup->f - dup->g + kid->g;
			dup->g = kid->g;
			dup->parent = parent;
			dup->op = op;
			dup->pop = e.revop;
			if (isopen) {
				open.post_update(dup);
			} else {
				this->res.reopnd++;
				open.push(dup);
			}
			nodes->destruct(kid);
		} else {
			kid->f = kid->g + d.h(e.state);
			kid->parent = parent;
			kid->op = op;
			kid->pop = e.revop;
			closed.add(kid, hash);
			open.push(kid);
		}
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
