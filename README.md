# Search Research Code Base
Code for researching heuristic search algorithms.

# Building Instructions
You can build with either GCC or clang.  If one or the other doesn't
work then it is a bug so please tell me.

Clang versions less than 3.0 do not seem to work.

## Ubuntu dependencies:
* libboost-dev
* libsdl1.2-dev libsdl-image1.2-dev (Required by plat2d/watch binary and graphics)
* liblapack-dev (Required by search/learnh/lms)

## Arch dependencies
* extra/boost
* extra/sdl2
* extra/sdl_image
* extra/sdl2_image
* extra/lapack 

## Fedora dependencies:
* boost-devel
* lapack-devel
* SDL2-devel (untested)
* SDL2_image-devel (untested)

# Running Instructions
Make sure the algorithm you are trying to run is in `./search/search/main.hpp` as well as in the `makefile`
Also make sure that the domain you are trying to run the algorithm on has a problem file
```
~/search$ make
~/search$ ./<path to executable file> <algorithm alias in main.hpp> < <path to problem file>
```

## Runnning algocomp
```sh
python3 python/algocomp.py <path to executable> <path to folder to test in> [algo1 -args:algo2 -args...]
```
Example
```sh
python3 python/algocomp.py ./tiles/15md_solver ./korf100/ astar-basic:cafe -threads 2:kbfs -threads 4
```