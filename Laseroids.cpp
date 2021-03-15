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
static constexpr uint NUM_STARS = 15;
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
	for (uint i=0; i<count; i++) { delete ptr[i]; }
	iter = count = 0; // safety
}

static inline FLOAT dist (const Point& a, const Point& b)
{
	// calculate kind-of-distance quick
	// it's even better for jumping if speed of X and Y axis unit are calculated separately!

	//return (a-b).length();
	return max(abs(a.x-b.x),abs(a.y-b.y));
}

__attribute__((unused))
static bool sort4_v2 (IObject* ptr[])
{
	// try to reorder the 2 middle points of a 4-point path for shortest distance
	// this version uses a 'first' and a 'last' point of objects.
	// optimization will be better than first-point-only or middle-point-only
	// but calculation of 2 points will take longer, middle point would be fastest

	Point e0 = ptr[0]->last();
	Point a1 = ptr[1]->first();
	Point e1 = ptr[1]->last();
	Point a2 = ptr[2]->first();
	Point e2 = ptr[2]->last();
	Point a3 = ptr[3]->first();

	FLOAT d1 = dist(e0,a1)+dist(e1,a2)+dist(e2,a3);
	FLOAT d2 = dist(e0,a2)+dist(e2,a1)+dist(e1,a3);

	bool f = d2 < d1;
	if (f) std::swap(ptr[1], ptr[2]);
	return f;
}

__attribute__((unused))
static bool sort3_anf_v2 (IObject* ptr[])
{
	// try to reorder the first and the 2nd point of a 3-point path for shortest distance.
	// this assumes / is for the start of drawing the whole frame.
	// this version uses a 'first' and a 'last' point of objects.

	Point a0 = ptr[0]->first();
	Point e0 = ptr[0]->last();
	Point a1 = ptr[1]->first();
	Point e1 = ptr[1]->last();
	Point a2 = ptr[2]->first();

	FLOAT d1 = dist(e0,a1)+dist(e1,a2);
	FLOAT d2 = dist(e1,a0)+dist(e0,a2);

	bool f = d2 < d1;
	if (f) std::swap(ptr[0], ptr[1]);
	return f;
}

__attribute__((unused))
static bool sort3_end_v2 (IObject* ptr[])
{
	// try to reorder the last and the previous point of a 3-point path for shortest distance.
	// this assumes / is for the end of drawing the whole frame.
	// this version uses a 'first' and a 'last' point of objects.

	Point e0 = ptr[0]->last();
	Point a1 = ptr[1]->first();
	Point e1 = ptr[1]->last();
	Point a2 = ptr[2]->first();
	Point e2 = ptr[2]->last();

	FLOAT d1 = dist(e0,a1)+dist(e1,a2);
	FLOAT d2 = dist(e0,a2)+dist(e2,a1);

	bool f = d2 < d1;
	if (f) std::swap(ptr[1], ptr[2]);
	return f;
}

static bool sort4_v1 (IObject* ptr[])
{
	// try to reorder the 2 middle points of a 4-point path for shortest distance
	// this version uses a single point of each object.
	// faster but not as good as optimization with 'start' and 'end'

	Point a0 = ptr[0]->origin();
	Point a1 = ptr[1]->origin();
	Point a2 = ptr[2]->origin();
	Point a3 = ptr[3]->origin();

	FLOAT d1 = dist(a0,a1) /* +dist(a1,a2) */ +dist(a2,a3);
	FLOAT d2 = dist(a0,a2) /* +dist(a2,a1) */ +dist(a1,a3);

	bool f = d2 < d1;
	if (f) std::swap(ptr[1], ptr[2]);
	return f;
}

static bool sort3_anf_v1 (IObject* ptr[])
{
	// try to reorder the 2 middle points of a 4-point path for shortest distance
	// this version uses a single point of each object.
	// faster but not as good as optimization with 'start' and 'end'

	Point a0 = ptr[0]->origin();
	Point a1 = ptr[1]->origin();
	Point a2 = ptr[2]->origin();

	FLOAT d1 = dist(a0,a1)+dist(a1,a2);
	FLOAT d2 = dist(a1,a0)+dist(a0,a2);

	bool f = d2 < d1;
	if (f) std::swap(ptr[0], ptr[1]);
	return f;
}

static bool sort3_end_v1 (IObject* ptr[])
{
	// try to reorder the 2 middle points of a 4-point path for shortest distance
	// this version uses a single point of each object.
	// faster but not as good as optimization with 'start' and 'end'

	Point a0 = ptr[0]->origin();
	Point a1 = ptr[1]->origin();
	Point a2 = ptr[2]->origin();

	FLOAT d1 = dist(a0,a1)+dist(a1,a2);
	FLOAT d2 = dist(a0,a2)+dist(a2,a1);

	bool f = d2 < d1;
	if (f) std::swap(ptr[1], ptr[2]);
	return f;
}

#define sort4 sort4_v1
#define sort3_anf sort3_anf_v1
#define sort3_end sort3_end_v1

void DisplayList::_insert (uint i)
{
	memmove(ptr+i+1, ptr+i, (count-i)*sizeof(IObject*));
	count++;
}

void DisplayList::_remove (uint i)
{
	count--;
	memmove(ptr+i, ptr+i+1, (count-i)*sizeof(IObject*));
}

uint DisplayList::_find(IObject* o)
{
	// search for object
	// optimized for current object of iteration:

	if (iter_up)
	{
a:		for (uint i=iter; i>0; )
		{
			if (ptr[--i] == o) return i;
		}
		if (iter_up) goto b;
	}
	else
	{
b:		for (uint i=iter; i<count; i++)
		{
			if (ptr[i] == o) return i;
		}
		if (!iter_up) goto a;
	}
	return ~0u;
}

void DisplayList::remove (IObject* o)
{
	uint i = _find(o);
	if (i == ~0u) return; // not found
	if (iter > i) iter--;
	_remove(i);
}

void DisplayList::add (IObject* o)
{
	// add object to pre-sorted display list.
	// objects are initially sorted in ascending order of X coordinate
	// and pair-by-pair opimized gradually, so that sorting by X is no longer stricktly true.
	// new points are added before the first point which has a higher X coordinate

	// add() should not be called from within an iteration.
	// if the new object is to be inserted at the position of the iterator,
	// then it is undefined on which side of the iterator it is added.

	// add() is intended to add initial objects at game start
	// or random objects, e.g. the alien ship, outside of an iteration.

	if (count == NELEM(ptr)) { printf("sorted_objects: overflow!\n"); return; }
	if (count==0)
	{
		ptr[count++] = o;
		return;
	}

	Point p0 = o->origin();

	//IObject* oo;
	//for (oo = last(); oo && oo->origin().x > p0.x; oo = prev()) {}
	//if (oo) add_right_of(oo,o);
	//else add_at_iterator(o);

	uint i = count++;

	while (i > 0 && p0.x < ptr[i-1]->origin().x)
	{
		ptr[i] = ptr[i-1];
		i--;
	}

	ptr[i] = o;

	if (iter > i) iter++;
}

void DisplayList::add_left_of_iterator (IObject* o)
{
	// insert object below iterator
	// if iterating up then this object will not be returned by this iteration
	// if iterating down then this object will be returned next by this iteration

	if (count == NELEM(ptr)) { printf("sorted_objects: overflow!\n"); return; }

	_insert(iter);
	ptr[iter] = o;
	iter++;
}

void DisplayList::add_right_of_iterator (IObject* o)
{
	// insert object above iterator
	// if iterating up then this object will be returned next by this iteration
	// if iterating down then this object will not be returned by this iteration

	if (count == NELEM(ptr)) { printf("sorted_objects: overflow!\n"); return; }

	_insert(iter);
	ptr[iter] = o;
}

void DisplayList::add_at_iterator (IObject* o)
{
	// add object at iterator on the 'already-processed' side
	// so that it is not immediately returned by next()

	if (count == NELEM(ptr)) { printf("sorted_objects: overflow!\n"); return; }

	_insert(iter);
	ptr[iter] = o;
	if (iter_up) iter++;
}

void DisplayList::add_left_of (IObject* o, IObject* new_o)
{
	// the new object is added left to the reference object.
	// if this is at the iterator, then the new object is added
	//   on the same side of the iterator as the reference object.
	// the reference object must exist.

	// use this function if an Object spawns a child left of it,
	// e.g. if the space ship is firing to the left, then add bullets with this method.

	uint i = _find(o);
	assert(i<count);		// reference object must exist

	_insert(i);
	ptr[i] = new_o;

	if (iter > i) iter++;	// note: same equation for both directions

	// if (iter_up)
	// {
	// 	if (iter > i) iter++;
	// }
	// else
	// {
	// 	if (iter > i) iter++;
	// }
}

void DisplayList::add_right_of  (IObject* o, IObject* new_o)
{
	// the new object is added right to the reference object.
	// if this is at the iterator, then the new object is added
	//   on the same side of an iterator as the reference object.
	// the reference object must exist.

	// use this function if Object o spawns a child which is assumed to be right of o,
	// e.g. if the space ship is firing to the right, then add bullets with this method.

	uint i = _find(o);
	assert(i<count);		// reference object must exist

	_insert(i+1);
	ptr[i+1] = new_o;

	if (iter > i) iter++;	// note: same equation for both directions

	// if (iter_up)
	// {
	// 	if (iter > i) iter++;
	// }
	// else
	// {
	// 	if (iter > i) iter++;
	// }
}

bool DisplayList::optimize (uint i)
{
	// optimize paths to and from object i.
	// returns true if 2 points were swapped. (for statistics)

	if (count <= 2) return false;

	// is i the first point?
	if (i == 0) return sort3_anf(ptr);

	// is i the last point?
	if (i == count-1) return sort3_end(ptr+count-3);

	// sort [i-1,i]:
	if (i == 1 ? sort3_anf(ptr) : sort4(ptr+i-2)) return true;

	// sort [i,i+1]:
	return i == count-2 ? sort3_end(ptr+count-3) : sort4(ptr+i-1);
}

uint DisplayList::sort()
{
	// sort the list for optimized drawing
	// returns the number of swapped points. (for statistics)

	if (count <= 2) return 0;

	uint cnt = 0;
	cnt += sort3_anf(ptr);
	cnt += sort3_end(ptr+count-3);

	for (uint i=0; i+4<count; i++)
	{
		cnt += sort4(ptr+i);
	}

	//if(cnt) printf("sort swaps: %u in %u points\n",cnt,count);
	return cnt;
}


// =====================================================================
//						IObject - Interface for Objects
// =====================================================================

inline IObject::IObject(cstr _name) : _name(_name)
{
	//printf("new %s\n",_name);
}

inline IObject::~IObject()
{
	//printf("delete %s\n",_name);
	display_list.remove(this);
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

void Star::draw()
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

void Score::draw()
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

void Lifes::draw()
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

void Object::wrap_at_borders()
{
	// TODO: should be removed and re-inserted in display list
	// problem: this may be before or after the Iterator!

	t.dx = int16(t.dx);
	t.dy = int16(t.dy);
	//if (position.x < MINPOS) position.x += SIZE;
	//if (position.x > MAXPOS) position.x -= SIZE;
	//if (position.y < MINPOS) position.y += SIZE;
	//if (position.y > MAXPOS) position.y -= SIZE;
}

void Object::move()
{
	t.addOffset(movement);
	wrap_at_borders();
}

void Object::rotate (FLOAT angle)
{
	if (angle == 0) return;

	FLOAT dx=t.dx, dy=t.dy;
	t.dx = 0;
	t.dy = 0;
	t.rotate(angle);
	t.dx = dx;
	t.dy = dy;
}


// =====================================================================
//							BULLET
// =====================================================================

Bullet::Bullet (const Point& p, const Dist& m) :
	Object("Bullet", Transformation(m.dy/*fx*/,m.dy/*fy*/,m.dx/*sx*/,-m.dx/*sy*/,p.x,p.y), m),
	count_down(BULLET_LIFETIME)
{}

void Bullet::draw()
{
	XY2::setTransformation(t);
	XY2::drawLine(Point(0,0),Point(0,FLOAT(1.000)),fast_straight);
}

void Bullet::move()
{
	if (--count_down <= 0) { delete this; return; }

	Object::move();

	// hit tests:
	Point p0{t.dx,t.dy};
	for (uint i=0; i<display_list.count; i++)	// attn: iterator in use by loop over IObjects!
	{
		if (display_list.ptr[i]->hit(p0))
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

void Asteroid::draw()
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

void PlayerShip::draw()
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

void AlienShip::draw()
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
		display_list.sort();	// incremental optimization
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



























