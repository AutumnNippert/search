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
		nodes = new std::unique_ptr<std::atomic<std::shared_ptr<Pool<Node>>>>(new std::atomic<std::shared_ptr<Pool<Node>>>());
		(*nodes)->store(std::make_shared<Pool<Node>>());
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-threads") == 0){
				num_threads = strtod(argv[++i], NULL);
				if(num_threads <= 0 ) {
					exit(1);
				}
			}
		}
	}

	~SPAstar() {
		delete nodes;
	}

	void threadloop(std::atomic<bool> &found, D &d, size_t id, std::barrier<> &barrier) {
		std::cerr << "Thread " << id << " started" << std::endl;
		// wait for all threads to start
		barrier.arrive_and_wait();
		num_working_threads.fetch_add(1);
		while (!(found.load()) && num_working_threads.load() >= 1) { // 1 thread because self
			std::unique_lock<std::mutex> open_lock(open_mutex);
			auto open_ptr = open.load();
			Node *n = open_ptr->pop();
			open_lock.unlock();
			// if failed to get a node, then continue
			if (n == NULL) {
				std::cerr << "Thread " << id << " failed to get a node" << std::endl;
				continue;
			}
			State buf, &state = d.unpack(buf, n->state);
			if (d.isgoal(state)) {
				std::cerr << "Thread " << id << " found a solution" << std::endl;
				found.store(true);
				// solpath<D, Node>(d, n, this->res);
				// set goal to the best solution
				auto goal_ptr = goal.load();
				if (goal_ptr == nullptr || n->g < goal_ptr->g) {
					goal.store(std::make_shared<Node>(*n));
				}
				break;
			}
			expand(d, n, state);
		}
		std::cerr << "Thread " << id << " finished" << std::endl;
		num_working_threads.fetch_sub(1);
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		//closed.init(d);

		// initialize atomic open and closed lists
		open.store(std::make_shared<OpenList<Node, Node, Cost>>());
		closed.store(std::make_shared<boost::unordered_flat_map<PackedState, Node *, StateHasher, StateEq>>());

		auto found = std::atomic<bool>(false);
		// init barrier to size of threads
		std::barrier<> barrier(num_threads); //yeah idk
		
		for (size_t i = 0; i < num_threads; i++){
			threads.emplace_back(&SPAstar::threadloop, this, std::ref(found), std::ref(d), i, std::ref(barrier));
		}

		auto open_ptr = open.load();
		auto closed_ptr = closed.load();

		Node * n0 = init(d, s0);
		closed_ptr->emplace(n0->state, n0);
		open_ptr->push(n0);

		// wait till all threads are done
		for (auto &thread : threads) {
			thread.join();
		}

		std::cerr << "All threads are done" << std::endl;

		auto goal_ptr = goal.load();
		if (goal_ptr != nullptr) {
			solpath<D, Node>(d, goal_ptr.get(), this->res);
		}
		this->finish();
	}

	virtual void reset() {
		auto open_ptr = open.load();
		auto closed_ptr = closed.load();
		auto nodes_ptr = (*nodes)->load();
		SearchAlgorithm<D>::reset();
		open_ptr->clear();
		closed_ptr->clear();
		// delete nodes_ptr;
		// nodes_ptr = new std::atomic<std::shared_ptr<Pool<Node>>>(); // cursed line
	}

	virtual void output(FILE *out) {
		auto open_ptr = open.load();
		SearchAlgorithm<D>::output(out);
		//closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open_ptr->kind());
		dfpair(stdout, "node size", "%u", sizeof(Node));
	}

private:

	void expand(D &d, Node *n, State &state) {
		SearchAlgorithm<D>::res.expd++;

		typename D::Operators ops(d, state);
		for (unsigned int i = 0; i < ops.size(); i++) {
			if (ops[i] == n->pop)
				continue;
			SearchAlgorithm<D>::res.gend++;

			std::unique_lock<std::mutex> nodes_lock(nodes_mutex);
			auto nodes_ptr = (*nodes)->load();
			Node *kid = nodes_ptr->construct();
			nodes_lock.unlock();

			assert (kid);
                        Oper op = ops[i];
			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);

			// unsigned long hash = kid->state.hash(&d);
			//lock open and closed
			std::lock_guard<std::mutex> open_lock(open_mutex);
			std::lock_guard<std::mutex> closed_lock(closed_mutex);
			auto open_ptr = open.load();
			auto closed_ptr = closed.load();
			auto dupl = closed_ptr->find(kid->state);
			if (dupl != closed_ptr->end()) {
				Node * dup = dupl->second;
				this->res.dups++;
				if (kid->g >= dup->g) {
					std::unique_lock<std::mutex> nodes_lock(nodes_mutex);
					nodes_ptr->destruct(kid);
					nodes_lock.unlock();
					continue;
				}
				bool isopen = open_ptr->mem(dup);
				if (isopen)
					open_ptr->pre_update(dup);
				dup->f = dup->f - dup->g + kid->g;
				dup->g = kid->g;
				dup->parent = n;
				dup->op = op;
				dup->pop = e.revop;
				if (isopen) {
					open_ptr->post_update(dup);
				} else {
					this->res.reopnd++;
					open_ptr->push(dup);
				}
				std::unique_lock<std::mutex> nodes_lock(nodes_mutex);
				nodes_ptr->destruct(kid);
				nodes_lock.unlock();
				continue;
			}

			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;

			// load them again
			closed_ptr = closed.load();
			closed_ptr->emplace(kid->state, kid);

			open_ptr = open.load();
			open_ptr->push(kid);
		}
	}

	Node *init(D &d, State &s0) {
		auto nodes_ptr = (*nodes)->load();
		Node * n0 = nodes_ptr->construct();
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

	// mutexes
	std::mutex open_mutex;
	std::mutex closed_mutex;
	std::mutex nodes_mutex;

	std::atomic<std::shared_ptr<OpenList<Node, Node, Cost>>> open;
 	// ClosedList<Node, Node, D> closed;
	std::atomic<std::shared_ptr<boost::unordered_flat_map<PackedState, Node *, StateHasher, StateEq>>> closed;
	std::unique_ptr<std::atomic<std::shared_ptr<Pool<Node>>>> *nodes;
};
