// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#pragma once

#include <cstdio>
#include <cassert>
#include "body.hpp"
#include "../graphics/image.hpp"

struct Tile {
	constexpr static unsigned int Collide = 1<<0;
	constexpr static unsigned int Water = 1<<1;
	constexpr static unsigned int Down = 1 << 4;
	constexpr static unsigned int Up = 1 << 5;
	constexpr static unsigned int Opaque = 1 << 6;
	

	
	constexpr static unsigned int Width = 32;
	constexpr static unsigned int Height = 32;
	

	static int read(FILE*);

	static Bbox bbox(unsigned int x, unsigned int y) {
		return Bbox(x * Width, y * Height,
			(x + 1) * Width, (y + 1) * Height);
	}

	static void draw(Image&, unsigned int, unsigned int, Color);

	Tile() : c(0) { }

	Tile(char ch, unsigned int f) : c(ch), flags(f) { }

	Isect isect(unsigned int x, unsigned int y, const Bbox & r) const {
		if (!(flags & Collide))
			return Isect();	
		return r.isect(bbox(x, y));
	}

	double gravity() const {
		static const double Grav = 0.5;
		return flags & Water ? 0.5 * Grav : Grav;
	}

	double drag() const {
		return flags & Water ? 0.7 : 1.0;
	}

	char c;	// c == 0 is invalid
	unsigned int flags;
};

struct Tiles {

	Tiles();

	bool istile(int t) const {
		return t >= 0 && t < Ntiles && tiles[t].c != 0;
	}

	const Tile &operator[](unsigned int i) const {
		assert (tiles[i].c != 0);
		assert ((unsigned int) tiles[i].c == i);
		return tiles[i];
	}

	enum { Ntiles = 256 };

private:
	Tile tiles[Ntiles];
};

extern const Tiles tiles;
