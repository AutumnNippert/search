#include "gridmap.hpp"
#include "../visnav/visgraph.hpp"
#include <cstdio>

int main(int argc, char *argv[]) {
	GridMap map(stdin);
	bool *blkd = new bool[map.size()];
	for (unsigned int i = 0; i < map.size(); i++)
		blkd[i] = false;
	for (unsigned int i = 0; i < map.width(); i++) {
	for (unsigned int j = 0; j < map.height(); j++)
		blkd[i * map.height() + j] = !map.passable(map.loc(i, j));
	}

	VisGraph vg(blkd, map.width(), map.height());
	vg.output(stdout);

	return 0;
}