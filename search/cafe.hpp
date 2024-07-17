// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap.hpp"
#include <unistd.h>

#define OPEN_LIST_SIZE 2000000

template <class D> struct CAFE : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;

	struct NodeComp;

	struct Node {
		ClosedEntry<HeapNode<Node, NodeComp>, D> closedent;
		//int openind;
		Node * parent;
		PackedState state;
		Oper op, pop;
		Cost f, g;

		// Node() : openind(-1) {
		// }
		Node(){}

		static ClosedEntry<HeapNode<Node, NodeComp>, D> &closedentry(Node *n) {
			return n->closedent;
		}

		static PackedState &key(Node *n) {
			return n->state;
		}

		// static void setind(Node *n, int i) {
		// 	n->openind = i;
		// }

		// static int getind(const Node *n) {
		// 	return n->openind;
		// }
	
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

		static void printNode(Node *n, std::ostream &o) {
			o << "[g:" << n->g << ", f:" << n->f << ", op:" << n->op << ", pop:" << n->pop << "]" << std::endl;
		}

		inline friend std::ostream& operator<<(std::ostream& stream, const Node& node){
			stream << "[g:" << node.g << ", f:" << node.f << ", op:" << node.op << ", pop:" << node.pop << "]";
			return stream;
		}
	};

	struct NodeComp{
		bool operator()(const Node& lhs, const Node& rhs) const{
			return Node::prio(&lhs) < Node::prio(&rhs);
		}
	};

	template <typename HeapNode, typename Ops>
	struct ClosedHelper{
		static PackedState& key(HeapNode * n){
			return Ops::key(&n->search_node);
		}

		static ClosedEntry<HeapNode, D>& closedentry(HeapNode *n){
			return Ops::closedentry(&n->search_node);
		}
	};

	CAFE(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), open(OPEN_LIST_SIZE), closed(30000001) {
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
			std::cout << "Open: " << open << std::endl;

			HeapNode<Node, NodeComp>* hn = open.get(0);
			open.pop();
			Node* n = &(hn->search_node);
			std::cout << "Popped Node: ";
			Node::printNode(n, std::cout);
			//sleep(1);
			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				break;
			}

			auto successor_ret = expand(d, hn, state);

			HeapNode<Node, NodeComp>* successors = successor_ret.first;
			size_t n_precomputed_successors = successor_ret.second;
			for (unsigned int i = 0; i < n_precomputed_successors; i++) {
				HeapNode<Node, NodeComp>* successor = &(successors[i]);
				Node *kid = &(successor->search_node);
				assert(kid);
				unsigned long hash = kid->state.hash(&d);
				hash--;
				std::cerr << kid;
				std::cerr << " " << &kid->state;
				std::cerr << " " << hash << "\n";
				HeapNode<Node, NodeComp> *dup_hn = closed.find(kid->state, hash);
				if (dup_hn != nullptr) { // if its in closed, check if dup ois better
					dup_hn = open.get(dup_hn->handle);
					Node &dup = dup_hn->search_node;
					this->res.dups++;
					if (kid->g >= dup.g) { // kid is worse so don't bother
						// nodes->destruct(kid);
						continue;
					}
					dup.f = dup.f - dup.g + kid->g;
					dup.g = kid->g;
					dup.parent = n;
					dup.op = kid->op;
					dup.pop = kid->pop;
					// nodes->destruct(kid);
					open.decrease_key(dup_hn->handle);
					continue;
				}
				closed.add(successor, hash);
				open.push(successor);
			}
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		// open.clear();
		closed.clear();
		delete nodes;
		nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		closed.prstats(stdout, "closed ");
		// dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	std::pair<HeapNode<Node, NodeComp> *, std::size_t> expand(D& d, HeapNode<Node, NodeComp> * hn, State& state) {
		SearchAlgorithm<D>::res.expd++;

		Node * n = &(hn->search_node);

		typename D::Operators ops(d, state);

		
		auto successors = nodes->reserve(ops.size());
		size_t successor_count = 0;
		std::cout << "Reserving: " << ops.size() << " successors" << std::endl;

		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			
			SearchAlgorithm<D>::res.gend++;
			Node * kid = &(successors[i].search_node);	
			assert (kid != nullptr);
			successor_count++;
			typename D::Edge e(d, state, ops[i]);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = ops[i];
			kid->pop = e.revop;
			
			std::cout << "Generated Node: ";
			Node::printNode(kid, std::cout);
		}
		// hn->set_completed();
		// print generated node: node
		std::cout << "Returning " << successor_count << " successors" << std::endl;
		return std::make_pair(successors, successor_count);
	}

	HeapNode<Node, NodeComp> * init(D &d, State &s0) {
		HeapNode<Node, NodeComp> * hn0 = nodes->reserve(1);
		Node* n0 = &(hn0->search_node);
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
