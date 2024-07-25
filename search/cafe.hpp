// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <unistd.h>

#include <cmath>

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

	static inline void dump_closed(std::ostream& stream, const boost::unordered_flat_map<PackedState, HeapNode<Node, NodeComp> *, StateHasher, StateEq>& c){
		stream << "Closed:\n";
		for (const auto & elem: c){
			stream << *((std::size_t *)(&elem.first)) << ": " << elem.second->search_node << "\n";
		}
		stream << "\n";
	}


	CAFE(int argc, const char *argv[]):
	 SearchAlgorithm<D>(argc, argv), open(OPEN_LIST_SIZE){
		num_threads = 1;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0){
				num_threads = strtod(argv[++i], NULL);
				if(num_threads <= 0 ) {
					exit(1);
				}
			}
			if (strcmp(argv[i], "-exp") == 0){
				extra_calcs = strtod(argv[++i], NULL);
			}
		}
		// nodes = new NodePool<Node, NodeComp>(OPEN_LIST_SIZE);
	}

	~CAFE() {
		// delete nodes;
	}

	void thread_speculate(D &d, std::stop_token token){
		NodePool<Node, NodeComp> nodes(OPEN_LIST_SIZE);
		while(!token.stop_requested()){
			auto start = std::chrono::high_resolution_clock::now();
			HeapNode<Node, NodeComp> *hn = open.fetch_work();
			if(hn == nullptr){
				thread_misses++;
				continue;
			}
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed_seconds = end - start;
			average_thread_fetch_time = (average_thread_fetch_time * nodes_speculated + elapsed_seconds.count()) / (nodes_speculated + 1);

			Node* n = &(hn->search_node);
			State buf, &state = d.unpack(buf, n->state);

			auto successor_ret = expand(d, hn, nodes, state);
			hn->set_completed(successor_ret.first, successor_ret.second);
			nodes_speculated++;
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
			threads.emplace_back(&CAFE::thread_speculate, this, std::ref(d), stop_source.get_token());
		}

		//start_latch.arrive_and_wait(); // because the main thread starts before the threads. On small problems, I hypothesize this causes threads to never stop as the main thread exits too quickly or something along those lines.

		// get time after threads are initialized
		auto end = std::chrono::high_resolution_clock::now();
		auto algo_start = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		std::cerr << "Time to Initialize Threads: " << elapsed_seconds.count() << std::endl;

		std::cerr << "All Threads Initialized. Beginning Algorithm." << std::endl;

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {	
			// std::cerr << "Open Pull" << std::endl;
			HeapNode<Node, NodeComp>* hn = open.get(0);
			open.pop();
			Node* n = &(hn->search_node);
			// std::cerr << "pop:" << *n << "\n";
			// dump_closed(std::cerr, closed);
			// std::cerr << open << std::endl;

			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				std::cerr << "Goal found!\n";
				stop_source.request_stop();
				// for (auto& thread : threads){
				// 	thread.join();
				// 	std::cerr << "joined!";
				// }

				auto algo_end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> elapsed_seconds_algo = algo_end - algo_start;

				std::cerr << "\n";
				std::cerr << "Total Time Taken: " << elapsed_seconds_algo.count() << " sec" << std::endl; 
				std::cerr << "Total Nodes Observed: " << total_nodes_observed << std::endl;
				std::cerr << "Total Speculated Nodes: " << nodes_speculated << std::endl;
				std::cerr << std::endl;
				std::cerr << "Speculated Nodes Expanded: " << speculated_nodes_expanded << std::endl;
				std::cerr << "Manual Expansions: " << manual_expansions << std::endl;
				std::cerr << "Time Spent Yielding: " << time_spent_yielding << " sec" << std::endl;
				std::cerr << "Total Sum: " << total_sum << std::endl;
				std::cerr << std::endl;
				std::cerr << "Average Expansion Time: " << average_expansion_time << " sec" << std::endl;
				std::cerr << std::endl;
				std::cerr << "Average Thread Fetch Time: " << average_thread_fetch_time << " sec" << std::endl;
				std::cerr << "CPU Time Spent Fetching: " << average_thread_fetch_time * nodes_speculated << " sec" << std::endl;
				std::cerr << "Thread Misses: " << thread_misses << std::endl;
				std::cerr << std::endl;
				std::cerr << "Expansion Rate: " << SearchAlgorithm<D>::res.expd / elapsed_seconds_algo.count() << "/sec" << std::endl;
				std::cerr << "Observe Rate: " << total_nodes_observed / elapsed_seconds_algo.count() << "/sec" << std::endl;
				std::cerr << std::endl;
				break;
			}
			SearchAlgorithm<D>::res.expd++;
			std::pair<HeapNode<Node, NodeComp> *, std::size_t> successor_ret;
			// if(hn->is_completed()){
			// 	// std::cerr << "Speculated Node Expanded" << std::endl;
			// 	speculated_nodes_expanded++;
			// 	successor_ret = hn->get_successors();
			// }else{
			// 	// std::cerr << "Manual Expansion" << std::endl;
			// 	manual_expansions++;
			// 	successor_ret = expand(d, hn, nodes, state);
			// }

			if(hn->reserve()){
				// expand manually
				manual_expansions++;
				successor_ret = expand(d, hn, nodes, state);
			}else{
				// wait for speculation
				// start timer
				auto start_yield = std::chrono::high_resolution_clock::now();
				while(!hn->is_completed()){
					std::this_thread::yield();
				}
				auto end_yield = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> elapsed_seconds_yield = end_yield - start_yield;
				time_spent_yielding += elapsed_seconds_yield.count();
				speculated_nodes_expanded++;
				successor_ret = hn->get_successors();
			}

			HeapNode<Node, NodeComp>* successors = successor_ret.first;
			size_t n_precomputed_successors = successor_ret.second;

			for (unsigned int i = 0; i < n_precomputed_successors; i++) {
				SearchAlgorithm<D>::res.gend++;
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
					//std::cerr << "duplicate:" << *kid << " " << dup << " same state? "<< StateEq()(kid->state, dup.state) << "\n";
					//std::cerr << open << "\n";
					closed[kid->state] = successor; // add to closed list
				}
				else{
					open.push(successor); // add to open list
					// std::cerr << "push:" << successor->search_node << "\n";
					// std::cerr << open << "\n";
					closed[kid->state] = successor; // add to closed list
				}
				
				// std::cerr << "Adding successor " << std::endl;
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
	size_t total_nodes_observed = 0;
	size_t nodes_speculated = 0;

	long double average_thread_fetch_time = 0; // including finding a node
	size_t thread_misses = 0;
	
	long double average_expansion_time = 0;
	long double time_spent_yielding = 0;

	long double total_sum = 0;

	size_t extra_calcs = 0;
	

	std::pair<HeapNode<Node, NodeComp> *, std::size_t> expand(D& d, HeapNode<Node, NodeComp> * hn, NodePool<Node, NodeComp> &nodes, State& state) {
		auto start = std::chrono::high_resolution_clock::now();
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
			
			// std::cerr << "Generated Node: " << *kid << std::endl;
		}
		// print each successor
		// for (unsigned int i = 0; i < successor_count; i++) {
		// 	std::cerr << "expand(): Viewing Successors " << i << ": " << successors[i].search_node << std::endl;
		// }

		// wait function
		long double sum = 0;
		for(size_t i = 0; i < extra_calcs; i++){
			sum = sin(sum + rand());
		}
		total_sum += sum;

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		average_expansion_time = (average_expansion_time * total_nodes_observed + elapsed_seconds.count()) / (total_nodes_observed + 1);
		total_nodes_observed++;

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
