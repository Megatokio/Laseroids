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

