// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <unistd.h>

#define OPEN_LIST_SIZE 20000000

template <class D> struct CAFE : public SearchAlgorithm<D> {

	typedef typename D::State State;
	typedef typename D::PackedState PackedState;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;


	struct StateEq{
		inline bool operator()(const PackedState& s, const PackedState& o) const{
			return s.eq(nullptr, o);
		}
	};

	struct StateHasher{
		inline unsigned long operator()(const PackedState& s) const{
			return const_cast<PackedState&>(s).hash(nullptr);
		}
	};

	struct NodeComp;

	struct Node {
		Node * parent;
		PackedState state;
		Oper op, pop;
		Cost f, g;

		Node(){}

		static PackedState &key(Node *n) {
			return n->state;
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

	CAFE(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv), open(OPEN_LIST_SIZE)/*, closed(30000001)*/ {
		nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	~CAFE() {
		delete nodes;
	}

	void search(D &d, typename D::State &s0) {
		this->start();

		HeapNode<Node, NodeComp> * hn0 = init(d, s0);
		closed.emplace(hn0->search_node.state, hn0);
		open.push(hn0);

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {	
			std::cout << "Open: " << open << std::endl;
			
			HeapNode<Node, NodeComp>* hn = open.get(0);
			open.pop();
			Node* n = &(hn->search_node);
			std::cout << "Popped Node: " << *n << std::endl;
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
				auto dup_it = closed.find(kid->state);
				if (dup_it != closed.end()) { // if its in closed, check if dup ois better
					HeapNode<Node, NodeComp> * dup_hn = dup_it->second;
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
				closed.emplace(kid->state, successor);
				std::cout << "search(): Adding successor " << *kid << std::endl;
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
		// closed.prstats(stdout, "closed ");
		// dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	std::pair<HeapNode<Node, NodeComp> *, std::size_t> expand(D& d, HeapNode<Node, NodeComp> * hn, State& state) {
		// std::cout << "Expand()" << std::endl;
		// std::cout << "Open: " << open;
		SearchAlgorithm<D>::res.expd++;

		Node * n = &(hn->search_node);

		typename D::Operators ops(d, state);

		
		auto successors = nodes->reserve(ops.size());
		size_t successor_count = 0;
		// std::cout << "Reserving: " << ops.size() << " successors" << std::endl;

		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			
			SearchAlgorithm<D>::res.gend++;
			Node * kid = &(successors[successor_count].search_node);	
			assert (kid != nullptr);
			successor_count++;
			typename D::Edge e(d, state, ops[i]);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = ops[i];
			kid->pop = e.revop;
			
			// std::cout << "Generated Node: " << *kid << std::endl;
		}
		// print each successor
		// for (unsigned int i = 0; i < successor_count; i++) {
		// 	std::cout << "expand(): Viewing Successors " << i << ": " << successors[i].search_node << std::endl;
		// }
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
	boost::unordered_flat_map<PackedState, HeapNode<Node, NodeComp> *, StateHasher, StateEq> closed;
	NodePool<Node, NodeComp> *nodes;
};
