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

