// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#include <cstdio>
#include "../search/search.hpp"

void dfrowhdr(FILE*, const char*, unsigned int, ...);
void dfrow(FILE*, const char*, const char*, ...);

template <class D> class ILDSstar : public SearchAlgorithm<D> {

public:

	typedef typename D::State State;
	typedef typename D::Cost Cost;
	typedef typename D::Oper Oper;

	ILDSstar(int argc, const char *argv[]) :
		SearchAlgorithm<D>(argc, argv) { }

	void search(D &d, State &s0) {
		this->start();
		numsols = 0;
		depth_bound = d.d(s0);
		dfrowhdr(stdout, "iter", 4, "no", "bound",
			"expd", "gend");

		i = 0;
		bound = std::numeric_limits<double>::infinity();
		min_pruned_k = bound;
		min_pruned_d = bound;
		k_max = 0;
		bool new_iter = false;
		while ((numsols == 0 || min_pruned_k < bound || min_pruned_d < bound)
			   && !SearchAlgorithm<D>::limit()) {

			min_pruned_k = bound;
			min_pruned_d = bound;
			bool solved = ilds(d, s0, D::Nop, Cost(0), k_max, depth_bound);
			
			if (new_iter) {
				dfrow(stdout, "iter", "dduu", (long) i, (long) depth_bound/2,
					  SearchAlgorithm<D>::res.expd, SearchAlgorithm<D>::res.gend);
				new_iter = false;
			}

			if (min_pruned_k >= bound || (solved && min_pruned_d < bound)) {
				depth_bound = depth_bound * 2;
				i++;
				k_max = 0;
				new_iter = true;
			} else {
				k_max++;
			}
		}
		
		dfrow(stdout, "iter", "dduu", (long) i, (long) depth_bound,
			  SearchAlgorithm<D>::res.expd, SearchAlgorithm<D>::res.gend);

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
  
	bool ilds(D &d, State &s, Oper pop, Cost g, unsigned int k, unsigned int depth) {
		Cost f = g + d.h(s);

		if ((unsigned int)d.d(s) > depth) {
			if (f < min_pruned_d) {
			  min_pruned_d = f;
			}
			return false;
		}

		// prune nodes worse than the incumbent
		if (f > bound) {
			return false;
		}

		if (f <= bound && d.isgoal(s)) {
			if (numsols == 0) {
				rowhdr();
			}

			if(numsols == 0 || f < bound) {
				numsols++;
				row(numsols, f, depth_bound, k_max);
				bound = f;
			}
			
			this->res.path.clear();
			this->res.ops.clear();
			this->res.path.push_back(s);
			return true;
		}

		this->res.expd++;

		bool solved = false;

		typename D::Operators ops(d, s);
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
			
			// second child is 1 discrep, third is 2, etc.
			unsigned int disc = n;
			
			if (n == 0 && depth > k) {
				goal = ilds(d, action.state, action.revop, action.g, k, depth-1);
			} else if (k > disc) {
				goal = ilds(d, action.state, action.revop, action.g, k-disc, depth-1);
			} else if (n != 0 && action.f < min_pruned_k) {
				min_pruned_k = action.f;
			}

			if (goal) {
				this->res.path.push_back(s);
				this->res.ops.push_back(action.op);
				solved = true;
				if (min_pruned_d < bound || min_pruned_k < bound)
				  return solved;
			}
		}

		return solved;
	}

	// rowhdr outputs the incumbent solution row header line.
	void rowhdr() {
		dfrowhdr(stdout, "incumbent", 7, "num", "nodes expanded",
			"nodes generated", "depth bound", "discrep bound", "solution cost",
			"wall time");
	}

	// row outputs an incumbent solution row.
  void row(unsigned long n, Cost cost, int depth, int k) {
		dfrow(stdout, "incumbent", "uuuuugg", n, this->res.expd,
			  this->res.gend, depth, k, (float)cost,
			walltime() - this->res.wallstart);
	}
	
	double bound;
	int depth_bound;
	int k_max;
	int i;
	double min_pruned_k;
	double min_pruned_d;
	int numsols;
};
