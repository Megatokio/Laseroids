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

	//TPoint& transform_ (const TTransformation<T>& t)
	//{
	//	T x = this->x;
	//	T y = this->y;
	//
	//	this->x = t.fx*x + t.sx*y + t.dx;
	//	this->y = t.fy*y + t.sy*x + t.dy;
	//
	//	if (t.is_projected)
	//	{
	//		T q = t.px*x + t.py*y + t.pz;
	//		this->x /= q;
	//		this->y /= q;
	//	}
	//
	//	return *this;
	//}
	//
	//TPoint transformed_ (const TTransformation<T>& t) const
	//{
	//	TPoint p { t.fx*x + t.sx*y + t.dx, t.fy*y + t.sy*x + t.dy };
	//	return t.is_projected ? p / (t.px*x + t.py*y + t.pz) : p;
	//}
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

	void transform (TPoint<T>& p)
	{
		// apply transformation to Point p

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

	TPoint<T> transformed (const TPoint<T>& p) const
	{
		// return transformed Point p

		T x = p.x;
		T y = p.y;
		TPoint<T> z { fx*x + sx*y + dx, fy*y + sy*x + dy };
		return is_projected ? z / (px*x + py*y + pz) : z;
	}

	TTransformation& operator+= (const TTransformation& t)
	{
		// a combined transformation is calculated where the supplied transformation t is applied first:
		// dx12 = dx2 + dx1*fx2 + dy1*sx2    fx12 = fx1*fx2 + sy1*sx2    sx12 = sx1*fx2 + fy1*sx2
		// dy12 = dy2 + dx1*sy2 + dy1*fy2    sy12 = fx1*sy2 + sy1*fy2    fy12 = sx1*sy2 + fy1*fy2

		const T dx1=t.dx, dy1=t.dy, fx1=t.fx, fy1=t.fy, sx1=t.sx, sy1=t.sy;
		const T dx2=  dx, dy2=  dy, fx2=  fx, fy2=  fy, sx2=  sx, sy2=  sy;

		const T dx = dx2 + dx1*fx2 + dy1*sx2;
		const T dy = dy2 + dx1*sy2 + dy1*fy2;
		const T fx = fx1*fx2 + sy1*sx2;
		const T sy = fx1*sy2 + sy1*fy2;
		const T sx = sx1*fx2 + fy1*sx2;
		const T fy = sx1*sy2 + fy1*fy2;

		new(this) TTransformation(fx,fy,sx,sy,dx,dy);
		return *this;
	}

	TTransformation operator+ (const TTransformation& t)
	{
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


	void setRotationCW (T rad)	// and reset scale
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = cosin; sx = +sinus;
		fy = cosin; sy = -sinus;
	}
	void setRotationCCW (T rad)	// and reset scale
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
	}


	TTransformation& rotateCW (T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T fx = cosin, sx = +sinus, dx = 0;
		const T fy = cosin, sy = -sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}
	TTransformation& rotateCCW (T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T fx = cosin, sx = -sinus, dx = 0;
		const T fy = cosin, sy = +sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}

	TTransformation rotatedCW (T rad)
	{
		return TTransformation(*this).rotatedCW(rad);
	}
	TTransformation rotatedCCW (T rad)
	{
		return TTransformation(*this).rotatedCCW(rad);
	}

	void setScaleAndRotationCW (T scale, T rad)
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		fx = cosin; sx = +sinus;
		fy = cosin; sy = -sinus;
	}
	void setScaleAndRotationCCW (T scale, T rad)
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		fx = cosin; sx = -sinus;
		fy = cosin; sy = +sinus;
	}

	void setScaleAndRotationCW (T x, T y, T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = x*cosin; sx = +x*sinus;
		fy = y*cosin; sy = -y*sinus;
	}
	void setScaleAndRotationCCW (T x, T y, T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		fx = x*cosin; sx = -x*sinus;
		fy = y*cosin; sy = +y*sinus;
	}


	TTransformation& scaleAndRotateCW (T scale, T rad)
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		const T fx = cosin, sx = +sinus, dx = 0;
		const T fy = cosin, sy = -sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}
	TTransformation& scaleAndRotateCCW (T scale, T rad)
	{
		const T sinus = sin(rad) * scale;
		const T cosin = cos(rad) * scale;
		const T fx = cosin, sx = -sinus, dx = 0;
		const T fy = cosin, sy = +sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}

	TTransformation& scaleAndRotateCW (T x, T y, T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T fx = x*cosin, sx = +x*sinus, dx = 0;
		const T fy = y*cosin, sy = -y*sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}
	TTransformation& scaleAndRotateCCW (T x, T y, T rad)
	{
		const T sinus = sin(rad);
		const T cosin = cos(rad);
		const T fx = x*cosin, sx = -x*sinus, dx = 0;
		const T fy = y*cosin, sy = +y*sinus, dy = 0;

		operator+=(TTransformation(fx,fy,sx,sy,dx,dy));		// TODO: optimize
		return *this;
	}

	TTransformation scaledAndRotatedCW (T scale, T rad)
	{
		return TTransformation(*this).scaledAndRotatedCW(scale,rad);
	}
	TTransformation scaledAndRotatedCCW (T scale, T rad)
	{
		return TTransformation(*this).scaledAndRotatedCCW(scale,rad);
	}

	TTransformation scaledAndRotatedCW (T x, T y, T rad)
	{
		return TTransformation(*this).scaledAndRotatedCW(x,y,rad);
	}
	TTransformation scaledAndRotatedCCW (T x, T y, T rad)
	{
		return TTransformation(*this).scaledAndRotatedCCW(x,y,rad);
	}



	TTransformation& setTranslation (T dx, T dy)
	{
		this->dx = dx;
		this->dy = dy;
		return *this;
	}

	TTransformation& setTranslation (TDist<T> d)
	{
		dx = d.dx;
		dy = d.dy;
		return *this;
	}

	TTransformation& operator+= (const TDist<T>& d)
	{
		dx += d.dx;
		dy += d.dy;
		return *this;
	}
	TTransformation operator+ (const TDist<T>& d)
	{
		return TTransformation(*this).operator+=(d);
	}

	TTransformation& operator-= (const TDist<T>& d)
	{
		dx -= d.dx;
		dy -= d.dy;
		return *this;
	}
	TTransformation operator- (const TDist<T>& d)
	{
		return TTransformation(*this).operator-=(d);
	}



	TTransformation& setScale (T fx, T fy)	// and adjust shear, keep offset
	{
		if (sx) sx *= fx / this->fx;
		if (sy) sy *= fy / this->fy;
		this->fx = fx;
		this->fy = fy;
		return *this;
	}
	TTransformation& scale (T fx, T fy)
	{
		if (sx) sx *= fx;
		if (sy) sy *= fy;
		this->fx *= fx;
		this->fy *= fy;
		return *this;
	}
	TTransformation scaled (T fx, T fy)
	{
		return TTransformation(*this).scale(fx,fy);
	}



	TTransformation& setShear (T sx, T sy)
	{
		this->sx = sx;
		this->sy = sy;
		return *this;
	}
	TTransformation& shear (T sx, T sy)
	{
		operator+=(TTransformation(1,1,sx,sy,0,0));	// TODO: optimize
		return *this;
	}
	TTransformation sheared (T sx, T sy)
	{
		return TTransformation(*this).shear(sx,sy);
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

	void reset()
	{
		new(this) TTransformation();
	}

	void set (T fx, T fy, T sx, T sy, T dx, T dy)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy);
	}

	void set (T fx, T fy, T sx, T sy, T dx, T dy, T px, T py, T pz=1)
	{
		new(this) TTransformation(fx,fy,sx,sy,dx,dy,px,py,pz);
	}

};



typedef struct TPoint<FLOAT> Point;
typedef struct TDist<FLOAT> Dist;
typedef struct TRect<FLOAT> Rect;
typedef struct TTransformation<FLOAT> Transformation;


typedef struct TPoint<int32> IntPoint;
typedef struct TDist<int32> IntDist;
typedef struct TRect<int32> IntRect;


