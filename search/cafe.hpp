// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../Cafe_Heap/cafe_heap2.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/circular_buffer.hpp>
#include <unistd.h>

#include <cmath>

// For multithreading
#include <latch>
#include <thread>
#include <stop_token>

#include <atomic>

#define OPEN_LIST_SIZE 100000000
#define TOP_QUEUE_SIZE 8
#define RECENT_QUEUE_SIZE 16
#define DEBUG true

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
		// size_t nodes_expanded_at_time_of_expansion;

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

	static inline std::size_t get_num_threads(int argc, const char *argv[]){
		std::size_t num_threads = 1;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0){
				num_threads = strtod(argv[++i], NULL);
				if(num_threads <= 0 ) {
					exit(1);
				}
			}
		}
		return num_threads;
	}

	CAFE(int argc, const char *argv[]):
	 SearchAlgorithm<D>(argc, argv),open(OPEN_LIST_SIZE, RECENT_QUEUE_SIZE, TOP_QUEUE_SIZE){
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
	    wmdat.resize(num_threads-1);
		for(std::size_t i = 1; i < num_threads; i++){
			wmdat[i-1].heap_top_fetch.resize(TOP_QUEUE_SIZE);
        	wmdat[i-1].recent_push_fetch.resize(RECENT_QUEUE_SIZE);
		}
		node_pools.reserve(num_threads);
		for (size_t i = 0; i < num_threads; i++){
			node_pools.emplace_back(OPEN_LIST_SIZE);
		}
	}

	~CAFE() {
		// delete nodes;
	}

	void thread_speculate(D &d, size_t id, std::stop_token token, NodePool<Node, NodeComp>& nodes, WorkerMetadata& wm){
		// NodePool<Node, NodeComp> nodes(OPEN_LIST_SIZE);
		while(!token.stop_requested()){
			// long double sum = 0;
			// for(size_t i = 0; i < 10; i++){
			// 	sum = sin(sum + rand());
			// }
			// total_sum += sum;
			// auto start = std::chrono::high_resolution_clock::now();
			// search open_queue
			//bool found = false;
			HeapNode<Node, NodeComp> *hn = nullptr;
			// for (size_t i = 0; i < open_queue.size(); i++){
			// 	if (token.stop_requested()){
			// 		return;
			// 	}
				
			// 	hn = open_queue[i];
			// 	if(hn == nullptr){
			// 		break; // if the node is null, leave the loop
			// 	}
			// 	assert(hn != nullptr);
			// 	int res = hn->reserve();
			// 	if (res == 0){
			// 		found = true;
			// 		break;
			// 	}
			// 	else if(res == 2){
			// 		speculations_collided++;
			// 	}
			// 	failed_reserves++;
			// }
			// if hn still bad, try to get from open
			// if (!found){
			// 	thread_misses++;
			// 	continue; // skip trying to search the open list
				hn = fetch_work(open, wm);
				if(hn == nullptr){
					thread_misses++;
					continue;
				}
			// }

			Node* n = &(hn->search_node);
			State buf, &state = d.unpack(buf, n->state);

			auto successor_ret = expand(d, hn, nodes, state);
			waste_time(extra_calcs);
			hn->set_completed(successor_ret.first, successor_ret.second);
			total_nodes_speculated++;
		}
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		// get time
		auto start = std::chrono::high_resolution_clock::now();
		//std::size_t s_i = 0;
        // volatile std::size_t * sum_i = &s_i;

		size_t speculated_nodes_expanded = 0;
		size_t manual_expansions = 0;
		WorkerMetadata prec_expanded_source;
		prec_expanded_source.heap_top_fetch.resize(TOP_QUEUE_SIZE);
		prec_expanded_source.recent_push_fetch.resize(RECENT_QUEUE_SIZE);

		std::vector<std::jthread> threads;
		std::stop_source stop_source;
		//std::latch start_latch(num_threads + 1);
		
		auto& nodes = node_pools[0];

		HeapNode<Node, NodeComp> * hn0 = init(d, nodes, s0);
		closed.emplace(hn0->search_node.state, hn0);
		open.push(hn0);

		// Create threads after initial node is pushed
		for (size_t i = 1; i < num_threads; i++){
			threads.emplace_back(&CAFE::thread_speculate, this, std::ref(d), i-1,  stop_source.get_token(), std::ref(node_pools[i]), std::ref(wmdat[i-1]));
		}

		if (DEBUG){
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed_seconds = end - start;
			std::cerr << "Time to Initialize Threads: " << elapsed_seconds.count() << std::endl;
			std::cerr << (num_threads-1) << " threads initialized. Beginning Algorithm." << std::endl;
		}

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {	
			HeapNode<Node, NodeComp>* hn = open.get(0);
			open.pop();
			Node* n = &(hn->search_node);
			State buf, &state = d.unpack(buf, n->state);

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				stop_source.request_stop();
				// for (auto& thread : threads){
				// 	thread.join();
				// 	std::cerr << "joined!";
				// }
				WorkerMetadata total_mdat;
				total_mdat.heap_top_fetch.resize(TOP_QUEUE_SIZE);
				total_mdat.recent_push_fetch.resize(RECENT_QUEUE_SIZE);
				for (std::size_t i = 1; i < num_threads; i++){
					std::cerr << "Worker " << i << ": ";
					std::cerr << wmdat[i-1].total() << "\n";
					total_mdat.add(wmdat[i-1]);
				}
				std::cerr << "Spec exp dist: " << total_mdat;
				std::cerr << "Total spec exp: " << total_mdat.total() << "\n";
				std::cerr << "Expanded precomputed node source:\n" << prec_expanded_source;

				if(DEBUG){
					std::cerr << "\n";
					std::cerr << "Total Nodes Observed: " << total_nodes_observed << std::endl;
					std::cerr << "Open List Size: " << open.get_size() << std::endl;
					std::cerr << std::endl;
					std::cerr << "Speculated Nodes Expanded: " << speculated_nodes_expanded << std::endl;
					std::cerr << "Manual Expansions: " << manual_expansions << std::endl;
					std::cerr << "Time Spent Yielding: " << time_spent_yielding << " sec" << std::endl;
					std::cerr << "Total Sum: " << total_sum << std::endl;
					std::cerr << std::endl;
					std::cerr << "Average Expansion Time: " << average_expansion_time << " sec" << std::endl;
					std::cerr << std::endl;
					std::cerr << "Average Thread Fetch Time: " << average_thread_fetch_time << " sec" << std::endl;
					std::cerr << "CPU Time Spent Fetching: " << average_thread_fetch_time * total_nodes_speculated << " sec" << std::endl;
					std::cerr << std::endl;
					std::cerr << "Total Speculated Nodes: " << total_nodes_speculated << std::endl;
					std::cerr << "Thread Misses: " << thread_misses << std::endl;
					std::cerr << "Speculations Collided: " << speculations_collided << std::endl;
					std::cerr << "Failed Reserves: " << failed_reserves << std::endl;
					std::cerr << std::endl;
					// std::cerr << "Expansion Rate: " << SearchAlgorithm<D>::res.expd / elapsed_seconds_algo.count() << "/sec" << std::endl;
					// std::cerr << "Observe Rate: " << total_nodes_observed / elapsed_seconds_algo.count() << "/sec" << std::endl;
					std::cerr << std::endl;

					// print histogram of node delays
					// std::cerr << "Node Delays: " << std::endl;
					// // bucket the delays in groups of 1000 (doesn't mean there are only 1000 buckets)
					// // get size and divide by 1000 to get count of buckets
					// size_t bucket_size = 1;
					// size_t num_buckets = (OPEN_LIST_SIZE / bucket_size) + 1;
					// std::vector<size_t> buckets = std::vector<size_t>(num_buckets, 0);
					// for(size_t i = 0; i < OPEN_LIST_SIZE; i++){
					// 	if (node_delays[i] == 0)
					// 		continue;
					// 	buckets[node_delays[i] / bucket_size]++;
					// }
					// for(size_t i = 0; i < num_buckets; i++){
					// 	if (buckets[i] != 0)
					// 		std::cerr << i * bucket_size << " - " << (i+1) * bucket_size << ": " << buckets[i] << std::endl;
					// }

					// std::cerr << "Speculated Node Delays: " << std::endl;
					// // bucket the delays in groups of 1000 (doesn't mean there are only 1000 buckets)
					// // get size and divide by 1000 to get count of buckets
					// bucket_size = 1;
					// num_buckets = (OPEN_LIST_SIZE / bucket_size) + 1;
					// buckets = std::vector<size_t>(num_buckets, 0);
					// for(size_t i = 0; i < OPEN_LIST_SIZE; i++){
					// 	if (speculated_node_delays[i] == 0)
					// 		continue;
					// 	buckets[speculated_node_delays[i] / bucket_size]++;
					// }
					// for(size_t i = 0; i < num_buckets; i++){
					// 	if (buckets[i] != 0)
					// 		std::cerr << i * bucket_size << " - " << (i+1) * bucket_size << ": " << buckets[i] << std::endl;
					// }
				}
				break;
			}
			SearchAlgorithm<D>::res.expd++;
			std::pair<HeapNode<Node, NodeComp> *, std::size_t> successor_ret;

			//bool wasSpec;

			if(hn->is_completed()){
				// std::cerr << "Speculated Node Expanded" << std::endl;
				prec_expanded_source.add(hn->ee);
				speculated_nodes_expanded++;
				successor_ret = hn->get_successors();
				//wasSpec = true;
			}else{
				// std::cerr << "Manual Expansion" << std::endl;
				manual_expansions++;
				successor_ret = expand(d, hn, nodes, state);
				waste_time(extra_calcs);
			}

			// if(hn->reserve()){
			// 	manual_expansions++;
			// 	successor_ret = expand(d, hn, nodes, state);
			// }else{
			// 	while(!hn->is_completed()){
			// 		std::this_thread::yield();
			// 	}
			// 	speculated_nodes_expanded++;
			// 	successor_ret = hn->get_successors();
			// 	wasSpec = true;
			// }

			HeapNode<Node, NodeComp>* successors = successor_ret.first;
			size_t n_precomputed_successors = successor_ret.second;
			open.batch_recent_push(successors, n_precomputed_successors);

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
					closed[kid->state] = successor;
				}
				else{
					open.push(successor); // add to open list
					closed[kid->state] = successor; // add to closed list
				}

				// if (wasSpec){
				// 	// add to speculated node delays
				// 	speculated_node_delays[SearchAlgorithm<D>::res.expd - kid->nodes_expanded_at_time_of_expansion]++;
				// }
				// else{
				// 	// add to node delays
				// 	if(kid->parent == nullptr){
				// 		node_delays[0]++;
				// 	}
				// 	else{
				// 		node_delays[SearchAlgorithm<D>::res.expd - kid->nodes_expanded_at_time_of_expansion]++;
				// 	}
				// }
			}
			// long double sum = 0;
			// for(size_t i = 0; i < 10; i++){
			// 	sum = sin(sum + rand());
			// }
			// total_sum += sum;
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
		dfpair(stdout, "open list type", "%s", "closed");
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:
	size_t num_threads;
	std::atomic<size_t> total_nodes_observed = 0;
	std::atomic<size_t> total_nodes_speculated = 0;

	std::atomic<size_t> speculations_collided = 0;
	std::atomic<size_t> failed_reserves = 0;

	std::atomic<double> average_thread_fetch_time = 0; // including finding a node
	std::atomic<size_t> thread_misses = 0;
	
	std::atomic<double> average_expansion_time = 0;
	long double time_spent_yielding = 0;

	long double total_sum = 0;

	size_t extra_calcs = 0;

	std::vector<WorkerMetadata> wmdat;

	// make atomic vector of size 1 million all set to 0
	std::vector<size_t> node_delays = std::vector<size_t>(OPEN_LIST_SIZE, 0);
	std::vector<size_t> speculated_node_delays = std::vector<size_t>(OPEN_LIST_SIZE, 0);

	

	std::vector<NodePool<Node, NodeComp>> node_pools;
	std::pair<HeapNode<Node, NodeComp> *, std::size_t> expand(D& d, HeapNode<Node, NodeComp> * hn, NodePool<Node, NodeComp> &nodes, State& state) {
		// auto start = std::chrono::high_resolution_clock::now();
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
			// kid->nodes_expanded_at_time_of_expansion = SearchAlgorithm<D>::res.expd;
			// std::cerr << kid->nodes_expanded_at_time_of_expansion - n->nodes_expanded_at_time_of_expansion << std::endl;
			// node_delays[kid->nodes_expanded_at_time_of_expansion - n->nodes_expanded_at_time_of_expansion]++;
			total_nodes_observed++; // generated
		}
		
		// long double sum = 0;
		// for(size_t i = 0; i < extra_calcs; i++){
		// 	sum = sin(sum + rand());
		// }
		// total_sum += sum;

		// auto end = std::chrono::high_resolution_clock::now();
		// std::chrono::duration<double> elapsed_seconds = end - start;
		// average_expansion_time = (average_expansion_time * total_nodes_observed + elapsed_seconds.count()) / (total_nodes_observed + 1);

		return std::make_pair(successors, successor_count);
	}

	HeapNode<Node, NodeComp> * init(D &d, NodePool<Node, NodeComp> &nodes, State &s0) {
		HeapNode<Node, NodeComp> * hn0 = nodes.reserve(1);
		hn0->zero();
		//hn0->mark_fresh();
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
