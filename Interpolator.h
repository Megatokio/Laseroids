// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "cdefs.h"
#include "basic_geometry.h"
#if XY2_USE_MAP_PICO_INTERP
extern "C"{
#include "hardware/interp.h"
}
#endif

template<uint bits> static constexpr uint mask = (1<<bits) - 1;


/*	clamp uint value to n bits
*/
template<uint bits> inline void clamp (uint& x)
{
	if (UNLIKELY(x>>bits)) x = int(x) < 0 ? 0 : mask<bits>;
}


/*  Interpolate value x between Ki and Kj
	Ki and Kj are the transformed values of i and j
	->	calculate Kx = transformed value of x
		with i <= x < j
		i & mask<bits> = 0
		j & mask<bits> = 0
		j = i + 1<<bits

	limits: (Kj-Ki) * (x & mask) must fit in uint
	=> e.g. uint=uint32 and bits=8   => all Kj-Ki pairs must fit in 24 bits
	=> e.g. uint=uint16 and bits=8   => all Kj-Ki pairs must fit in 8 bits
*/
template<uint bits> inline uint interpolate (uint x, uint Ki, uint Kj)
{
	return Ki + (((Kj-Ki) * (x & mask<bits>)) >> bits);
}

#if XY2_USE_MAP_PICO_INTERP
inline uint interpolate_Pico (uint x, uint Ki, uint Kj)
{
	// The RP2040 interpolator always uses 8 bits
	// and intermediate result must fit in 40 bits (which is always true)
	// the Interpolator must have been setup before: mode, shift, mask etc.

	interp0->base[0] = Ki;
	interp0->base[1] = Kj;
	interp0->accum[0] = x;
	return interp0->peek[1];
}
#endif

/*	map an input value to an output value using a map[].
	The map[] must have 1<<map_bits ranges which require 1<<map_bits + 1 values.
	The low order bits are interpolated.
	The input value x must be in range 0 to 1<<(map_bits+low_order_bits> excl.
	It will be clamped to this range.
*/
template<uint map_bits, uint bits>
uint map1D (uint x, uint* map)
{
	clamp<map_bits+bits>(x);

	uint i = x >> bits;
	return interpolate<bits>(x, map[i], map[i+1]);
}



/* ==========================================
	  clamp, interpolate and map 2D points
   ========================================== */


/*  The scanner uses unsigned coordinates.
	Unsigned coordinates also make clamping easier. :-)
*/
//struct Point2D { uint32 x,y; };
typedef struct TPoint<uint32> Point2D;


/*	clamp uint coordinates of Point2D to n bits
*/
template<uint bits> inline void clamp (Point2D& P)
{
	if (UNLIKELY(P.x>>bits)) P.x = int32(P.x) < 0 ? 0 : mask<bits>;
	if (UNLIKELY(P.y>>bits)) P.y = int32(P.y) < 0 ? 0 : mask<bits>;
}


/*  Interpolate point at alpha a between Pi and Pj
	Pi and Pj are the transformed points of i and j
	->	calculate Pa = transformed point of a
		with i <= a < j
		i & mask<bits> = 0
		j & mask<bits> = 0
		j = i + 1<<bits

	limits: both coordinates of (Pj-Pi) * (a & mask) must fit in uint32
	=> for alpha/mask with 8 bits all Pj-Pi coordinate pairs must fit in 24 bits
*/
template<uint bits> inline Point2D interpolate (uint a, Point2D Pi, Point2D Pj)
{
	a &= mask<bits>;
	Pi.x += ((Pj.x-Pi.x) * a) >> bits;
	Pi.y += ((Pj.y-Pi.y) * a) >> bits;
	return Pi;
}
#if XY2_USE_MAP_PICO_INTERP
inline Point2D interpolate_Pico (uint a, Point2D Pi, Point2D Pj)
{
	// The RP2040 interpolator always uses 8 bits.
	// It does not have the 'limit' of the arithmetic interpolate() above
	// because it always uses 8 bit alpha and an intermediate result of 40 bits.
	// the Interpolator must have been setup before: mode, shift, mask etc.

	interp0->accum[1] = a;		// alpha needs only be loaded once for x and y

	interp0->base[0] = Pi.x;
	interp0->base[1] = Pj.x;
	Pi.x = interp0->peek[1];	// lane 1 returns the interpolation

	interp0->base[0] = Pi.y;
	interp0->base[1] = Pj.y;
	Pi.y = interp0->peek[1];

	return Pi;
}
#endif


/*	map an input point to an output point using a 2-dimensional map[,].
	The map[,] must have 1<<map_bits ranges which require 1<<map_bits + 1 values
	in both dimensions.
	The low order bits are interpolated.

	The input coordinates must be in range 0 to 1<<(map_bits+low_order_bits> excl.
	They will be clamped to this range.
*/
template<uint map_bits, uint bits>
Point2D map2D (Point2D p, Point2D map[])
{
	clamp<map_bits+bits>(p);

	uint32 ix = p.x >> bits;
	uint32 iy = p.y >> bits;

	// interpolate P(0,0) - P(1,0)
	map += ix + iy + (iy << map_bits);		// ix + iy*(2^map_bits+1)
	Point2D p1 = interpolate<bits>(p.x, map[0], map[1]);

	// interpolate P(0,1) - P(1,1)
	map += 1 + (1<<map_bits);
	Point2D p2 = interpolate<bits>(p.x, map[0], map[1]);

	// interpolate vertically:
	return interpolate<bits>(p.y, p1, p2);
}
#if XY2_USE_MAP_PICO_INTERP
template<uint map_bits, uint bits>
Point2D map2D_Pico (Point2D p, Point2D map[])
{
	clamp<map_bits+bits>(p);

	uint ix = p.x >> bits;
	uint iy = p.y >> bits;

	// interpolate P(0,0) - P(1,0)
	map += ix + iy + (iy << map_bits);
	Point2D p1 = interpolate_Pico(p.x, map[0], map[1]);

	// interpolate P(0,1) - P(1,1)
	map += 1 + (1<<map_bits);
	Point2D p2 = interpolate_Pico(p.x, map[0], map[1]);

	// interpolate vertically:
	return interpolate_Pico(p.y, p1, p2);
}
#endif



























