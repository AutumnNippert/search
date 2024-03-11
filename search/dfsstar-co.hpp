// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#include <cstdio>
#include <algorithm>
#include "../search/search.hpp"

void dfrowhdr(FILE*, const char*, unsigned int, ...);
void dfrow(FILE*, const char*, const char*, ...);

template <class D> class DFSstarCO : public SearchAlgorithm<D> {

public:

	typedef typename D::State State;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;

	DFSstarCO(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv) { }

	void search(D &d, State &s0) {
		this->start();
		numsols = 0;
		bound = d.h(s0);
		dfrowhdr(stdout, "iter", 4, "no", "bound",
			"expd", "gend");

		i = 0;
		while (!dfs(d, s0, D::Nop, Cost(0)) && !SearchAlgorithm<D>::limit()) {

			dfrow(stdout, "iter", "dguu", (long) i, (double) bound,
				SearchAlgorithm<D>::res.expd, SearchAlgorithm<D>::res.gend); 

			bound = (Cost)(bound * 2);
			i++;
		}

		this->finish();
	}

private:

	struct Action {
		Cost f;
		Cost g;
		Oper op;
		Oper revop;
		State state;
		Action(Cost f, Cost g, Oper op, Oper revop, State s) : f(f), g(g),
															   op(op),
															   revop(revop),
															   state(s) {}
		bool operator<(const Action &o) const {
			if (f == o.f)
				return g > o.g;
			else
				return f < o.f;
		}
	};
  
	bool dfs(D &d, State &s, Oper pop, Cost g) {
		Cost f = g + d.h(s);

		if (f <= bound && d.isgoal(s)) {
			if (numsols == 0) {
				dfrow(stdout, "iter", "dguu", (long) i, (double) bound,
					  SearchAlgorithm<D>::res.expd,
					  SearchAlgorithm<D>::res.gend); 
				rowhdr();
			}

			if(numsols == 0 || f < bound) {
				numsols++;
				row(numsols, f);
				bound = f;
			}
			
			this->res.path.clear();
			this->res.ops.clear();
			this->res.path.push_back(s);
			return true;
		}

		if (f > bound) {
			return false;
		}

		this->res.expd++;

		typename D::Operators ops(d, s);
		bool solved = false;
		std::vector<Action> actions;
		for (unsigned int n = 0; n < ops.size(); n++) {
			if (this->limit())
				return false;
			if (ops[n] == pop)
				continue;
			
			this->res.gend++;

			{	// Put the transition in a new scope so that
				// it is destructed before we create the next child.
				typename D::Edge e(d, s, ops[n]);
				Cost child_f = d.h(e.state) + g + e.cost;
				Action action(child_f, g + e.cost, ops[n], e.revop, e.state);
			    actions.push_back(action);
			}
		}

		std::sort(actions.begin(), actions.end());

		for (unsigned int n = 0; n < actions.size(); n++) {
			bool goal = false;
			Action action = actions[n];
			goal = dfs(d, action.state, action.revop, action.g);

			if (goal) {
				this->res.path.push_back(s);
				this->res.ops.push_back(action.op);
				solved = true;
			}
		}

		return solved;
	}

	// rowhdr outputs the incumbent solution row header line.
	void rowhdr() {
		dfrowhdr(stdout, "incumbent", 6, "num", "nodes expanded",
			"nodes generated", "solution bound", "solution cost",
			"wall time");
	}

	// row outputs an incumbent solution row.
	void row(unsigned long n, Cost cost) {
		dfrow(stdout, "incumbent", "uuuggg", n, this->res.expd,
			  this->res.gend, (float)bound, (float)cost,
			walltime() - this->res.wallstart);
	}
	
	Cost bound;
	int i;
	int numsols;
};
