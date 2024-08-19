// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include <boost/unordered/unordered_flat_map.hpp>
#include "../search/search.hpp"
#include "../utils/pool.hpp"

#include <thread>
#include <stop_token>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex> // yay pain
#include <barrier>

template <class D> struct SPAstar : public SearchAlgorithm<D> {

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

	SPAstar(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv) {
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0){
				num_threads = strtod(argv[++i], NULL);
				if(num_threads <= 0 ) {
					exit(1);
				}
				if (strcmp(argv[i], "-exp") == 0){
					extra_calcs = strtod(argv[++i], NULL);
				}
			}
		}
	}

	~SPAstar() {
	}

	void threadloop(std::atomic<bool> &found, D &d, size_t id, std::barrier<> &barrier, Pool<Node> * nodes) {
		// std::cerr << "Thread " << id << " started" << std::endl;
		// wait for all threads to start
		barrier.arrive_and_wait();
		num_working_threads.fetch_add(1);
		while (!(found.load()) && num_working_threads.load() >= 1) { // 1 thread because self
			std::unique_lock<std::mutex> open_lock(open_mutex);
			Node *n = open.pop();
			open_lock.unlock();
			// if failed to get a node, then continue
			if (n == NULL) {
				// std::cerr << "Thread " << id << " failed to get a node" << std::endl;
				continue;
			}
			State buf, &state = d.unpack(buf, n->state);
			if (d.isgoal(state)) {
				// std::cerr << "Thread " << id << " found a solution" << std::endl;
				found.store(true);
				auto goal_ptr = goal.load();
				if (goal_ptr == nullptr || n->g < goal_ptr->g) {
					goal.store(std::make_shared<Node>(*n));
				}
				break;
			}
			expand(d, n, state, nodes);
		}
		// std::cerr << "Thread " << id << " finished" << std::endl;
		num_working_threads.fetch_sub(1);
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		//closed.init(d);

		auto found = std::atomic<bool>(false);
		// init barrier to size of threads
		std::barrier<> barrier(num_threads);

		std::vector<Pool<Node>* > pools;
		
		for (size_t i = 0; i < num_threads; i++){
			pools.push_back(new Pool<Node>());
			threads.emplace_back(&SPAstar::threadloop, this, std::ref(found), std::ref(d), i, std::ref(barrier), pools[i]);
		}

		Pool<Node> * nodes = new Pool<Node>();
		Node * n0 = init(d, s0, nodes);
		closed[n0->state] =  n0;
		open.push(n0);

		// wait till all threads are done
		for (auto &thread : threads) {
			thread.join();
		}

		// std::cerr << "All threads are done" << std::endl;

		auto goal_ptr = goal.load();
		if (goal_ptr != nullptr) {
			solpath<D, Node>(d, goal_ptr.get(), this->res);
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		closed.clear();
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		//closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open.kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	void expand(D &d, Node *n, State &state, Pool<Node> * nodes) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;
			Node *kid = nodes->construct();

			assert (kid);
                        Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			// unsigned long hash = kid->state.hash(&d);
			//lock open and closed
			std::lock_guard<std::mutex> open_lock(open_mutex);
			std::lock_guard<std::mutex> closed_lock(closed_mutex);
			auto dupl = closed.find(kid->state);
			if (dupl != closed.end()) {
				Node * dup = dupl->second;
				this->res.dups++;
				if (kid->g >= dup->g) {
					nodes->destruct(kid);
					continue;
				}
				bool isopen = open.mem(dup);
				if (isopen)
					open.pre_update(dup);
				dup->f = dup->f - dup->g + kid->g;
				dup->g = kid->g;
				dup->parent = n;
				dup->op = op;
				dup->pop = e.revop;
				if (isopen) {
					open.post_update(dup);
				} else {
					this->res.reopnd++;
					open.push(dup);
				}
				nodes->destruct(kid);
				continue;
			}

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;

			// should be covered by lock guards above
			closed[kid->state] = kid;
			open.push(kid);

		}
	}

	Node *init(D &d, State &s0, Pool<Node> * nodes) {
		Node * n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		return n0;
	}

	size_t num_threads = 1;
	std::atomic<size_t> num_working_threads = 0;
	std::vector<std::jthread> threads;
	std::atomic<std::shared_ptr<Node>> goal;


	size_t extra_calcs = 0;
	std::atomic<double> total_sum = 0;

	// mutexes
	std::mutex open_mutex;
	std::mutex closed_mutex;
	std::mutex nodes_mutex;

	OpenList<Node, Node, Cost> open;
 	// ClosedList<Node, Node, D> closed;
	boost::unordered_flat_map<PackedState, Node *, StateHasher, StateEq> closed;
};
