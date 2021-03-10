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


//static inline FLOAT sin (FLOAT a) { return sinf(a); }
//static inline FLOAT cos (FLOAT a) { return cosf(a); }
static constexpr FLOAT pi = FLOAT(3.1415926538);
static inline FLOAT rad4deg(FLOAT deg) { return pi/180 * deg; }
static inline FLOAT rad4deg(int deg)   { return pi/180 * FLOAT(deg); }


/*	Das Spielfeld ist nominal 10000 'Meter' groß und wrapt in beiden Dimensionen.
*/
static constexpr FLOAT SIZE   = 0x10000;		// this should be SCANNER_WIDTH
static constexpr FLOAT MAXPOS = +0x7fff;
static constexpr FLOAT MINPOS = -0x8000;

static constexpr uint NUM_ASTEROIDS = 3;



static FLOAT costab[91];
static void init_costab()
{
	for (uint16 i=0; i<=90; i++) costab[i] = cos(rad4deg(i));
}

static FLOAT cosin (int deg)
{
	uint udeg = uint(abs(deg));
	if (udeg >= 360) { udeg = udeg % 360; }
	if (udeg >= 180) { udeg = 360 - udeg; }

	return udeg <= 90 ? costab[udeg] : -costab[180-udeg];
}

static inline FLOAT sinus (int deg)
{
	return cosin(deg-90);
}

static inline uint rand (uint max)	// --> [0 .. max]
{
	uint mask = 0xffffffffu >> __builtin_clz(max);

	for(;;)
	{
		uint n = uint(rand()) & mask;
		if (n <= max) return n;
	}
}

static inline int rand (int min, int max)	// --> [min .. max]
{
	return min + int(rand(uint(max - min)));
}

static inline FLOAT rand (FLOAT max)
{
	return FLOAT(rand()) * max / FLOAT(RAND_MAX);
}

static inline FLOAT rand (FLOAT min, FLOAT max)
{
	return min + rand(max-min);
}

void Object::wrap ()
{
	t.dx = int16(t.dx);
	t.dy = int16(t.dy);
	//if (position.x < MINPOS) position.x += SIZE;
	//if (position.x > MAXPOS) position.x -= SIZE;
	//if (position.y < MINPOS) position.y += SIZE;
	//if (position.y > MAXPOS) position.y -= SIZE;
}


// =====================================================================
//							OBJECT
// =====================================================================

Object::Object (Transformation&& t, const Dist& movement, FLOAT rotation) :
	t(std::move(t)),
	movement(movement),
	rotation(rotation)
{}

Object::Object (const Transformation& t, const Dist& movement, FLOAT rotation) :
	t(t),
	movement(movement),
	rotation(rotation)
{}

Object::Object (const Point& position, FLOAT orientation, FLOAT scale, const Dist& movement, FLOAT rotation) :
	t(),
	movement(movement),
	rotation(rotation)
{
	t.setRotationAndScale(orientation,scale);
	t.setOffset(position);
}

void Object::move()
{
	t.addOffset(movement);
	wrap();
	t.rotate(rotation);
}


// =====================================================================
//							STAR
// =====================================================================

using StarPtr = Star*;

Star::Star () :
	Object(rand(MINPOS,MAXPOS),rand(MINPOS,MAXPOS))
{
	//printf("%s\n",__func__);
}

void Star::draw()
{
	XY2::setOffset(t.dx,t.dy);	// no need to set scale and rotation
	static const LaserSet set {.speed=1, .pattern=0x3ff, .delay_a=0, .delay_m=0, .delay_e=12};
	XY2::drawLine(Point(),Point(),set);	// TODO: drawPoint()
}

int compare_stars (const void* a, const void* b)
{
	const StarPtr f = StarPtr(a);
	const StarPtr s = StarPtr(b);
	if (f->t.dx > s->t.dx) return  1;
	if (f->t.dx < s->t.dx) return -1;
	return 0;
}


// =====================================================================
//							PLAYER SHIP
// =====================================================================

void PlayerShip::draw()
{
	//printf("%s\n",__func__);

	XY2::setTransformation(t);

	// thrust:
	static Point acc[][3] =
	{
		{{-0.75f,-2.5f}, {0,-1-2.5f},{+0.75f,-2.5f}},	// acc=1
		{{-1,    -2.5f}, {0,-2-2.5f},{+1,    -2.5f}},	// acc=2
		{{-1.25f,-2.5f}, {0,-3-2.5f},{+1.25f,-2.5f}}	// acc=3
	};
	static bool flicker=0;
	if (acceleration>=1 && (flicker=!flicker))
	{
		XY2::drawPolyLine(3,acc[minmax(0,int(acceleration)-1,2)],fast_straight);
	}

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

void PlayerShip::move()
{
	Dist d{t.fx,t.fy}; d *= acceleration / d.length();
	movement = movement + d;
	Object::move();
}

PlayerShip::PlayerShip()
{
	printf("%s\n",__func__);

	t.setScale(SIZE/90);
	// t.orientation=0  = ok
	// t.position={0,0} = ok

	rotation = 2;		// TEST
	acceleration = 2;	// TEST
}


// =====================================================================
//							ALIEN SHIP
// =====================================================================

void AlienShip::draw(){}
void AlienShip::move(){}
AlienShip::AlienShip(const Point& _position)
:
	Object(_position)
{
	printf("%s\n",__func__);
}


// =====================================================================
//							BULLET
// =====================================================================

void Bullet::draw()
{
	XY2::setTransformation(t);
	XY2::drawLine(Point(0,0),Point(0,80),fast_straight);
}

//void Bullet::move(){}

Bullet::Bullet (const Point& _position, const Dist& _movement) :
	Object(_position)
{
	printf("%s\n",__func__);
	t.setRotation(_movement.direction());
	movement = _movement;
}


// =====================================================================
//							ASTEROID
// =====================================================================

void Asteroid::draw()
{
	XY2::setTransformation(t);
	XY2::drawPolygon(num_vertices,vertices,fast_rounded);
}

//void Asteroid::move()
//{
//	Object::move();
//}

Asteroid::Asteroid (uint size, const Point& _position, const Dist& _speed, FLOAT _orientation, FLOAT _rotation) :
	Object(_position,_orientation,1, _speed,_rotation),
	size(size)
{
	//printf("%s\n",__func__);

	FLOAT jitter;
	switch(size)
	{
	case 1: num_vertices =  4; radians = SIZE/1000*15; jitter = radians/2; break;
	case 2: num_vertices =  8; radians = SIZE/1000*25; jitter = radians/2; break;
	case 3:	num_vertices = 12; radians = SIZE/1000*30; jitter = radians/3; break;
	default:num_vertices = 16; radians = SIZE/1000*40; jitter = radians/4; break;
	}

	for (uint i=0; i<num_vertices; i++)
	{
		int   a = int(360 * i / num_vertices);
		FLOAT x = sinus(a) * radians + rand(-jitter,+jitter);
		FLOAT y = cosin(a) * radians + rand(-jitter,+jitter);
		new(vertices+i) Point(x,y);
	}
}


// =====================================================================
//							THE GAME
// =====================================================================

Laseroids::Laseroids()
{
	printf("%s\n",__func__);

	// setup cosine table:
	init_costab();

	// sort stars
	qsort(stars,num_stars,sizeof(Star),compare_stars);

	num_asteroids = NUM_ASTEROIDS;
	for (uint i=0; i<NUM_ASTEROIDS; i++)
	{
		Point pt{FLOAT(rand(MINPOS,MAXPOS)),FLOAT(rand(MINPOS,MAXPOS))};
		Dist  spd{FLOAT(rand(-90,90)),FLOAT(rand(-90,90))};
		int16 ori = 0;
		int16 rot = int16(rand(-2,+2));
		new(asteroids+i) Asteroid(4/*largest*/,pt,spd,ori,rot);
	}
}

void Laseroids::run_1_frame()
{
	move_all();
	//test_collissions();
	draw_all();
}


void Laseroids::draw_all()
{
	XY2::resetTransformation();
	//XY2::drawRect(Rect(MAXPOS/2,MINPOS/2,MINPOS/2,MAXPOS/2),fast_straight); // TEST

	for (uint i=0; i<num_stars; i++)
	{
		stars[i].draw();
	}

	for (uint i=0; i<num_asteroids; i++)
	{
		asteroids[i].draw();
	}

	for (uint i=0; i<num_bullets; i++)
	{
		bullets[i].draw();
	}

	alien.draw();
	player.draw();
}

void Laseroids::move_all()
{
	for (uint i=0; i<num_asteroids; i++)
	{
		asteroids[i].move();
	}

	for (uint i=0; i<num_bullets; i++)
	{
		bullets[i].move();
	}

	alien.move();
	player.move();
}

void Laseroids::test_collissions()
{
	printf("%s: TODO\n",__func__);
}

void Laseroids::accelerate()
{
	if (player.acceleration < 3) player.acceleration++;
}

void Laseroids::decelerate()
{
	if (player.acceleration > 0) player.acceleration--;
}

void Laseroids::rotate_right()
{
	if (player.rotation > -2) player.rotation--;
}

void Laseroids::rotate_left()
{
	if (player.rotation < 2) player.rotation++;
}

void Laseroids::activate_shield(bool f)
{
	player.shield = f;
}

void Laseroids::shoot()
{
	if (num_bullets < NELEM(bullets))
	{
		//TODO
	}
}
















