// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"

#include <stop_token> // for stopping the threads
#include <mutex>
#include <condition_variable>

enum ThreadState {to_expand, expanded};

template <class D> struct KBFS : public SearchAlgorithm<D> {

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
	
	struct Node {
		int openind;
		Node *parent;
		PackedState state;
		Oper op, pop;
		Cost f, g;

		Node() : openind(-1) {
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
		SearchAlgorithm<D>(argc, argv) {
		num_threads = 1;
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0)
				num_threads = strtod(argv[++i], NULL);
		}
		nodes = new Pool<Node>();
	}

	~KBFS() {
		delete nodes;
	}

	void worker(D &d, size_t i, std::stop_token token){
		while(!token.stop_requested()){
			std::unique_lock<std::mutex> lk(locks[i]);
			worker_start[i].wait(lk, [&]{return thread_state[i] == ThreadState::to_expand;});
			if(token.stop_requested()){
				return;
			}
			children[i].clear();
			// std::cerr << "Thread " << i << " waiting for start" << std::endl;
			// check topNodes[id] for the node* to expand
			Node* n = node_to_expand[i];
			assert(n);
			State buf, &state = d.unpack(buf, n->state);
			// should be a vector of nodes
			children[i] = expand(d, n, state);
			thread_state[i] = ThreadState::expanded;
			lk.unlock();
			worker_done[i].notify_one();
			// std::cerr << "Thread " << i << " waiting to complete" << std::endl;
		}
		// std::cerr << "Thread " << i << " stopped" << std::endl;
	}

	void search(D &d, typename D::State &s0) {
		//kpbfs main thread
		const std::size_t k = num_threads;
		reset_sync();

		this->start();
		// closed.init(d);

		Node *n0 = init(d, s0);
		closed.emplace(n0->state, n0);
		open.push(n0);

		bool found = false;

		std::vector<std::jthread> threads;
		threads.reserve(k); //idk why but ref said this so

		std::stop_source stop_source;


		// +1 for the main thread to signal to them/wait for them
		
		for (std::size_t i = 0; i < k; i++){
			stop_token st = stop_source.get_token();
			threads.emplace_back(&KBFS::worker, this, std::ref(d), i, st);
		}

		// std::cout << "Threads created" << std::endl;

		while (!open.empty() && !SearchAlgorithm<D>::limit() && !found) {
			// get k top nodes on open list (less than k if open list is smaller)
			std::size_t n_threads_dispatched = 0;
			for (std::size_t i = 0; i < k; i++) {
				if (open.empty()) {
					break;
				}
				std::unique_lock<std::mutex> lk(locks[i]);
				node_to_expand[i] = open.pop();
				thread_state[i] = ThreadState::to_expand;
				lk.unlock();
				worker_start[i].notify_one();
				n_threads_dispatched++;
			}
			// if (loops %10000 == 0){
			// 	std::cout << "homies should be chuggin: " << loops << std::endl;
			// }
			// loops++;

			// std::cout << "Main: Waiting for threads to complete" << std::endl;
			// std::cout << "Main: Threads completed" << std::endl;



			// add all to open
			for (size_t i = 0; i < n_threads_dispatched; i++) {
				std::unique_lock<std::mutex> lk(locks[i]);
				worker_done[i].wait(lk, [&]{return thread_state[i] == ThreadState::expanded;});

				std::vector<Node*>& exp_nodes = children[i];
				SearchAlgorithm<D>::res.expd++;
				for (Node* kid : exp_nodes){
					State buf, &state = d.unpack(buf, kid->state);
					if (d.isgoal(state)) {
						solpath<D, Node>(d, kid, this->res);
						// start barrier to stop the threads
						found = true;
						break;
					}
					SearchAlgorithm<D>::res.gend++;

					auto dup_it = closed.find(kid->state);
					if (dup_it != closed.end()) { // if its in closed, check if dup is better
						Node *dup = dup_it->second;
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
						dup->parent = kid;
						dup->op = kid->op;
						dup->pop = kid->pop; // TODO: This might not be correct
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
					closed.emplace(kid->state, kid);
					open.push(kid);
				}
				if(found) break;
			}
			if(found) break;
		}
		stop_source.request_stop();
		// std::cerr << "Main: Request Stop" << std::endl;
		for (std::size_t i = 0; i < k; i++){
			std::unique_lock<std::mutex> lk(locks[i]);
			thread_state[i] = ThreadState::to_expand;
			lk.unlock();
			worker_start[i].notify_one();
		}
		// std::cerr << "threads stopped\n";
		this->finish();
		
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		// closed.clear();
		delete nodes;
		nodes = new Pool<Node>();
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		// closed.prstats(stdout, "closed ");
		// dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "open list type", "%s", "boost::unordered_flat_map");
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	std::vector<Node*> expand(D &d, Node *n, State &state) {
		std::vector<Node*> children;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;

			Node *kid = nodes->construct();
			assert (kid);
            Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;
			children.push_back(kid);
		}
		return children;
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

	inline void reset_sync(){
		worker_start = std::vector<std::condition_variable>(num_threads);
		worker_done = std::vector<std::condition_variable>(num_threads);
		locks = std::vector<std::mutex>(num_threads);
		node_to_expand = std::vector<Node *>(num_threads, nullptr);
		children = std::vector<std::vector<Node *>>(num_threads);
		thread_state = std::vector<ThreadState>(num_threads, ThreadState::expanded);
	}

	OpenList<Node, Node, Cost> open;
	boost::unordered_flat_map<PackedState, Node*, StateHasher, StateEq> closed;
	Pool<Node> * nodes;
	std::size_t num_threads;
	std::vector<std::condition_variable> worker_start;
	std::vector<std::condition_variable> worker_done;
	std::vector<std::mutex> locks;
	std::vector<Node *> node_to_expand;
	std::vector<std::vector<Node *>> children;
	std::vector<ThreadState> thread_state;
};
