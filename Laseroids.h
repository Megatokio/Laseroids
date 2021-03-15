// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "cdefs.h"
#include "basic_geometry.h"


class IObject
{
public:
	IObject()=delete;
	IObject(cstr _name);
	virtual ~IObject();
	IObject(const IObject&) = delete;
	IObject& operator=(const IObject&) = delete;

	virtual Point origin() const = 0;
	virtual Point first() const { return origin(); }	// start of polygon / drawing
	virtual Point last() const { return first(); }		// end of polygon / drawing
	virtual void draw() const = 0;
	virtual void move() = 0;
	virtual bool hit (const Point&) { return false; }

	cstr  _name = "IObject";
	IObject* _next;
	IObject* _prev;
	void _link_behind(IObject* p);	// link this behind p
	void _link_before(IObject* n);	// link this before n
	void _unlink();
};


class DisplayList
{
public:
	DisplayList() = default;
	~DisplayList();

	void add (IObject*);
	void add_left_of (IObject* o, IObject* new_o);
	void add_right_of (IObject* o, IObject* new_o);
	void add_left_of_iterator (IObject*);
	void add_right_of_iterator (IObject*);
	void remove (IObject*);

	uint optimize();
	bool _try_revert_path(IObject* a, IObject* b);
	IObject* _best_insertion_point (IObject* new_o);

	// iterate:
	IObject* first(){ iter = root; return iter; }
	IObject* next()	{ iter = iter->_next; return iter==root ? nullptr : iter; }

	IObject* root = nullptr;

	IObject* iter = nullptr;	// points at recently with _next() returned item
								// logical position is between iter and iter->_next
};


class Star : public IObject
{
public:
	Star(const Point& position);
	Star();		// star at random position

	virtual void draw() const override;
	virtual void move() override {} // does not move
	virtual Point origin() const override { return position; }

	Point position;
};

class Score : public IObject
{
public:
	Score(const Point& position);
	Score();	// score at default position

	virtual void draw() const override;
	virtual void move() override {} // does not move
	virtual Point origin() const override { return position; }

	Point position;
	uint score = 0;
};

class Lifes : public IObject
{
public:
	Lifes(const Point& position);
	Lifes();	// display of lifes left at standard position
	virtual void draw() const override;
	virtual void move() override {} // does not move
	virtual Point origin() const override { return position; }

	Point position;
	uint lifes = 4;
};



class Object : public IObject
{
public:
	Object (cstr name) : IObject(name) {}
	virtual ~Object () override {}
	Object (cstr name, Transformation&&, const Dist& movement);
	Object (cstr name, const Transformation&, const Dist& movement);

	Object (cstr name, const Point& position, FLOAT orientation, FLOAT scale, const Dist& movement);
	Object (cstr name, const Point& position) : IObject(name) { t.setOffset(position); }
	Object (cstr name, FLOAT x, FLOAT y) : IObject(name) { t.setOffset(x,y); }

	virtual void draw() const override = 0;
	virtual void move() override;
	virtual bool overlap (const Point&) { return false; }
	virtual Point origin() const override { return getPosition(); }

	Point getPosition() const { return Point(t.dx,t.dy); }
	Dist getDirection() const { return Dist(t.sx,t.fy); }		// Y axis
	FLOAT getOrientation() const { return getDirection().direction(); }

	void _wrap_at_borders();
	void rotate(FLOAT angle) { if(angle != 0) t.rotate(angle); }

	Transformation t;	// -> position, rotation and scale
	Dist  movement;		// dx,dy
};


class Bullet : public Object
{
public:
	Bullet(const Point& position, const Dist& movement);

	virtual void draw() const override;
	virtual void move() override;

	int count_down;
};


class Asteroid : public Object
{
public:
	virtual ~Asteroid() override;
	Asteroid(uint size_id, const Point& position, const Dist& speed, FLOAT rotation=0);

	virtual void draw() const override;
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

	virtual void draw() const override;
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
	virtual void draw() const override;
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





