// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap.hpp"

#define OPEN_LIST_SIZE 100000000

template <class D> struct CAFE : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;
	
	struct Node {
		ClosedEntry<Node, D> closedent;
		int openind;
		Node * parent;
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

		static Cost prio(const Node *n) {
			return n->f;
		}

		static Cost tieprio(const Node *n) {
			return n->g;
		}
	};

	struct NodeComp{
		bool operator()(const Node& lhs, const Node& rhs){
			return Node::prio(&lhs) < Node::prio(&rhs);
		}
	};

	template <typename HeapNode, typename Ops>
	struct ClosedHelper{
		static PackedState& key(HeapNode * n){
			return Ops::key(n);
		}

		static ClosedEntry<HeapNode, D>& closedentry(Node *n){
			return Ops::closedentry(n);
		}
	};

	CAFE(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), closed(30000001) {
		nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	~CAFE() {
		delete nodes;
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		closed.init(d);

		HeapNode<Node, NodeComp> * n0 = init(d, s0);
		closed.add(n0);
		open.push(n0);

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {
			const HeapNode<Node, NodeComp>*hn = open.top(); // does not get or pop
			Node* n = hn->search_node;
			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				break;
			}

			expand(d, hn, state);
			typename D::Edge e(d, n->state, n->op);

			for (Node* kid : n->children){
				// Create a heap node version of the child
				HeapNode<Node, NodeComp>*hn = nodes->reserve(1);
				hn->search_node = kid;
				
				unsigned long hash = kid->state.hash(&d);
				Node *dup = closed.find(kid->state, hash);
				if (dup) {
					this->res.dups++;
					if (kid->g >= dup->g) { // kid is worse so don't bother
						// nodes->destruct(kid);
						// WE SHALL WASTE RAM MUAHAHAHA
						continue;
					}
					// Else, update existing duplicate with better path
					// bool isopen = open.mem(dup);
					// if (isopen)
					// 	open.pre_update(dup);
					dup->f = dup->f - dup->g + kid->g;
					dup->g = kid->g;
					dup->parent = n;
					dup->op = kid->op;
					dup->pop = e.revop;
					// if (isopen) {
					// 	open.post_update(dup);
					// } else {
					// 	this->res.reopnd++;
					// 	open.push(dup);
					// }
					// nodes->destruct(kid);
					open->decrease_key(dup->openind);
					continue;
				}
				// add to closed and open
				// cout << "Adding a child to open" << endl;
				closed.add(kid, hash);

				hn->handle = open.push(hn);
			}
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		closed.clear();
		delete nodes;
		nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	void expand(D& d, HeapNode<Node, NodeComp> * hn, State& state) {
		SearchAlgorithm<D>::res.expd++;

		Node * n = hn->search_node;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;

			Node * kid = nodes->construct(); // nodes->construct() is a function that returns a new node from the pool?
			assert (kid);
			Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			// unsigned long hash = kid->state.hash(&d);
			// Node *dup = closed.find(kid->state, hash);
			// if (dup) {
			// 	this->res.dups++;
			// 	if (kid->g >= dup->g) {
			// 		nodes->destruct(kid);
			// 		continue;
			// 	}
			// 	bool isopen = open.mem(dup);
			// 	if (isopen)
			// 		open.pre_update(dup);
			// 	dup->f = dup->f - dup->g + kid->g;
			// 	dup->g = kid->g;
			// 	dup->parent = n;
			// 	dup->op = op;
			// 	dup->pop = e.revop;
			// 	if (isopen) {
			// 		open.post_update(dup);
			// 	} else {
			// 		this->res.reopnd++;
			// 		open.push(dup);
			// 	}
			// 	nodes->destruct(kid);
			// 	continue;
			// }

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;
			// closed.add(kid, hash);

			// open.push(kid);
			hn->precomputed_successors.push_back(kid);
		}
		hn->set_completed();
	}

	HeapNode<Node, NodeComp> * init(D &d, State &s0) {
		HeapNode<Node, NodeComp> * hn0 = nodes->reserve(1);
		Node * n0 = hn0->search_node;
		d.pack(n0->state, s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		return hn0;
	}

	CafeMinBinaryHeap<Node, NodeComp> open;
 	ClosedList<ClosedHelper<HeapNode<Node, NodeComp>, Node>, HeapNode<Node, NodeComp>, D> closed;
	NodePool<Node, NodeComp> *nodes;
};
