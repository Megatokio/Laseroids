// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "cdefs.h"
#include "basic_geometry.h"
class IObject;


class DisplayList
{
public:
	DisplayList() = default;
	~DisplayList();

	void add(IObject*);
	void add_left_of  (IObject* o, IObject* new_o);
	void add_right_of  (IObject* o, IObject* new_o);
	void add_left_of_iterator (IObject*);
	void add_right_of_iterator (IObject*);
	void add_at_iterator (IObject*);
	void remove(IObject*);

	uint sort();
	bool optimize(uint new_i);
	void sort4(uint i);
	void sort3_anf(uint i);
	void sort3_end(uint i);

	IObject* ptr[256];
	uint count = 0;

	IObject* next()	{ return iter<count ? ptr[iter++] : nullptr; }
	IObject* prev()	{ return iter > 0 ? ptr[--iter] : nullptr; }
	IObject* first(){ assert(count); iter_up=true;  iter=0;     return next(); }
	IObject* last()	{ assert(count); iter_up=false; iter=count; return prev(); }

	uint iter = 0;	// points between items: 0=before first, count=behind last
	bool iter_up;	// true: iteration was started at first(), else at last()

	void _insert(uint i);
	void _remove(uint i);
	uint _find(IObject*);
};


class IObject
{
public:
	IObject()=delete;
	IObject(cstr _name);
	virtual ~IObject();
	IObject(const IObject&) = delete;
	IObject& operator=(const IObject&) = delete;

	virtual Point origin() = 0;
	virtual Point first() { return origin(); }	// start of polygon / drawing
	virtual Point last() { return first(); }	// end of polygon / drawing
	virtual void draw() = 0;
	virtual void move() = 0;
	virtual bool hit (const Point& p) { return false; }

	cstr  _name = "IObject";
};


class Star : public IObject
{
public:
	Star(const Point& position);
	Star();		// star at random position

	virtual void draw() override;
	virtual void move() override {} // does not move
	virtual Point origin() override { return position; }

	Point position;
};

class Score : public IObject
{
public:
	Score(const Point& position);
	Score();	// score at default position

	virtual void draw() override;
	virtual void move() override {} // does not move
	virtual Point origin() override { return position; }

	Point position;
	uint score = 0;
};

class Lifes : public IObject
{
public:
	Lifes(const Point& position);
	Lifes();	// display of lifes left at standard position
	virtual void draw() override;
	virtual void move() override {} // does not move
	virtual Point origin() override { return position; }

	Point position;
	uint lifes = 4;
};



class Object : public IObject
{
public:
	Object (cstr _name) : IObject(_name) {}
	virtual ~Object () override {}
	Object (cstr _name, Transformation&&, const Dist& movement);
	Object (cstr _name, const Transformation&, const Dist& movement);

	Object (cstr _name, const Point& position, FLOAT orientation, FLOAT scale, const Dist& movement);
	Object (cstr _name, const Point& position) : IObject(_name) { t.setOffset(position); }
	Object (cstr _name, FLOAT x, FLOAT y) : IObject(_name) { t.setOffset(x,y); }

	virtual void draw() = 0;
	virtual void move();
	virtual bool overlap (const Point& pt) { return false; }
	virtual Point origin() override { return getPosition(); }

	Point getPosition() { return Point(t.dx,t.dy); }
	Dist getDirection() { return Dist(t.sx,t.fy); }		// Y axis
	FLOAT getOrientation(){ return getDirection().direction(); }

	void wrap_at_borders();
	void rotate(FLOAT angle);

	Transformation t;	// -> position, rotation and scale
	Dist  movement;		// dx,dy
};


class Bullet : public Object
{
public:
	Bullet() = default;
	Bullet(const Point& position, const Dist& movement);

	virtual void draw() override;
	virtual void move() override;

	int count_down;
};


class Asteroid : public Object
{
public:
	virtual ~Asteroid() override;
	Asteroid(uint size_id, const Point& position, const Dist& speed, FLOAT rotation=0);

	virtual void draw() override;
	virtual void move() override;
	virtual bool overlap (const Point& pt) override;
	virtual bool hit (const Point& p) override;

	uint  size;				// size class: 1 .. 4
	Point vertices[16];
	uint  num_vertices;
	FLOAT radians;
	FLOAT rotation = 0;		// rotational speed
};


class PlayerShip : public Object
{
public:
	PlayerShip();			// at (0,0)

	virtual void draw() override;
	virtual void move() override;
	virtual bool hit (const Point& p) override;

	bool shield = false;
	uint accelerating = 0;
	FLOAT rotation = 0;		// rotational speed

	void accelerate();
};

class AlienShip : public Object
{
public:
	bool visible = false;
	virtual void draw() override;
	virtual void move() override;
	virtual bool hit (const Point& p) override;
	//AlienShip(){printf("%s\n",__func__);}
	AlienShip(const Point& position);
};

class Laseroids
{
public:

	Laseroids();
	~Laseroids();

	void startNewGame();
	void runOneFrame();
	void accelerateShip();
	void rotateRight();
	void rotateLeft();
	void activateShield(bool=1);
	void shootCannon();
	bool isGameOver();
};

