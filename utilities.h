// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#include "standard_types.h"
#include "settings.h"

extern int32 parseInteger(const char* bu, uint& i, int32 min, int32 dflt, int32 max);
extern FLOAT parseFloat(const char* bu, uint& i, FLOAT min, FLOAT dflt, FLOAT max);

template<typename T, typename U, typename V, typename Z>
inline Z map_range (T v, U min1, V max1, Z min2, Z max2)
{
	return min2 + (max2-min2) * Z(v-min1) / Z(max1-min1);
}
