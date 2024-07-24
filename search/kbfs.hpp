// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include "../search/search.hpp"
#include "../utils/pool.hpp"

#include <barrier> // for worker synchronization
#include <stop_token> // for stopping the threads

#define THREAD_COUNT 2

std::barrier start_barrier{THREAD_COUNT+1};
std::barrier complete_barrier{THREAD_COUNT+1};


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
		nodes = new Pool<Node>();
	}

	~KBFS() {
		delete nodes;
	}

	void worker(D &d, size_t id, Node** top_nodes, std::vector<Node*>* expanded_nodes, std::stop_token token){
		while(!token.stop_requested()){
			std::cerr << "Thread " << id << " waiting for start" << std::endl;
			start_barrier.arrive_and_wait();
			if(token.stop_requested()) break;

			// check topNodes[id] for the node* to expand
			Node* n = top_nodes[id];
			if(n == nullptr){
				std::cout << "Thread " << id << " has no node to expand." << std::endl;
				// no node to expand in top_nodes[id]
				expanded_nodes[id] = std::vector<Node*>();
				complete_barrier.arrive_and_wait();
				continue;
			}
			State buf, &state = d.unpack(buf, n->state);
			auto successors = expand(d, n, state);
			// should be a vector of nodes
			expanded_nodes[id] = successors;
			
			std::cerr << "Thread " << id << " waiting to complete" << std::endl;
			complete_barrier.arrive_and_wait();
		}
		start_barrier.arrive_and_drop();
		std::cerr << "Thread " << id << " stopped" << std::endl;
	}

	void search(D &d, typename D::State &s0) {
		//kpbfs main thread
		const size_t k = THREAD_COUNT;

		this->start();
		// closed.init(d);

		Node *n0 = init(d, s0);
		closed.emplace(n0->state, n0);
		open.push(n0);

		bool found = false;

		std::vector<std::jthread> threads;
		threads.reserve(k); //idk why but ref said this so

		std::stop_source stop_source;

		Node** top_nodes = new Node*[k];
		std::vector<Node*>* expanded_nodes = new std::vector<Node*>[k];

		// +1 for the main thread to signal to them/wait for them
		
		for (size_t i = 0; i < k; i++){
			stop_token st = stop_source.get_token();
			threads.emplace_back(&KBFS::worker, this, std::ref(d), i, std::ref(top_nodes), std::ref(expanded_nodes), st);
		}

		std::cout << "Threads created" << std::endl;

		size_t loops = 0;

		while (!open.empty() && !SearchAlgorithm<D>::limit() && !found) {
			// get k top nodes on open list (less than k if open list is smaller)
			for (size_t i = 0; i < k; i++) {
				if (open.empty()) {
					break;
				}
				top_nodes[i] = open.pop();
			}
			start_barrier.arrive_and_wait(); // Starts the threads on expansion
			if (loops %10000 == 0){
				std::cout << "homies should be chuggin: " << loops << std::endl;
			}
			loops++;

			std::cout << "Main: Waiting for threads to complete" << std::endl;
			complete_barrier.arrive_and_wait(); // Waits for all the threads to complete
			std::cout << "Main: Threads completed" << std::endl;



			// add all to open
			for (size_t i = 0; i < k; i++) {
				std::vector<Node*> exp_nodes = expanded_nodes[i];
				SearchAlgorithm<D>::res.expd++;
				for (Node* kid : exp_nodes){
					State buf, &state = d.unpack(buf, kid->state);
					if (d.isgoal(state)) {
						solpath<D, Node>(d, kid, this->res);
						stop_source.request_stop();
						std::cerr << "Main: Request Stop" << std::endl;
						// start barrier to stop the threads
						start_barrier.arrive_and_drop();
						for (auto& thread : threads){
							thread.join();
						}
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
		delete[] top_nodes;
		delete[] expanded_nodes;
		
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

	OpenList<Node, Node, Cost> open;
	boost::unordered_flat_map<PackedState, Node*, StateHasher, StateEq> closed;
	Pool<Node> *nodes;
};
