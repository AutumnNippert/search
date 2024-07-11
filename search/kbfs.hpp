// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
void busywait()  __attribute__((optimize(0)));
void busywait(){
	return;
}

template <class D> struct KBFS : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;
	
	struct Node {
		ClosedEntry<Node, D> closedent;
		int openind;
		Node *parent;
		vector<Node*> children; // Vector of children nodes
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

	void search(D &d, typename D::State &s0) {
		//kpbfs main thread
		long k = 2;

		this->start();
		closed.init(d);

		Node *n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);

		boost::asio::thread_pool tp(k);

		bool found = false;

		while (!open.empty() && !SearchAlgorithm<D>::limit() && !found) {

			// get k top nodes on open list (less than k if open list is smaller)

			std::vector<Node*> topNodes;
			for (int i = 0; i < k; i++) {
				if (open.empty()) {
					break;
				}
				topNodes.push_back(open.pop());
			}

			size_t completedCount = 0;

			// give nodes to thread pool
			size_t size = topNodes.size();
			for (size_t i = 0; i < size; i++) {
				// cout << "ThreadPool :: Posting to thread pool" << endl;
				Node* n = topNodes[i];
				State buf, &state = d.unpack(buf, n->state);
				boost::asio::post(tp, [this, &d, n, &state, &completedCount](){
					kbfsWorker(d, n, state, completedCount);
				});
			}

			// wait till done
			while(completedCount < size){
				busywait();
			}

			// cout << "kbatch of size " << topNodes.size() << " done" << endl;

			// add all to open
			for (size_t i = 0; i < topNodes.size(); i++) {
				Node* n = topNodes[i];
				State buf, &state = d.unpack(buf, n->state);

				// cout << "Checking node with " << n->children.size() << " children" << endl;
				for (Node* kid : n->children){

					if (d.isgoal(state)) {
						solpath<D, Node>(d, n, this->res);
						tp.join();
						found = true; // should break out of the outer while loop
						break;
					}

					unsigned long hash = kid->state.hash(&d);
					Node *dup = closed.find(kid->state, hash);
					if (dup) {
						this->res.dups++;
						if (kid->g >= dup->g) { // kid is worse so don't bother
							nodes->destruct(kid);
							continue;
						}
						// Else, update existing duplicate with better path
						bool isopen = open.mem(dup);
						if (isopen)
							open.pre_update(dup);
						dup->f = dup->f - dup->g + kid->g;
						dup->g = kid->g;
						dup->parent = n;
						dup->op = kid->op;
						// dup->pop = e.revop; // I literally can't find what e.revop is
						if (isopen) {
							open.post_update(dup);
						} else {
							this->res.reopnd++;
							open.push(dup);
						}
						nodes->destruct(kid);
						continue;
					}
					// add to closed and open
					// cout << "Adding a child to open" << endl;
					closed.add(kid, hash);
					open.push(kid);
				}
			}
			// cout << "openlist size: " << open.size() << endl;
		}
		
		this->finish();
	}

	void kbfsWorker(D &d, Node *n, State &state, size_t &completedCount){
		expandAndStore(d, n, state);
		completedCount++; // should increment when worker is done?
		// cout << "kbfs worker completed. Current count completed: " << completedCount << endl;
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

	void expandAndStore(D &d, Node *n, State &state) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;

			Node *kid = nodes->construct(); // nodes->construct() is a function that returns a new node from the pool?
			assert (kid);
            Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;
			// closed.add(kid, hash);
			n->children.push_back(kid);
			// cout << "expandAndStore() :: Added child" << endl;
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
