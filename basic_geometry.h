// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#include <cmath>
#include <ostream>
#include "cdefs.h"


template<typename T>
struct TTransformation
{
	// A Transformation object contains a 3 x 3 matrix:
	//
	//   [ m11 m12 m13 ]
	//   [ m21 m22 m23 ]
	//   [ m31 m32 m33 ]
	//
	// m31 and m32: horizontal and vertical translation dx, dy.
	// m11 and m22: horizontal and vertical scaling fx, fy.
	// m21 and m12: horizontal and vertical shearing sx, sy.
	// m13 and m23: horizontal and vertical projection px, py,
	// with m33 as an additional projection factor.

	// The coordinates are transformed using the following formula:
	//
	//	x' = fx*x + sx*y + dx		= m11*x + m21*y + dx
	//	y' = fy*y + sy*x + dy		= m22*y + m12*x + dy
	//	if (is_projected)
	//		w' = px*x + py*y + pz	= m13*x + m23*y + m33
	//		x' /= w'
	//		y' /= w'

	//FLOAT fx=1, sy=0, px=0;	// m11, m12, m13
	//FLOAT sx=0, fy=1, py=0;	// m21, m22, m23
	//FLOAT dx=0, dy=0, pz=1;	// m31, m32, m33
	FLOAT fx=1, fy=1, sx=0, sy=0, dx=0, dy=0;
	FLOAT px=0, py=0, pz=1;
	bool is_projected=false;

	TTransformation()=default;
	TTransformation(T fx,T fy,T sx,T sy, T dx, T dy) : fx(fx),fy(fy),sx(sx),sy(sy),dx(dx),dy(dy){}
	TTransformation(T fx,T fy,T sx,T sy, T dx, T dy,T px,T py,T pz=1) :
		fx(fx),fy(fy),sx(sx),sy(sy),dx(dx),dy(dy),px(px),py(py),pz(pz),is_projected(px||py||pz!=1){}

	void setRotationCW (T rad)	// and reset scale
	{
		T sinus = sin(rad);
		T cosin = cos(rad);
		fx = cosin; sx = +sinus;
		fy = cosin; sy = -sinus;
	}

	void setRotationCCW (T rad)	// and reset scale
	{
		T sinus = sin(rad);
		T cosin = cos(rad);
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
	}

	void setScaleAndRotationCW (T scale, T rad)
	{
		T sinus = sin(rad) * scale;
		T cosin = cos(rad) * scale;
		fx = cosin; sx = +sinus;
		fy = cosin; sy = -sinus;
	}

	void setScaleAndRotationCCW (T scale, T rad)
	{
		T sinus = sin(rad) * scale;
		T cosin = cos(rad) * scale;
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
	}

	void setScaleAndRotationCW (T x, T y, T rad)
	{
		T sinus = sin(rad);
		T cosin = cos(rad);
		fx = x*cosin; sx = +x*sinus;
		fy = y*cosin; sy = -y*sinus;
	}

	void setScaleAndRotationCCW (T x, T y, T rad)
	{
		T sinus = sin(rad);
		T cosin = cos(rad);
		fx = x*cosin; sx = -x*sinus;
		fy = y*cosin; sy = +y*sinus;
	}

	void setShift (T dx, T dy)
	{
		this->dx=dx;
		this->dy=dy;
	}

	void setScale (T fx, T fy)	// and adjust shear
	{
		if (sx) sx *= fx / this->fx;
		if (sy) sy *= fy / this->fy;
		this->fx=fx;
		this->fy=fy;
	}

	void setShear (T sx, T sy)
	{
		this->sx=sx;
		this->sy=sy;
	}

	void setProjection (T px, T py, T pz=1)
	{
		this->px=px;
		this->py=py;
		this->pz=pz;
		is_projected = true;
	}

	void resetProjection ()
	{
		px = py = pz = 0;
		is_projected = false;
	}

	void reset() { new(this) TTransformation(); }

	void set (T fx, T fy, T sx, T sy, T dx, T dy)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy);
	}

	void set (T fx, T fy, T sx, T sy, T dx, T dy, T px, T py, T pz=1)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy,px,py,pz);
	}
};


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

	// Multiply Distance with Factor.
	TDist operator* (T f) const { return TDist(dx * f, dy * f); }

	// Divide Distance by Divisor.
	TDist operator/ (T d) const { return TDist{dx/d,dy/d}; }

	// Add two Distances
	TDist operator+ (const TDist& q) const { return TDist{dx+q.dx,dy+q.dy}; }

	// Compare two distances for Equality.
	friend bool operator== (const TDist& lhs, const TDist& rhs)
	{
		return lhs.dx == rhs.dx && lhs.dy == rhs.dy;
	}

	// Calculate normal vector of this distance.
	TDist normal () const { return (*this) / length(); }
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

	// Divide Point by Divisor.
	// This scales the Image tho which this Point belongs
	// by this factor with the origin as the center.
	TPoint operator/ (T a) const
	{
		return TPoint(x / a, y / a);
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

	TPoint& transform (const TTransformation<T>& t)
	{
		T x = this->x;
		T y = this->y;

		this->x = t.fx*x + t.sx*y + t.dx;
		this->y = t.fy*y + t.sy*x + t.dy;

		if (t.is_projected)
		{
			T q = t.px*x + t.py*y + t.pz;
			this->x /= q;
			this->y /= q;
		}

		return *this;
	}

	TPoint transformed (const TTransformation<T>& t) const
	{
		TPoint p { t.fx*x + t.sx*y + t.dx, t.fy*y + t.sy*x + t.dy };
		return t.is_projected ? p / (t.px*x + t.py*y + t.pz) : p;
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


typedef struct TPoint<FLOAT> Point;
typedef struct TDist<FLOAT> Dist;
typedef struct TRect<FLOAT> Rect;
typedef struct TTransformation<FLOAT> Transformation;


typedef struct TPoint<int32> IntPoint;
typedef struct TDist<int32> IntDist;
typedef struct TRect<int32> IntRect;


