#include "mdlearn.hpp"
#include "../search/idastar.hpp"
#include "../search/astar.hpp"
#include "../incl/utils.hpp"
#include <cstdio>

typedef TilesMDLearn TilesDomain;

static void search(TilesDomain&, Search<TilesDomain>&, TilesDomain::State&);
static Search<TilesDomain> *getsearch(int, char *[]);

int main(int argc, char *argv[]) {
	TilesDomain d(stdin);
	Search<TilesDomain> *srch = getsearch(argc, argv);
	TilesDomain::State s0 = d.initstate();
	search(d, *srch, s0);
	delete srch;
	return 0;
}

static void search(TilesDomain &d, Search<TilesDomain> &srch, TilesDomain::State &s0) {
	dfpair(stdout, "initial heuristic", "%d", d.h(s0));
	Result<TilesDomain> res = srch.search(d, s0);
	res.output(stdout);
	dfprocstatus(stdout);
}

static Search<TilesDomain> *getsearch(int argc, char *argv[]) {
	Search<TilesDomain> *srch = NULL;

	if (argc > 1 && strcmp(argv[1], "idastar") == 0)
		srch = new Idastar<TilesDomain, true>();
	else
		srch = new Astar<TilesDomain>();

	return srch;
}
