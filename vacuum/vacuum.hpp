// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once

#include "../utils/utils.hpp"
#include "../gridnav/gridmap.hpp"
#include "../structs/binheap.hpp"
#include <memory>
#include <cstring>
#include <cstdio>

class Vacuum {
public:
	typedef int Cost;

	typedef int Oper;	// directions 0, 1, 2, 3
	static const Oper Suck = 4;
	static const Oper Charge = 5;
	static const Oper Nop = -1;

	Vacuum(FILE*, const char *);

	class State {
	public:
		State() : loc(-1), energy(-1), ndirt(-1) {
		}

		bool eq(const Vacuum*, const State &o) const {
			if (loc != o.loc || ndirt != o.ndirt)
				return false;

			for (unsigned int i = 0; i < dirt->size(); i++) {
				if (dirt->at(i) != o.dirt->at(i))
					return false;
			}

			return true;
		}

		unsigned long hash(const Vacuum*) const {
			return loc*dirt->size() + ndirt;
		}

		int loc, energy, ndirt, weight;
		std::shared_ptr<std::vector<bool> > dirt;
		Cost dirt_dist;
	};

	typedef State PackedState;

	State initialstate(void) const;

	Cost md(const std::pair<int, int> a, const std::pair<int, int> b) const {
		int ax = a.first;
		int ay = a.second;

		int bx = b.first;
		int by = b.second;

		return abs(ax - bx) + abs(ay - by);
	}

	struct MSTNode {
		int openind;
		int dist;
		std::pair<int, int> dirtloc;
		int ind;

		MSTNode(int dst, std::pair<int, int> dl, int i) :
		  openind(-1), dist(dst), dirtloc(dl), ind(i) {
		}

		static void setind(MSTNode *n, int i) {
			n->openind = i;
		}
	  
		static bool pred(MSTNode *a, MSTNode *b) {
			return a->dist < b->dist;
		}
	  
	};

	Cost getMST(const State &s) const {
		int sum = 0;

		if (s.ndirt == 0) {
			return 0;
		}
		
		BinHeap<MSTNode, MSTNode *> heap;

		int dists[s.dirt->size()];
		
		unsigned int i;
		for (i = 0; i < s.dirt->size() && !s.dirt->at(i); i++) {
			dists[i] = 0;
		}

		std::pair<int, int> root = dirtLocs[i];
		dists[i] = 0;

		for (i++; i < s.dirt->size(); i++) {
			if (!s.dirt->at(i)) {
				dists[i] = 0;
				continue;
			}

			MSTNode *n = new MSTNode(md(root, dirtLocs[i]), dirtLocs[i], i);
			heap.push(n);
			dists[i] = n->dist;
		}

		while(!heap.empty()) {
			MSTNode *n = *heap.pop();

			for (i = 0; i < heap.size(); i++) {
		    	MSTNode *other = heap.at(i);

				int dist = md(n->dirtloc, other->dirtloc);
				if(dist < other->dist) {
					other->dist = dist;
					heap.update(i);
					dists[other->ind] = dist;
				}
			}
			
			delete n;
		}
		
		for (i = 0; i < s.dirt->size(); i++) {
			sum += dists[i];
		}
		
		return sum;
	}

	Cost h(const State &s) const {
		return s.ndirt + (d(s) - s.ndirt) * s.weight;
	}

	Cost d(const State &s) const {

		if (s.ndirt == 0) {
			return 0;
		}
		
		unsigned int i;
		for (i = 0; i < s.dirt->size() && !s.dirt->at(i); i++)
			;

		std::pair<int, int> agentLoc = map->coord(s.loc);
		int agentDist = md(agentLoc, dirtLocs[i]);

		for (i++; i < s.dirt->size(); i++) {
			if (!s.dirt->at(i))
				continue;
			int d = md(agentLoc, dirtLocs[i]);
			agentDist = std::min(agentDist, d);
		}
		
		assert(s.weight > 0);
		return s.ndirt + (agentDist + s.dirt_dist) * 1.0;
	}

	bool isgoal(const State &s) const {
		return s.ndirt == 0;
	}

	struct Operators {
		Operators(const Vacuum &d, const State &s) : n(0) {
			if (s.energy == 0)
				return;

			int dirt = d.dirt[s.loc];
			if (dirt >= 0 && s.dirt->at(dirt)) {
				ops[n++] = Suck;
				return;
			}

			for (unsigned int i = 0; i < d.map->nmvs; i++) {
				if (d.map->ok(s.loc, d.map->mvs[i]))
					ops[n++] = i;
			}

			if (d.chargers[s.loc])
				ops[n++] = Charge;
		}

		unsigned int size() const {
			return n;
		}

		Oper operator[] (unsigned int i) const {
			return ops[i];
		}

	private:
		unsigned int n;
		Oper ops[6];	// Up, Down, Left, Right, Suck, Charge
	};

	struct Edge {
		State state;
		Cost cost;
		Oper revop;
		Cost revcost;

		Edge(const Vacuum &d, const State &s, Oper op) : state(s), cost(s.weight), revcost(1) {
			if (op == Suck) {
				int dirt = d.dirt[s.loc];

				assert (dirt >= 0);
				assert ((unsigned int) dirt < d.ndirt());
				assert (state.dirt->at(dirt));

				state.dirt = std::make_shared<std::vector<bool>>(s.dirt->begin(), s.dirt->end());
				state.dirt->at(dirt) = false;
				state.ndirt--;
			    state.weight += d.cost_mod;
				cost = 1;

				revop = Nop;
				revcost = Cost(-1);

				state.dirt_dist = d.getMST(state);

			} else if (op == Charge) {
				fatal("Charge operator!");

			} else {
				assert (op >= 0);
				assert (op <= 3);
				revop = d.rev[op];
				state.loc += d.map->mvs[op].delta;
			}
		}

		~Edge(void) { }
	};

	void pack(PackedState &dst, State &src) const {
		dst = src;
	}

	State &unpack(State &buf, PackedState &pkd) const {
		return pkd;
	}

	void dumpstate(FILE *out, const State &s) const {
		// auto pt = map->coord(s.loc);
		// fprintf(out, "(%d, %d), energy=%d, ndirt=%d", pt.first, pt.second, s.energy, s.ndirt);
		// for (unsigned int i = 0; i < s.dirt->size(); i++)
		// 	fprintf(out, " %d", (int) s.dirt->at(i));
		
	    fprintf(out, "%d %d %d ", s.loc, s.energy, s.ndirt);
		
	    for (unsigned int i = 0; i < s.dirt->size(); i++)
		 	fprintf(out, "%d", (int) s.dirt->at(i));
	}

	Cost pathcost(const std::vector<State>&, const std::vector<Oper>&);

	void printpath(FILE*, const std::vector<Oper>&) const;

protected:
	int orig_dirt;
	Cost cost_mod;

private:

	unsigned int ndirt() const {
		return dirtLocs.size();
	}

	// reverseops computes the reverse operators
	void reverseops();

	// rev holds the index of the reverse of each operator
	int rev[8];

	GridMap *map;
	int x0, y0;

	std::vector<int> dirt;	// Indexed by map location, gives dirt ID.
	std::vector<std::pair<int, int> > dirtLocs;
	std::vector<bool> chargers;
};
