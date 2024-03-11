// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once
#include <cstring>
#include <vector>
#include "tiles.hpp"
#include "packed.hpp"

class TilesMdist : public Tiles {
public:

	typedef PackedTiles<Ntiles> PackedState;

	struct State {
		bool operator==(const State &o) const {
			if (b != o.b)
				return false;
			for (unsigned int i = 0; i < Ntiles; i++) {
				if (ts[i] != o.ts[i] && i != b)
					return false;
			}
			return true;
		}
	private:
		friend class TilesMdist;
		Tile ts[Ntiles] = {};
		Pos b;
		Cost h;
		int d;
	};

	TilesMdist(FILE*, const char*);

	State initialstate();

	Cost h(State &s) const { return s.h; }

	Cost d(State &s) const { return s.d; }

	bool isgoal(State &s) const {
	  for(unsigned int t = 1; t < Ntiles; t++)
		if(s.ts[t] != t)
		  return false;
	  s.h = 0;
	  s.d = 0;
	  return true;
	}

	struct Operators {
		Operators(TilesMdist& d, State &s) :
			n(d.ops[s.b].n), mvs(d.ops[s.b].mvs) { }

		unsigned int size() const {
			return n;
		}

		Oper operator[](unsigned int n) const {
			return mvs[n];
		}

	private:
		unsigned int n;
		const Pos *mvs;
	};

	std::pair<Oper, Cost> ident(const State &) const {
		return std::make_pair(Ident, 1);
	}

	struct Edge {
		Cost cost;
		Oper revop;
		Cost revcost;
		State &state;

		Edge(TilesMdist &d, State &s, Oper op) :
		  revop(s.b), state(s), oldh(s.h), oldd(s.d) {

			if (op == Ident) {
				revop = Ident;
				cost = 0;
				revcost = 0;
				return;
			}

			Tile t = state.ts[op];
			cost = d.costs[t];
			revcost = d.costs[t];
			state.ts[state.b] = t;
			state.h += d.costs[t] * d.incr[t][op][state.b];
			state.d += d.incr[t][op][state.b];
			state.b = op;
			s.ts[s.b] = 0;
		}

		~Edge() {
			if (revop == Ident)
				return;
			state.ts[state.b] = state.ts[revop];
			state.b = revop;
			state.h = oldh;
			state.d = oldd;
		}

	private:
		friend class TilesMdist;
		Cost oldh;
		int oldd;
	};

	void pack(PackedState &dst, State &s) const {
		s.ts[s.b] = 0;
		dst.pack(s.ts);
	}

	State &unpack(State &buf, PackedState &pkd) const {
	  buf.b = pkd.unpack_md(md, costs, buf.ts, &buf.h, &buf.d);
		return buf;
	}

	// void dumpstate(FILE *out, State &s) const {
	// 	s.ts[s.b] = 0;
	// 	Tiles::dumptiles(out, s.ts);
	// }

	void dumpstate(FILE *out, State &s) const {
		s.ts[s.b] = 0;
		PackedState pst;
		pack(pst, s);
		fprintf(out, "%lu", pst.hash(this));
	}
  

	Cost pathcost(const std::vector<State>&, const std::vector<Oper>&);

protected:

	unsigned int md[Ntiles][Ntiles];
	Cost costs[Ntiles];
	Cost minCost;

private:
	void initmd();
	void initincr();
	void initcosts(const char*);

	int incr[Ntiles][Ntiles][Ntiles];
};
