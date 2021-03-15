// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "utilities.h"
#include <math.h>
#include "cdefs.h"
#include "Laseroids.h"
#include "XY2.h"
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

//static inline FLOAT sin (FLOAT a) { return sinf(a); }
//static inline FLOAT cos (FLOAT a) { return cosf(a); }
static constexpr FLOAT pi = FLOAT(3.1415926538);
//static inline FLOAT rad4deg(FLOAT deg) { return pi/180 * deg; }
static inline FLOAT rad4deg(int deg)   { return pi/180 * FLOAT(deg); }
//static inline int   deg4rad(FLOAT rad) { return int(180/pi * rad +FLOAT(0.5)); }


/*	Das Spielfeld wrapt in beiden Dimensionen:
*/
static constexpr FLOAT SIZE   = 0x10000;		// this should be SCANNER_WIDTH
static constexpr FLOAT MAXPOS = +0x7fff;
static constexpr FLOAT MINPOS = -0x8000;

static constexpr uint NUM_ASTEROIDS = 3;
static constexpr uint NUM_STARS = 7;
static constexpr uint NUM_LIFES = 4;		// incl. the currently active one

static constexpr uint  BULLET_LIFETIME = 30;
static constexpr FLOAT BULLET_SPEED = SIZE/BULLET_LIFETIME * FLOAT(0.7);
static constexpr FLOAT BULLET_LENGTH = FLOAT(0.666);


// ******** Components of Laseroids Game **************
static DisplayList display_list;
static Lifes* lifes;
static Score* score;
static PlayerShip* player;
static AlienShip* alien;
static uint num_asteroids = 0;



// =====================================================================
//						DISPLAY LIST
// =====================================================================

// all objects are added to the display list by the IObject constructor
// the display list provides methods to reorder the objects for fastest drawing

DisplayList::~DisplayList()
{
	while (root) { delete root; }
}

static inline FLOAT dist (const Point& p1, const Point& p2)
{
	// calculate kind-of-distance quick

	FLOAT a = abs(p1.x-p2.x);
	FLOAT b = abs(p1.y-p2.y);
	return a>b ? a+b/2 : a/2+b;
}
static inline FLOAT dist (const IObject& o1, const IObject& o2)
{
	return dist(o1.last(),o2.first());
}
static inline FLOAT dist (const IObject* o1, const IObject* o2)
{
	return dist(o1->last(),o2->first());
}

IObject* DisplayList::_best_insertion_point (IObject* new_o)
{
	IObject* o = root;
	FLOAT o_dist = FLOAT(1e30);
	IObject* i = root;

	do
	{
		FLOAT i_dist = dist(i,new_o) + dist(new_o,i->_next) - dist(i,i->_next);
		if (i_dist < o_dist)
		{
			o = i;
			o_dist = i_dist;
		}
	}
	while ((i=i->_next) != root);

	return o;	// best position is between o and o->_next
}

void DisplayList::remove (IObject* o)
{
	if (o == root)
	{
		root = o->_next;

		if (root == o)
		{
			root = iter = nullptr;
			return;
		}
	}

	if (o == iter)
	{
		iter = iter->_prev;
	}

	o->_unlink();
}

void DisplayList::add (IObject* o)
{
	// add object to display list.
	// use for initial objects at game start or objects popping up at random positions.

	// add() should not be called from within an iteration because
	// it is unpredictable on which side of the iterator it is added.

	if (root == nullptr)
	{
		root = o->_prev = o->_next = o;
		return;
	}

	if (root->_next == root)
	{
		o->_link_behind(root);
		return;
	}

	o->_link_behind(_best_insertion_point(o));
}

void DisplayList::add_left_of_iterator (IObject* o)
{
	// insert object before iterator
	// if iterating up then this object will not be returned by this iteration
	// if iterating down then this object will be returned next by this iteration

	o->_link_behind(iter);
	iter = o;
}

void DisplayList::add_right_of_iterator (IObject* o)
{
	// insert object behind iterator
	// if iterating up then this object will be returned next by this iteration
	// if iterating down then this object will not be returned by this iteration

	o->_link_behind(iter);
}

void DisplayList::add_left_of (IObject* o, IObject* new_o)
{
	// the new object is added left to the reference object.
	// if this is at the iterator, then the new object is added
	//   on the same side of an iterator as the reference object.

	new_o->_link_before(o);
}

void DisplayList::add_right_of (IObject* o, IObject* new_o)
{
	// the new object is added right to the reference object.
	// if this is at the iterator, then the new object is added
	//   on the same side of an iterator as the reference object.

	new_o->_link_behind(o);
	if (iter == o) iter = new_o;
}

bool DisplayList::_try_revert_path (IObject* b, IObject* c)
{
	// try to revert subpath b->c using o->first() only as an approximation
	// => reverting b->c does not change path length
	// => only length change at start and end need to be calculated

	// there must be at least 1 point outside range b..c

	IObject* a = b->_prev;
	IObject* d = c->_next;

	FLOAT ab = dist(a, b);
	FLOAT ac = dist(a, c);
	FLOAT bd = dist(b, d);
	FLOAT cd = dist(c, d);

	if (ac + bd < ab + cd)
	{
		a->_next = c;
		c->_prev = a;
		c->_next = b;
		b->_prev = c;
		b->_next = d;
		d->_prev = b;
		return true;
	}

	return false;
}

uint DisplayList::optimize()
{
	// reorder the list for optimized drawing
	// returns the number of optimizations. (for statistics)

	// exit if number of objects <= 2:
	if (!root || root->_next == root->_prev) return 0;

	uint cnt = 0;
	IObject* o = root;
	do
	{
		IObject* o2 = o->_next;
		if (_try_revert_path(o,o2) == false) continue;
		cnt++;
		if (o2==root) break;
	}
	while ((o=o->_next) != root);

	if (cnt) printf("optimize(): swaps: %u\n",cnt);
	return cnt;
}


// =====================================================================
//						IObject - Interface for Objects
// =====================================================================

inline IObject::IObject(cstr _name) : _name(_name)
{
	printf("new %s\n",_name);
}

inline IObject::~IObject()
{
	printf("delete %s\n",_name);
	display_list.remove(this);
}

void IObject::_link_behind(IObject* p)
{
	// link this behind p
	_prev = p;
	_next = p->_next;
	p->_next = this;
	_next->_prev = this;
}

void IObject::_link_before(IObject* n)
{
	// link this before n
	_next = n;
	_prev = n->_prev;
	n->_prev = this;
	_prev->_next = this;
}

void IObject::_unlink()
{
	_prev->_next = _next;
	_next->_prev = _prev;
}


// =====================================================================
//							STAR
// =====================================================================

Star::Star (const Point& position) : IObject("Star"), position(position)
{}

Star::Star() : IObject("Star"), position(rand(MINPOS,MAXPOS),rand(MINPOS,MAXPOS))
{
	//printf("star at %.0f,%.0f\n",position.x,position.y);
}

void Star::draw() const
{
	static const LaserSet set{ .speed=1, .pattern=0x3ff, .delay_a=0, .delay_m=0, .delay_e=12 };

	XY2::resetTransformation();
	XY2::setOffset(position.x,position.y);
	XY2::drawLine(Point(),Point(),set);	// TODO: drawPoint()
}


// =====================================================================
//							SCORE
// =====================================================================

Score::Score(const Point& position) : IObject("Score"), position(position), score(0)
{}

Score::Score() : IObject("Score"), position(MINPOS+5000,MAXPOS-11000), score(0)
{}

void Score::draw() const
{
	char text[] = "000";
	uint score = this->score;
	if (score>=1000) { text[0] += score/1000; score %= 1000; }
	if (score>=100)  { text[1] += score/100;  score %= 100; }
	text[2] += score/10;

	XY2::resetTransformation();
	XY2::setOffset(position.x,position.y);
	XY2::printText(Point(),400,400,text);
}


// =====================================================================
//							LIFES
// =====================================================================

Lifes::Lifes (const Point& position) : IObject("Lifes"), position(position), lifes(NUM_LIFES)
{}

Lifes::Lifes() : IObject("Lifes"), position(MINPOS+14000,MAXPOS-11000), lifes(NUM_LIFES)
{}

void Lifes::draw() const
{
	if (lifes <= 1) return;			// no additional lifes left

	static const FLOAT size = 500;
	static Point shape[] =
	{
		{ 0*size, 0*size},
		{-2*size, 1*size},
		{ 0*size, 5*size},
		{ 2*size, 1*size},
	};

	XY2::resetTransformation();
	XY2::setOffset(position.x,position.y);

	for (uint i=1; i<lifes; i++)	// draw one less because one is in use
	{
		if (i>1) XY2::addOffset(size*6,0);
		XY2::drawPolygon(4,shape,slow_straight);
	}
}


// =====================================================================
//							OBJECT
// =====================================================================

Object::Object (cstr name, Transformation&& t, const Dist& m) :
	IObject(name), t(std::move(t)), movement(m)
{}

Object::Object (cstr name, const Transformation& t, const Dist& m) :
	IObject(name), t(t), movement(m)
{}

Object::Object (cstr name, const Point& position, FLOAT orientation, FLOAT scale, const Dist& m) :
	IObject(name), movement(m)
{
	t.setRotationAndScale(orientation,scale);
	t.setOffset(position);
}

void Object::_wrap_at_borders()
{
	// ATTN: insertion may be before or after the Iterator!

	if (t.dx >= MINPOS && t.dx <= MAXPOS && t.dy >= MINPOS && t.dy <= MAXPOS)
		return;

	if (t.dx < MINPOS) t.dx += SIZE;
	if (t.dx > MAXPOS) t.dx -= SIZE;
	if (t.dy < MINPOS) t.dy += SIZE;
	if (t.dy > MAXPOS) t.dy -= SIZE;

	_unlink();
	display_list.add(this);
}

void Object::move()
{
	t.addOffset(movement);
	_wrap_at_borders();
}


// =====================================================================
//							BULLET
// =====================================================================

Bullet::Bullet (const Point& p, const Dist& m) :
	Object("Bullet", Transformation(m.dy/*fx*/,m.dy/*fy*/,m.dx/*sx*/,-m.dx/*sy*/,p.x,p.y), m),
	count_down(BULLET_LIFETIME)
{}

void Bullet::draw() const
{
	XY2::setTransformation(t);
	XY2::drawLine(Point(0,0),Point(0,FLOAT(1.000)),fast_straight);
}

void Bullet::move()
{
	if (--count_down <= 0) { delete this; return; }

	Object::move();

	// hit tests:
	Point p0 = last();		// position of tip
	for (IObject* o = _next; o!=this; o=o->_next)
	{
		if (o->hit(p0))
		{
			delete this;
			return;
		}
	}
}


// =====================================================================
//							ASTEROID
// =====================================================================

Asteroid::~Asteroid()
{
	num_asteroids--;
}

Asteroid::Asteroid (uint size, const Point& p, const Dist& m, FLOAT rotation) :
	Object("Asteroid", Transformation(1,1,0,0, p.x,p.y), m),
	size(size),
	rotation(rotation)
{
	num_asteroids++;

	FLOAT jitter;
	switch(size)
	{
	case 1: num_vertices =  4; radians = SIZE/1000*15; jitter = radians/2; break;
	case 2: num_vertices =  8; radians = SIZE/1000*22; jitter = radians/2; break;
	case 3:	num_vertices = 12; radians = SIZE/1000*30; jitter = radians/3; break;
	default:num_vertices = 16; radians = SIZE/1000*40; jitter = radians/4; break;
	}

	for (uint i=0; i<num_vertices; i++)
	{
		FLOAT a = 2*pi * i / num_vertices;
		FLOAT x = sin(a) * radians + rand(-jitter,+jitter);
		FLOAT y = cos(a) * radians + rand(-jitter,+jitter);
		new(vertices+i) Point(x,y);
	}
}

void Asteroid::draw() const
{
	XY2::setTransformation(t);
	XY2::drawPolygon(num_vertices,vertices,fast_rounded);
}

void Asteroid::move()
{
	rotate(rotation);
	Object::move();

	// TODO: collission test
}

bool Asteroid::overlap (const Point& pt)
{
	return (pt-Point(t.dx,t.dy)).length() <= radians;
}

bool Asteroid::hit (const Point& p)
{
	// quick test:
	if (dist(p,origin()) > radians) return false;

	// better test:
	if ((p-origin()).length() > radians) return false;

	// polygon test:
	// TODO

	score->score+=10;

	if (size == 1)
	{
		delete this;
		return true;
	}

	// split into 3 smaller ones
	Dist dist0{0.0f, radians * 0.666f};		// offset to new asteroid's center
	for (int i=0; i<=2; i++)
	{
		dist0.rotate(i ? pi*2/3 : rand(pi/3));
		FLOAT rot0 = rotation * rand(0.666f,1.5f);
		Dist speed0 = movement + dist0/size;
		Point p0 = p + dist0;
		//display_list.add_right_of(this, new Asteroid(size-1,p0,speed0,rot0));
		display_list.add(new Asteroid(size-1,p0,speed0,rot0));
	}

	delete this;
	return true;
}


// =====================================================================
//							PLAYER SHIP
// =====================================================================

// always at (0,0) with 0 speed and no rotation:
PlayerShip::PlayerShip() : Object("PlayerShip", Transformation(SIZE/90,SIZE/90, 0,0, 0,0), Dist(0,0))
{}

void PlayerShip::draw() const
{
	XY2::setTransformation(t);

	if (shield)
	{
		Rect bbox{4,-4,-4,4};
		XY2::drawEllipse(bbox,-pi/2,8,fast_rounded);
	}

	// thrust:
	static Point acc[][3] =
	{
		{{-0.75f,-2.5f}, {0,-1-2.5f},{+0.75f,-2.5f}},	// acc=1
		{{-1,    -2.5f}, {0,-2-2.5f},{+1,    -2.5f}},	// acc=2
		{{-1.25f,-2.5f}, {0,-3-2.5f},{+1.25f,-2.5f}}	// acc=3
	};

	static bool flicker=0;
	if (accelerating && (flicker=!flicker))
	{
		XY2::drawPolyLine(3,acc[minmax(0,accelerating/8,2)],fast_straight);
	}

	// ship:
	static Point shape[] =
	{
		{0, -2},
		{-2,-1},
		{0,  3},
		{2, -1},
		{0, -2},
		{0,2.5f}
	};
	XY2::drawPolyLine(6,shape,slow_straight);
}

void PlayerShip::accelerate()
{
	if (!accelerating) return;

	uint a = 1+accelerating/8;
	accelerating -= a;

	Dist d{getDirection()};		// orientation of ship
	d *= FLOAT(a) / 40;
	movement = movement + d;

	if (movement.length() > SIZE/50)	// max speed limiter
	{
		movement *= FLOAT(0.98);
	}
}

void PlayerShip::move()
{
	accelerate();
	rotate(rotation);
	Object::move();

	// TODO: collission / hit test
}

bool PlayerShip::hit (const Point&)
{
	// TODO
	return false;
}

// =====================================================================
//							ALIEN SHIP
// =====================================================================

void AlienShip::draw() const
{
	// TODO
}

void AlienShip::move()
{
	// TODO
}

bool AlienShip::hit (const Point&)
{
	// TODO
	return false;
}

AlienShip::AlienShip(const Point& _position) : Object("AlienShip", _position)
{
	// TODO
}


// =====================================================================
//							THE GAME
// =====================================================================

void draw_all()
{
	for (IObject* o = display_list.first(); o; o = display_list.next())
	{
		o->draw();
	}
}

void move_all()
{
	// move all objects
	// this includes collission detection and objects may be added or removed at random.

	for (IObject* o = display_list.first(); o; o = display_list.next())
	{
		o->move();
	}
}


Laseroids::~Laseroids()
{
	display_list.~DisplayList();
}

Laseroids::Laseroids()
{
	assert(display_list.count==0);

	srand(time_us_32());

	new(&display_list) DisplayList;
	display_list.add(lifes = new Lifes);
	display_list.add(score = new Score);
	for (uint i=0; i<NUM_STARS; i++) { display_list.add(new Star); }
	display_list.add(player = new PlayerShip);
	alien = nullptr;

	num_asteroids = 0;
	for (uint i=0; i<NUM_ASTEROIDS; i++)
	{
		Point pt{rand(MINPOS,MAXPOS),rand(MINPOS,MAXPOS)};
		Dist  spd{FLOAT(rand(-200,200)),FLOAT(rand(-200,200))};
		FLOAT rot = rad4deg(rand(-5,+5));
		uint size = 4; // largest
		display_list.add(new Asteroid(size,pt,spd,rot));
	}
}

void Laseroids::runOneFrame()
{
	if (lifes->lifes != 0)
	{
		score->score++;
		move_all();
		display_list.optimize();	// incremental optimization
		draw_all();
	}
}


void Laseroids::accelerateShip()
{
	activateShield(false);
	player->accelerating += 10;
}

void Laseroids::rotateRight()
{
	activateShield(false);
	if (player->rotation > rad4deg(2))
	{
		player->rotation = 0;
	}
	else if (player->rotation > rad4deg(-10))
	{
		player->rotation -= rad4deg(3);
	}
}

void Laseroids::rotateLeft()
{
	activateShield(false);
	if (player->rotation < rad4deg(-2))
	{
		player->rotation = 0;
	}
	else if (player->rotation < rad4deg(10))
	{
		player->rotation += rad4deg(3);
	}
}

void Laseroids::activateShield(bool f)
{
	player->shield = f;
}

void Laseroids::shootCannon()
{
	activateShield(false);
	Dist movement{player->getDirection().normalized()*BULLET_SPEED};
	Point position{player->getPosition()+movement};
	Bullet* bullet = new Bullet(position,movement);
	if (bullet) display_list.add_left_of(player, bullet);
	else printf("shootCannon: out of memory\n");
}

bool Laseroids::isGameOver()
{
	return lifes->lifes == 0;
}

void Laseroids::startNewGame()
{
	this->~Laseroids();
	new(this) Laseroids();
}



























