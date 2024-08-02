// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#include "search.hpp"
#include "idastar.hpp"
#include "dfsstar.hpp"
#include "dfsstar-co.hpp"
#include "ildsstar.hpp"
#include "astar.hpp"
#include "astar-dump.hpp"
#include "wastar.hpp"
#include "greedy.hpp"
#include "bugsy.hpp"
#include "bugsy-slim.hpp"
#include "arastar.hpp"
#include "lsslrtastar.hpp"
#include "fhatlrtastar.hpp"
#include "dtastar-dump.hpp"
#include "dtastar.hpp"
#include "beam.hpp"
#include "bead.hpp"
#include "cab.hpp"
#include "trianglebead.hpp"
#include "rectangle.hpp"
#include "hhatgreedy.hpp"
//#include "ees.hpp"
#include "aees.hpp"
#include "ucs.hpp"
#include "naive-cafe.hpp"
#include "cafe.hpp"
#include "astar-basic.hpp"
#include "kbfs.hpp"
#include "spastar.hpp"

#include <cstddef>
#include <cstdio>

// Functions for conveniently defining a new main
// function for a domain.

void dfheader(FILE*);
void dffooter(FILE*);
void dfpair(FILE *, const char *, const char *, ...);
void dfprocstatus(FILE*);
void fatal(const char*, ...);

template<class D> SearchAlgorithm<D> *getsearch(int argc, const char *argv[]);

template<class D> Result<D> search(D &d, int argc, const char *argv[]) {
	return searchGet(getsearch, d, argc, argv);
}

template<class D> Result<D> searchGet(SearchAlgorithm<D>*(*get)(int, const char *[]), D &d, int argc, const char *argv[]) {
	SearchAlgorithm<D> *srch = get(argc, argv);
	if (!srch && argc > 1)
		fatal("Unknow search algorithm: %s", argv[1]);
	if (!srch)
		fatal("Must specify a search algorithm");

	typename D::State s0 = d.initialstate();
	dfpair(stdout, "initial heuristic", "%f", (double) d.h(s0));
	dfpair(stdout, "initial distance", "%f", (double) d.d(s0));
	dfpair(stdout, "algorithm", argv[1]);

	try {
		srch->search(d, s0);
	} catch (std::bad_alloc&) {
		dfpair(stdout, "out of memory", "%s", "true");
		srch->res.path.clear();
		srch->res.ops.clear();
		srch->finish();
	}
	if (srch->res.path.size() > 0) {
		dfpair(stdout, "final sol cost", "%f",
			(double) d.pathcost(srch->res.path, srch->res.ops));
	} else {
		dfpair(stdout, "final sol cost", "%f", -1.0);
	}
	srch->output(stdout);
	std::cerr << "printed\n";
	Result<D> res = srch->res;
	std::cerr << "deleting\n";
	delete srch;
	std::cerr << "deleted\n";
	return res;
}

template<class D> SearchAlgorithm<D> *getsearch(int argc, const char *argv[]) {
	if (argc < 2)
		fatal("No algorithm specified");

	if (strcmp(argv[1], "idastar") == 0)
		return new Idastar<D>(argc, argv);
	if (strcmp(argv[1], "dfsstar") == 0)
		return new DFSstar<D>(argc, argv);
	if (strcmp(argv[1], "dfsstar-co") == 0)
		return new DFSstarCO<D>(argc, argv);
	if (strcmp(argv[1], "ildsstar") == 0)
		return new ILDSstar<D>(argc, argv);
	else if (strcmp(argv[1], "astar") == 0)
		return new Astar<D>(argc, argv);
	else if (strcmp(argv[1], "astar-basic") == 0)
		return new AstarBasic<D>(argc, argv);
	else if (strcmp(argv[1], "astar-dump") == 0)
		return new Astar_dump<D>(argc, argv);
	else if (strcmp(argv[1], "wastar") == 0)
		return new Wastar<D>(argc, argv);
	else if (strcmp(argv[1], "greedy") == 0)
		return new Greedy<D>(argc, argv);
	else if (strcmp(argv[1], "speedy") == 0)
		return new Greedy<D, true>(argc, argv);
	else if (strcmp(argv[1], "bugsy") == 0)
		return new Bugsy<D>(argc, argv);
	else if (strcmp(argv[1], "bugsy-slim") == 0)
		return new Bugsy_slim<D>(argc, argv);
	else if (strcmp(argv[1], "arastar") == 0)
		return new Arastar<D>(argc, argv);
	else if (strcmp(argv[1], "arastarmon") == 0)
		return new ArastarMon<D>(argc, argv);
	else if (strcmp(argv[1], "arastarnora") == 0)
		return new ArastarNORA<D>(argc, argv);
	else if (strcmp(argv[1], "dtastar-dump") == 0)
		return new Dtastar_dump<D>(argc, argv);
	else if (strcmp(argv[1], "lsslrtastar2") == 0)
		return new Lsslrtastar2<D>(argc, argv);
	else if (strcmp(argv[1], "fhatlrtastar") == 0)
		return new Fhatlrtastar<D>(argc, argv);
	else if (strcmp(argv[1], "dtastar") == 0)
		return new Dtastar<D>(argc, argv);
	else if (strcmp(argv[1], "beam") == 0)
		return new BeamSearch<D>(argc, argv);
	else if (strcmp(argv[1], "bead") == 0)
		return new BeadSearch<D>(argc, argv);
	else if (strcmp(argv[1], "cab") == 0)
		return new CABSearch<D>(argc, argv);
	else if (strcmp(argv[1], "triangle") == 0)
		return new TriangleBeadSearch<D>(argc, argv);
	else if (strcmp(argv[1], "rectangle") == 0)
		return new RectangleBeadSearch<D>(argc, argv);
	else if (strcmp(argv[1], "hhatgreedy") == 0)
		return new Hhatgreedy<D>(argc, argv);
	//else if (strcmp(argv[1], "ees") == 0)
	//	return new EES<D>(argc, argv);
	else if (strcmp(argv[1], "aees") == 0)
		return new AnytimeEES<D>(argc, argv);
	else if (strcmp(argv[1], "ucs") == 0)
		return new UniformCost<D>(argc, argv);
	else if (strcmp(argv[1], "naive-cafe") == 0)
		return new Naive_CAFE<D>(argc, argv);
	else if (strcmp(argv[1], "cafe") == 0)
		return new CAFE<D>(argc, argv);
	else if (strcmp(argv[1], "kbfs") == 0)
		return new KBFS<D>(argc, argv);
	else if (strcmp(argv[1], "spastar") == 0)
		return new SPAstar<D>(argc, argv);

	fatal("Unknown algorithm: %s", argv[1]);
	return NULL;	// Unreachable
}
