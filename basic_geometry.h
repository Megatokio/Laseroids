// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#include <cmath>
#include <ostream>
#include "cdefs.h"


/*
 * Template for Data Type to Represent a Distance in 2-dimensional Space.
 */
template<typename T>
struct TDist
{
	T dx = 0, dy = 0;

	// Default c'tor: create Dist with dx = dy = 0
	TDist(){}

	// c'tor with initial values
	TDist (T dx, T dy) : dx(dx), dy(dy) {}

	// Calculate Length
	T length() const noexcept { return sqrt(dx*dx+dy*dy); } // <cmath>

	// Calculate direction:
	T direction() const noexcept { return atan(dy/dx); }

	// normalize to length 1:
	TDist normalized () { return TDist(*this) / length(); }

	// Add two Distances
	TDist& operator+= (const TDist& q) { dx+=q.dx; dy+=q.dy; return *this; }
	TDist operator+ (TDist q) const { return q += *this; }

	TDist& operator-= (const TDist& q) { dx-=q.dx; dy-=q.dy; return *this; }
	TDist operator- (const TDist& q) const { return TDist{dx-q.dx,dy-q.dy}; }

	// Multiply Distance with Factor.
	TDist& operator*= (T f) { dx*=f; dy*=f; return *this; }
	TDist operator* (T f) const { return TDist{dx*f,dy*f}; }

	// Divide Distance by Divisor.
	TDist& operator/= (T f) { dx/=f; dy/=f; return *this; }
	TDist operator/ (T d) const { return TDist{dx/d,dy/d}; }

	// Compare two distances for Equality.
	friend bool operator== (const TDist& lhs, const TDist& rhs)
	{
		return lhs.dx == rhs.dx && lhs.dy == rhs.dy;
	}
	friend bool operator!= (const TDist& lhs, const TDist& rhs)
	{
		return lhs.dx != rhs.dx || lhs.dy != rhs.dy;
	}

	TDist& rotate(T rad)	// CCW
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T x = cosin * dx - sinus * dy;
		const T y = cosin * dy + sinus * dx;
		dx = x;
		dy = y;
		return *this;
	}

	TDist& rotate(T sinus, T cosin)
	{
		const T x = cosin * dx - sinus * dy;
		const T y = cosin * dy + sinus * dx;
		dx = x;
		dy = y;
		return *this;
	}
};


/*
 * Template for Data Type to Represent a Point in 2-dimensional Space.
 */
template<typename T>
struct TPoint
{
	T x = 0, y = 0;

	// Default c'tor: x = y = 0.
	TPoint(){}

	// Create point with initial values.
	TPoint(T x, T y) : x(x), y(y) {}

	// Create point from another point with different underlying type.
	template<typename Q>
	explicit TPoint(const Q &q) : x(q.x), y(q.y) {}

	// Move Point by Distance: add Distance to Point.
	TPoint operator+ (const TDist<T> &d) const
	{
		return TPoint(x + d.dx, y + d.dy);
	}
	TPoint& operator+= (const TDist<T> &d)
	{
		x += d.dx;
		y += d.dy;
		return *this;
	}

	// Move Point by Distance: subtract Distance from Point.
	TPoint operator- (const TDist<T>& d) const
	{
		return TPoint(x - d.dx, y - d.dy);
	}
	TPoint& operator-= (const TDist<T> &d)
	{
		x -= d.dx;
		y -= d.dy;
		return *this;
	}

	// Calculate Distance between 2 Points.
	TDist<T> operator- (const TPoint &d) const
	{
		return TDist<T>(x - d.x, y - d.y);
	}

	// Multiply Point with Factor.
	// This scales the Image tho which this Point belongs
	// by this factor with the origin as the center.
	TPoint operator* (T a) const
	{
		return TPoint(x * a, y * a);
	}
	TPoint& operator*= (T a)
	{
		x *= a;
		y *= a;
		return *this;
	}

	// Divide Point by Divisor.
	// This scales the Image tho which this Point belongs
	// by this factor with the origin as the center.
	TPoint operator/ (T a) const
	{
		return TPoint(x / a, y / a);
	}
	TPoint& operator/= (T a)
	{
		x /= a;
		y /= a;
		return *this;
	}

	// Multiply Point by a Power of 2.
	// Evtl. this is only possible for Points based on integer types.
	// Used to convert int16 to int32 based Points.
	TPoint operator<< (int n) const
	{
		return TPoint(x << n, y << n);
	}

	// Divide Point by a Power of 2.
	// Evtl. this is only possible for Points based on integer types.
	// Used to convert int32 to int16 based Points.
	TPoint operator>> (int n) const
	{
		return TPoint(x >> n, y >> n);
	}

	// Compare 2 Points for non-equality
	bool operator!= (const TPoint& q) const noexcept
	{
		return x != q.x || y != q.y;
	}

	// Compare 2 Points for Equality
	friend bool operator== (const TPoint& lhs, const TPoint& rhs)
	{
		return lhs.x == rhs.x && lhs.y == rhs.y;
	}

	TPoint& rotate_cw (T sin, T cos)
	{
		T px = x * cos + y * sin;
		T py = y * cos - x * sin;
		x = px;
		y = py;
		return *this;
	}

	TPoint& rotate_ccw (T sin, T cos)
	{
		T px = x * cos - y * sin;
		T py = y * cos + x * sin;
		x = px;
		y = py;
		return *this;
	}
};


/*
 * Template for Data Type to Represent a Rectangle in 2-dim Space.
 */
template<typename T>
struct TRect
{
	T top = 0, left = 0, bottom = 0, right = 0;

	// Default c'tor: create empty Rect with all corners set to 0,0
	TRect(){}

	// Create Rect with initial values from 4 Coordinates.
	TRect(T top, T left, T bottom, T right) :
		top(top), left(left), bottom(bottom), right(right) {}

	// Create Rect with initial values from 2 Points.
	TRect(const TPoint<T> &topleft, const TPoint<T> &bottomright) :
		top(topleft.y), left(topleft.x), bottom(bottomright.y), right(bottomright.x) {}

	// Move Rect by adding a Dist.
	TRect operator+ (const TDist<T> &d) const noexcept
	{
		return TRect(top + d.dy, left + d.dx, bottom + d.dy, right + d.dx);
	}

	// Move Rect by subtracting a Dist.
	TRect operator- (const TDist<T> &d) const noexcept
	{
		return TRect(top - d.dy, left - d.dx, bottom - d.dy, right - d.dx);
	}

	// Scale Rect by a Factor.
	// Scales the Rect by this factor with the origin as the center.
	TRect operator* (T f) const noexcept		// scaled from origin
	{
		return TRect( top * f, left * f, bottom * f, right * f);
	}

	// Calculate the Bounding box of two Rects.
	void uniteWith (const TRect& q)
	{
		if (q.top > top) top = q.top;
		if (q.bottom < bottom) bottom = q.bottom;
		if (q.left < left) left = q.left;
		if (q.right > right) right = q.right;
	}

	// Compare 2 Rects for Equality
	friend bool operator==(const TRect& lhs, const TRect& rhs)
	{
		return lhs.left == rhs.left && lhs.top == rhs.top &&
			   lhs.right == rhs.right && lhs.bottom == rhs.bottom;
	}

	// Get Width of this Rect
	T width() const noexcept { return right - left; }

	// Get Height of this Rect
	T height() const noexcept { return top - bottom; }

	// Get bottom-left Point of this Rect
	TPoint<T> bottom_left() const noexcept { return TPoint<T>{left,bottom}; }

	// Get bottom-right Point of this Rect
	TPoint<T> bottom_right() const noexcept { return TPoint<T>{right,bottom}; }

	// Get top-left Point of this Rect
	TPoint<T> top_left() const noexcept { return TPoint<T>{left,top}; }

	// Get top-right Point of this Rect
	TPoint<T> top_right() const noexcept { return TPoint<T>{right,top}; }

	// Get Center of this Rect
	TPoint<T> center() const noexcept { return TPoint<T>{(left+right)/2,(bottom+top)/2}; }

	// Test whether this Rect is empty.
	// A Rect is non-empty if width and height are > 0.
	bool isEmpty() const noexcept { return right <= left || top <= bottom; }

	// Test whether this Rect fully encloses another Rect.
	// @param q: the other Rect
	bool encloses(const TRect& q) const noexcept
	{
		return left<=q.left && right>=q.right && bottom<=q.bottom && top>=q.top;
	}

	// Test whether a Point lies inside (or on an edge of) this Rect.
	bool contains(const TPoint<T>& p) const noexcept
	{
		return left<=p.x && right>=p.x && bottom<=p.y && top>=p.y;
	}

	// Calculate the Union of this Rect and a Point.
	// Grows the Rect so that it encloses the Point.
	TRect unitedWith (const TPoint<T>& p) const noexcept
	{
		return TRect(max(top,p.y),min(left,p.x),min(bottom,p.y),max(right,p.x));
	}

	// Force a Point inside this Rect.
	// If the Point is inside this Rect then it is returned unmodified.
	// Else it is moved to the nearest boundary of this Rect.
	TPoint<T> forcedInside(const TPoint<T>& p) const noexcept
	{
		return TPoint<T>(minmax(left,p.x,right),minmax(bottom,p.y,top));
	}

	// Grow this Rect so that it encloses the Point.
	// Calculates the Union of this Rect and a Point.
	// Note: there is also method @ref unitedWith() which does not modify this Rect
	//   but returns the resulting Rect.
	void uniteWith (const TPoint<T>& p) noexcept
	{
		if (p.y > top) top = p.y;
		if (p.y < bottom) bottom = p.y;
		if (p.x < left) left = p.x;
		if (p.x > right) right = p.x;
	}

	// Grow this Rect at all sides by a certain distance.
	// If the Dist is negative then the Rect will shrink.
	void grow (T d) noexcept { left-=d; right+=d; bottom-=d; top+=d; }

	// Shrink this Rect at all sides by a certain distance.
	// If the Dist is negative then the Rect will grow.
	void shrink (T d) noexcept { grow(-d); }
};


template<typename T>
struct TTransformation
{
	// A Transformation contains a 3 x 3 matrix:
	//
	//   [ m11 m12 m13 ]     [ fx sy px ]
	//   [ m21 m22 m23 ]  =  [ sx fy py ]
	//   [ m31 m32 m33 ]     [ dx dy pz ]
	//
	//   dx, dy: horizontal and vertical translation
	//   fx, fy: horizontal and vertical scaling
	//   sx, sy: horizontal and vertical shearing
	//   px, py: horizontal and vertical projection
	//   pz:     additional projection factor.

	// The coordinates are transformed using the following formula:
	//
	//	x' = fx*x + sx*y + dx
	//	y' = fy*y + sy*x + dy
	//
	//	if (is_projected)
	//		w' = px*x + py*y + pz
	//		x' /= w'
	//		y' /= w'

	FLOAT fx=1, fy=1, sx=0, sy=0, dx=0, dy=0;
	FLOAT px=0, py=0, pz=1;
	bool is_projected = false;	// must be last

	TTransformation()=default;
	TTransformation(T fx,T fy,T sx,T sy, T dx, T dy) : fx(fx),fy(fy),sx(sx),sy(sy),dx(dx),dy(dy){}
	TTransformation(T fx,T fy,T sx,T sy, T dx, T dy,T px,T py,T pz=1) :
		fx(fx),fy(fy),sx(sx),sy(sy),dx(dx),dy(dy),px(px),py(py),pz(pz),is_projected(px||py||pz!=1){}


	// ==========================
	// transform Point p
	//
	void transform (TPoint<T>& p)
	{
		T x = p.x;
		T y = p.y;

		p.x = fx*x + sx*y + dx;
		p.y = fy*y + sy*x + dy;

		if (is_projected)
		{
			T q = px*x + py*y + pz;
			p.x /= q;
			p.y /= q;
		}
	}

	// ==========================
	// return transformed Point p
	//
	TPoint<T> transformed (const TPoint<T>& p) const
	{
		T x = p.x;
		T y = p.y;
		TPoint<T> z { fx*x + sx*y + dx, fy*y + sy*x + dy };
		return is_projected ? z / (px*x + py*y + pz) : z;
	}


	TTransformation& addTransformation (T fx1, T fy1, T sx1, T sy1, T dx1, T dy1)
	{
		// calculate a combined transformation where the supplied transformation t1 is applied first:
		// dx = dx2 + dx1*fx2 + dy1*sx2    fx = fx1*fx2 + sy1*sx2    sx = sx1*fx2 + fy1*sx2
		// dy = dy2 + dx1*sy2 + dy1*fy2    sy = fx1*sy2 + sy1*fy2    fy = sx1*sy2 + fy1*fy2

		const T dx2=dx, dy2=dy, fx2=fx, fy2=fy, sx2=sx, sy2=sy;

		const T dx = dx2 + dx1*fx2 + dy1*sx2;
		const T dy = dy2 + dx1*sy2 + dy1*fy2;
		const T fx = fx1*fx2 + sy1*sx2;
		const T sy = fx1*sy2 + sy1*fy2;
		const T sx = sx1*fx2 + fy1*sx2;
		const T fy = sx1*sy2 + fy1*fy2;

		new(this) TTransformation(fx,fy,sx,sy,dx,dy);
		return *this;
	}
	TTransformation& addTransformation (const TTransformation& t)
	{
		// calculate a combined transformation where the supplied transformation t1 is applied first:
		return addTransformation(t.fx,t.fy,t.sx,t.sy,t.dx,t.dy);
	}
	TTransformation& operator+= (const TTransformation& t)
	{
		// calculate a combined transformation where the supplied transformation t1 is applied first:
		return addTransformation(t.fx,t.fy,t.sx,t.sy,t.dx,t.dy);
	}
	TTransformation operator+ (const TTransformation& t)
	{
		// calculate a combined transformation where the supplied transformation t1 is applied first:
		return TTransformation(*this) += t;
	}

	TTransformation& invert()
	{
		//  quot = 1 / (fy*fx - sx*sy)
		//	dx' = (dy*sx - dx*fy)*quot   fx' =  fy*quot   sx' = -sx*quot
		//	dy' = (dx*sy - dy*fx)*quot   sy' = -sy*quot   fy' =  fx*quot

		// check: (alt. calc.)
		// quot = (sy*sx - fx*fy)
		// dx' = (dx*fy-dy*sx)/quot   fx' = -fy/quot   sx' =  sx/quot
		// dy' = (dy*fx-dx*sy)/quot   sy' =  sy/quot   fy' = -fx/quot

		const T dx=this->dx, dy=this->dy, fx=this->fx, fy=this->fy, sx=this->sx, sy=this->sy;
		const T quot = 1 / (fy*fx - sx*sy);

		this->dx = (dy*sx - dx*fy)*quot;
		this->dy = (dx*sy - dy*fx)*quot;
		this->fx = fy*quot;
		this->fy = fx*quot;
		this->sx = -sx*quot;
		this->sy = -sy*quot;

		is_projected = false;	// can't handle (yet?)
		return *this;
	}
	TTransformation inverted()
	{
		return TTransformation(*this).invert();
	}


	// NOTE:
	// If Y-axis is pointing up, then Rotation is CCW
	// If Y-axis is pointing down, then Rotation is CW


	TTransformation& setScale (T scale)
	{
		if (sx) sx *= scale / fx;  fx = scale;
		if (sy) sy *= scale / fy;  fy = scale;
		return *this;
	}
	TTransformation& scale (T scale)
	{
		if (sx) sx *= scale;  fx *= scale;
		if (sy) sy *= scale;  fy *= scale;
		return *this;
	}
	TTransformation  scaled (T scale)
	{
		return TTransformation(*this).scale(scale);
	}
	TTransformation& setScale (T x, T y)
	{
		if (sx) sx *= x / fx;  fx = x;
		if (sy) sy *= y / fy;  fy = y;
		return *this;
	}
	TTransformation& scale (T x, T y)
	{
		if (sx) sx *= x;  fx *= x;
		if (sy) sy *= y;  fy *= y;
		return *this;
	}
	TTransformation  scaled (T x, T y)
	{
		return TTransformation(*this).scale(x,y);
	}

	TTransformation& setRotation (T rad)	// resets scale and shear
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
		return  *this;
	}
	TTransformation& rotate (T rad)
	{
		// Add rotation around the input origin
		// not around the output origin of the Transformation
		// => dx and dy are preserved and not rotated.

		if (rad != 0)
		{
			const T sinus = sin(rad);
			const T cosin = cos(rad);
			const T fx2=fx, fy2=fy, sx2=sx, sy2=sy;

			fx = cosin*fx2 + sinus*sx2;
			sy = cosin*sy2 + sinus*fy2;
			sx = cosin*sx2 - sinus*fx2;
			fy = cosin*fy2 - sinus*sy2;
		}
		return *this;
	}
	TTransformation  rotated (T rad)
	{
		return TTransformation(*this).rotate(rad);
	}

	TTransformation& setRotationAndScale (T rad, T scale)	// resets shear
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
		return *this;
	}
	TTransformation& setRotationAndScale (T rad, T x, T y)	// resets shear
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = x*cosin; sx = -x*sinus;
		fy = y*cosin; sy = +y*sinus;
		return *this;
	}
	TTransformation& rotateAndScale (T rad, T scale)
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		const T fx = cosin, sx = -sinus, dx = 0;
		const T fy = cosin, sy = +sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}
	TTransformation& rotateAndScale (T rad, T x, T y)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T fx = x*cosin, sx = -x*sinus, dx = 0;
		const T fy = y*cosin, sy = +y*sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}
	TTransformation  rotatedAndScaled (T rad, T scale)
	{
		return TTransformation(*this).rotateAndScale(rad, scale);
	}
	TTransformation  rotatedAndScaled (T rad, T x, T y)
	{
		return TTransformation(*this).rotateAndScale(rad,x,y);
	}

	TTransformation& setShear (T x, T y)
	{
		sx = x;
		sy = y;
		return *this;
	}
	TTransformation& shear (T x, T y)
	{
		operator += (TTransformation(1,1,x,y,0,0));	// TODO: optimize
		return *this;
	}
	TTransformation  sheared (T sx, T sy)
	{
		return TTransformation(*this).shear(sx,sy);
	}

	TTransformation& setOffset (T dx, T dy)
	{
		this->dx = dx;
		this->dy = dy;
		return *this;
	}
	TTransformation& setOffset (const TDist<T>& d)
	{
		dx = d.dx;
		dy = d.dy;
		return *this;
	}
	TTransformation& setOffset (const TPoint<T>& p)
	{
		dx = p.x;
		dy = p.y;
		return *this;
	}
	TTransformation& addOffset (T dx, T dy)
	{
		this->dx += dx;
		this->dy += dy;
		return *this;
	}
	TTransformation& addOffset (const TDist<T>& d)
	{
		dx += d.dx;
		dy += d.dy;
		return *this;
	}

	TTransformation& setProjection (T px, T py, T pz=1)
	{
		this->px=px;
		this->py=py;
		this->pz=pz;
		is_projected = px||py||pz!=1;
		return *this;
	}
	TTransformation& resetProjection ()
	{
		px = py = pz = 0;
		is_projected = false;
		return *this;
	}

	TTransformation& reset()
	{
		new(this) TTransformation();
		return *this;
	}
	TTransformation& set (T fx, T fy, T sx, T sy, T dx, T dy)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy);
		return *this;
	}
	TTransformation& set (T fx, T fy, T sx, T sy, T dx, T dy, T px, T py, T pz=1)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy,px,py,pz);
		return *this;
	}
};



typedef struct TPoint<FLOAT> Point;
typedef struct TDist<FLOAT> Dist;
typedef struct TRect<FLOAT> Rect;
typedef struct TTransformation<FLOAT> Transformation;


typedef struct TPoint<int32> IntPoint;
typedef struct TDist<int32> IntDist;
typedef struct TRect<int32> IntRect;





















