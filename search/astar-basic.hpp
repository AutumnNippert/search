// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include <boost/unordered/unordered_flat_map.hpp>
#include "../search/search.hpp"
#include "../utils/pool.hpp"


template <class D> struct AstarBasic : public SearchAlgorithm<D> {

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
		ClosedEntry<Node, D> closedent;
		int openind;
		Node *parent;
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

		static Cost prio(Node *n) {
			return n->f;
		}

		static Cost tieprio(Node *n) {
			return n->g;
		}

		inline friend std::ostream& operator<<(std::ostream& stream, const Node& node){
			stream << "[g:" << node.g << ", f:" << node.f << ", op:" << node.op << ", pop:" << node.pop << "]";
			return stream;
		}
	};

	static inline void dump_closed(std::ostream& stream, const boost::unordered_flat_map<PackedState, Node *, StateHasher, StateEq>& c){
		stream << "Closed:\n";
		for (const auto & elem: c){
			stream << *((std::size_t *)(&elem.first)) << ": " << *elem.second << "\n";
		}
		stream << "\n";
	}

	AstarBasic(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv) {
		nodes = new Pool<Node>();
		for (int i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-exp") == 0){
				extra_calcs = strtod(argv[++i], NULL);
			}
		}
	}

	~AstarBasic() {
		delete nodes;
	}

	void search(D &d, typename D::State &s0) {
		this->start();
		//closed.init(d);

		Node * n0 = init(d, s0);
		closed.emplace(n0->state, n0);
		open.push(n0);

		while (!open.empty() && !SearchAlgorithm<D>::limit()) {
			// std::cout << "Open Pull" << std::endl;
			Node *n = open.pop();
			// std::cerr << "pop:"<<*n << "\n";
			// dump_closed(std::cerr, closed);
			// std::cerr << open << std::endl;
			State buf, &state = d.unpack(buf, n->state);
			
			// std::cout << "Popped Node: " << *n << std::endl;

			if (d.isgoal(state)) {
				solpath<D, Node>(d, n, this->res);
				break;
			}

			expand(d, n, state);
		}
		this->finish();
	}

	virtual void reset() {
		SearchAlgorithm<D>::reset();
		open.clear();
		closed.clear();
		delete nodes;
		nodes = new Pool<Node>();
	}

	virtual void output(FILE *out) {
		SearchAlgorithm<D>::output(out);
		//closed.prstats(stdout, "closed ");
		dfpair(stdout, "open list type", "%s", open.kind());
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

			Node *kid = nodes->construct(); // nodes->construct() is a function that returns a new node from the pool?
			assert (kid);
            Oper op = ops[i];

			typename D::Edge e(d, state, op);
			kid->g = n->g + e.cost;
			d.pack(kid->state, e.state);
			// comment out below along with debug
			kid->f = kid->g + d.h(e.state);
			kid->parent = n;
			kid->op = op;
			kid->pop = e.revop;
			// unsigned long hash = kid->state.hash(&d);
			auto dupl = closed.find(kid->state);
			if (dupl != closed.end()) {
				Node * dup = dupl->second;
				this->res.dups++;
				if (kid->g >= dup->g) {
					nodes->destruct(kid);
					continue;
				}
				// std::cerr << "duplicate:" << *kid << " " << *dup << " same state? "<< StateEq()(kid->state, dup->state) << "\n";
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
					// std::cerr << open << std::endl;
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
			// std::cout << "Adding successor " << std::endl;// *kid << std::endl;
			closed.emplace(kid->state, kid);
			open.push(kid);
			// std::cerr << "push:" << *kid << "\n";
			// std::cerr << open << "\n";
		}
		waste_time(extra_calcs);
	}

	Node *init(D &d, State &s0) {
		Node * n0 = nodes->construct();
		d.pack(n0->state, s0);
		n0->g = Cost(0);
		n0->f = d.h(s0);
		n0->pop = n0->op = D::Nop;
		n0->parent = NULL;
		return n0;
	}

	size_t extra_calcs = 0;
	OpenList<Node, Node, Cost> open;
 	// ClosedList<Node, Node, D> closed;
	boost::unordered_flat_map<PackedState, Node *, StateHasher, StateEq> closed;
	Pool<Node> *nodes;
};
