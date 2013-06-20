// Copyright © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.
#include "body.hpp"

struct Lvl;

static const double Dex = 5.0;	// Initial value from mid

static const double Runspeed = 2.0 + Dex / 4.0;

static const double Jmpspeed = 7.0 + Dex / 5.0;

enum { Maxjframes = 8 };

struct Player {

	enum {
		Left = 1<<0,
		Right = 1 << 1,
		Jump = 1 << 2,
	};

	enum {
		// from mid
		Width = 21,
		Offx = 7,
		Height = 29,
		Offy = 2,
	};

	Player() { }

	Player(unsigned int x, unsigned int y, unsigned int w, unsigned int h) :
		body(x, y, w, h), jframes(0) { }

	Player(const Player &o) : body(o.body), jframes(o.jframes) { }

	bool operator==(const Player &o) const {
		return jframes == o.jframes && body == o.body;
	}

	bool operator!=(const Player &o) const {
		return !(*this == o);
	}


	void act(const Lvl&, unsigned int);

	// bottom left
	geom2d::Pt loc() { return body.bbox.min; }

	// canjump returns true if the jump action can
	// actually do anything.
	bool canjump() const {
		return !body.fall || (jframes == 0 && body.dy <= 0) || jframes > 0;
	}

	Body body;
	unsigned char jframes;

private:
	// xvel returns the x velocity given the control bits.
	double xvel(const Lvl&, unsigned int);

	void chngjmp(unsigned int);
};