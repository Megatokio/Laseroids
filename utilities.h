// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#include "standard_types.h"
#include "settings.h"
#include <pico/stdlib.h>
#include <stdlib.h>			// rand()


extern int32 parseInteger(const char* bu, uint& i, int32 min, int32 dflt, int32 max);
extern FLOAT parseFloat(const char* bu, uint& i, FLOAT min, FLOAT dflt, FLOAT max);

template<typename T, typename U, typename V, typename Z>
inline Z map_range (T v, U min1, V max1, Z min2, Z max2)
{
	return min2 + (max2-min2) * Z(v-min1) / Z(max1-min1);
}

template<uint iter=0>
inline float q3_reverse_sqrt (float x)
{
	// the Quake III algorithm for 1/sqrt(x)
	// finds the first approximation for sqrt(x)
	// by simply shifting the exponent of the float right by 1
	// and the 1/sqrt(x) by taking it's negative.

	// it only works on processors which do the INTs and the FLOATs like Intel:
	// little endian, use complements for negative INTs and IEEE754 4-byte floats.

	// Speed test on Raspberry Pico, compared to built-in 1.0f/sqrt(x):
	// 0 Iter:   6% execution time, accuracy ~3%
	// 1 Iter: 256% execution time, accuracy ~0.15%
	// 2 Iter: 476% execution time, accuracy ~0.0005%
	// 3 Iter: 676% execution time, accuracy ~0.000015%
	// -->
	// no need to use this except for a very fast first estimation!

	using iptr = int*;
	using fptr = float*;

	float x2 = x * 0.5f;		// x/2 needed for Newton's
	float y = x;				// y for the result

	int32 i;
	i = *iptr(&y);				// approximating:
	i = 0x5f3759df - (i>>1);	// log(1/sqrt(x)) = -1/2*log(x)
	y = *fptr(&i);				// 0x5f3759df mostly corrects offsets

	for (uint i=0; i<iter; i++)
		y *= 1.5f - x2*y*y;		// Newton's Iteration

	return y;
}


inline uint rand (uint max)	// --> [0 .. max]
{
	uint mask = 0xffffffffu >> __builtin_clz(max);

	for(;;)
	{
		uint n = uint(rand()) & mask;
		if (n <= max) return n;
	}
}

inline int rand (int min, int max)	// --> [min .. max]
{
	return min + int(rand(uint(max - min)));
}

inline FLOAT rand (FLOAT max)
{
	return FLOAT(rand()) * max / FLOAT(RAND_MAX);
}

inline FLOAT rand (FLOAT min, FLOAT max)
{
	return min + rand(max-min);
}

