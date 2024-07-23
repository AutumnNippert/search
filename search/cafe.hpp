// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <unistd.h>

// For multithreading
#include <latch>
#include <thread>
#include <stop_token>

#define OPEN_LIST_SIZE 200000000

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

		static bool pred(const Node *a, const Node *b) {
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
			return Node::pred(&lhs, &rhs);
		}
	};

	CAFE(int argc, const char *argv[]):
	 SearchAlgorithm<D>(argc, argv), open(OPEN_LIST_SIZE){
		num_threads = 1;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0)
				num_threads = strtod(argv[++i], NULL);
		}
		// nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	~CAFE() {
		// delete nodes;
	}

	void thread_speculate(D &d, std::stop_token& token){
		NodePool<Node, NodeComp> nodes(OPEN_LIST_SIZE);
		while(!token.stop_requested()){
			HeapNode<Node, NodeComp> *hn = open.fetch_work();
			if(hn == nullptr){
				continue;
			}

			Node* n = &(hn->search_node);
			State buf, &state = d.unpack(buf, n->state);

			auto successor_ret = expand(d, hn, nodes, state);
			hn->set_completed(successor_ret.first, successor_ret.second);
		}
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		// get time
		auto start = std::chrono::high_resolution_clock::now();

		size_t speculated_nodes_expanded = 0;
		size_t manual_expansions = 0;

		std::vector<std::jthread> threads;
		std::stop_source stop_source;
		//std::latch start_latch(num_threads + 1);

		NodePool<Node, NodeComp> nodes(OPEN_LIST_SIZE);

		HeapNode<Node, NodeComp> * hn0 = init(d, nodes, s0);
		closed.emplace(hn0->search_node.state, hn0);
		open.push(hn0);

		// Create threads after initial node is pushed
		for (size_t i = 0; i < num_threads - 1; i++){
			stop_token st = stop_source.get_token();
			threads.emplace_back(&CAFE::thread_speculate, this, std::ref(d), std::ref(st));
		}

		//start_latch.arrive_and_wait(); // because the main thread starts before the threads. On small problems, I hypothesize this causes threads to never stop as the main thread exits too quickly or something along those lines.

		// get time after threads are initialized
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		std::cout << "Time to Initialize Threads: " << elapsed_seconds.count() << std::endl;

		std::cout << "All Threads Initialized. Beginning Algorithm." << std::endl;

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {	
			// std::cout << "Open: " << open << std::endl;
			// std::cout << "Open Pull" << std::endl;
			HeapNode<Node, NodeComp>* hn = open.get(0);
			open.pop();
			Node* n = &(hn->search_node);
			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);

				stop_source.request_stop();
				for (auto& thread : threads){
					thread.join();
				}

				std::cout << "Speculated Nodes Expanded: " << speculated_nodes_expanded << std::endl;
				std::cout << "Manual Expansions: " << manual_expansions << std::endl;
				break;
			}

			std::pair<HeapNode<Node, NodeComp> *, std::size_t> successor_ret;
			if(hn->is_completed()){
				// std::cout << "Speculated Node Expanded" << std::endl;
				speculated_nodes_expanded++;
				successor_ret = hn->get_successors();
			}else{
				// std::cout << "Manual Expansion" << std::endl;
				manual_expansions++;
				successor_ret = expand(d, hn, nodes, state);
			}

			SearchAlgorithm<D>::res.expd++;

			HeapNode<Node, NodeComp>* successors = successor_ret.first;
			size_t n_precomputed_successors = successor_ret.second;

			for (unsigned int i = 0; i < n_precomputed_successors; i++) {
				HeapNode<Node, NodeComp>* successor = &(successors[i]);
				Node *kid = &(successor->search_node);
				assert(kid);

				auto dup_it = closed.find(kid->state);
				if (dup_it != closed.end()) { // if its in closed, check if dup is better
					HeapNode<Node, NodeComp> * dup_hn = dup_it->second;
					Node &dup = dup_hn->search_node;
					this->res.dups++;
					if (kid->g >= dup.g) { // kid is worse so don't bother
						// nodes->destruct(kid);
						continue;
					}
					open.decrease_key(dup_hn->handle, successor);
				}
				else{
					open.push(successor); // add to open list
					SearchAlgorithm<D>::res.gend++;
				}
				closed.emplace(kid->state, successor); // add to closed list
				// std::cout << "Adding successor " << std::endl;
			}
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		// open.clear();
		closed.clear();
		// delete nodes;
		// nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		// closed.prstats(stdout, "closed ");
		// dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:
	size_t num_threads;
	std::pair<HeapNode<Node, NodeComp> *, std::size_t> expand(D& d, HeapNode<Node, NodeComp> * hn, NodePool<Node, NodeComp> &nodes, State& state) {
		Node * n = &(hn->search_node);

		typename D::Operators ops(d, state);
		auto successors = nodes.reserve(ops.size());
		size_t successor_count = 0;

		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;

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

	HeapNode<Node, NodeComp> * init(D &d, NodePool<Node, NodeComp> &nodes, State &s0) {
		HeapNode<Node, NodeComp> * hn0 = nodes.reserve(1);
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
};
