// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "cdefs.h"
#include "XY2.h"


//#ifdef XY2_IMPLEMENT_CHECKER_BOARD_DEMO
//#ifdef XY2_IMPLEMENT_ANALOGUE_CLOCK_DEMO
//#ifdef XY2_IMPLEMENT_LISSAJOUS_DEMO


struct LissajousData
{
	FLOAT x_scale, y_scale, x_step, x_angle, y_angle;
	FLOAT f_ratio_center, f_ratio_angle_step, f_ratio_angle, f_ratio_scale;

	LissajousData(FLOAT min_f_ratio, FLOAT max_f_ratio, FLOAT steps_per_rot, FLOAT rots_per_sweep);
};


// run on core0:
extern void drawCheckerBoard (const Rect& bbox, uint count, const LaserSet&);
extern void drawClock (const Rect& bbox, uint32 second, const LaserSet& straight, const LaserSet& rounded);
extern void drawLissajous (FLOAT width, FLOAT height, LissajousData&, const LaserSet&);
