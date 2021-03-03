// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "demos.h"
#include "cdefs.h"
#include <math.h>

//static inline FLOAT sin (FLOAT a) { return sinf(a); }
//static inline FLOAT cos (FLOAT a) { return cosf(a); }
static const FLOAT pi = FLOAT(3.1415926538);


#ifdef XY2_IMPLEMENT_CHECKER_BOARD_DEMO
void drawCheckerBoard (const Rect& bbox, uint count, const LaserSet& set) // core0
{
	for (uint i=0; i<=count; i++)
	{
		FLOAT y = bbox.bottom + bbox.height() * FLOAT(i) / FLOAT(count);
		XY2::drawLine(Point(i & 1 ? bbox.right : bbox.left, y), Point(i & 1 ? bbox.left : bbox.right, y), set);
	}
	for (uint i=0; i<=count; i++)
	{
		FLOAT x = bbox.left + bbox.width() * FLOAT(i) / FLOAT(count);
		XY2::drawLine(Point(x, i & 1 ? bbox.top : bbox.bottom), Point(x, i & 1 ? bbox.bottom : bbox.top), set);
	}
}
#endif


#ifdef XY2_IMPLEMENT_ANALOGUE_CLOCK_DEMO
void drawClock (const Rect& bbox, uint32 second_of_day, const LaserSet& straight, const LaserSet& rounded) // core0
{
	assert(bbox.center() == Point(0,0));

	FLOAT second =      (second_of_day %  60);
	FLOAT minute =      (second_of_day % (60*60))    /  60;		// FLOAT=>smooth, no FLOAT=>jump
	FLOAT hour   = FLOAT(second_of_day % (60*60*12)) / (60*60);

	uint i;
	FLOAT rad;
	Point triangle[3];
	const FLOAT radius = bbox.width()/2;

	static constexpr FLOAT
		O_RING_DIA = FLOAT(1.0),	// outer ring diameter
		I_RING_DIA = FLOAT(0.05),	// clock axis diameter
		HM_O_DIA   = FLOAT(0.95),	// hour markers outer diameter
		HM_I_DIA   = FLOAT(0.80),	// hour markers inner diameter
		SH_LENGTH  = FLOAT(0.90), SH_I_DIA = FLOAT(-0.2),	// seconds handle length and inner length
		MH_LENGTH  = FLOAT(0.70), MH_I_DIA = FLOAT(-0.15), MH_WIDTH = FLOAT(0.05), // minute handle
		HH_LENGTH  = FLOAT(0.55), HH_I_DIA = FLOAT(-0.15), HH_WIDTH = FLOAT(0.05); // hour handle

	// draw outer ring:
	XY2::drawEllipse(bbox * O_RING_DIA, pi/2, 12*2, rounded);   // start at 12 o'clock

	// draw full hour markers for 1 to 11 o'clock:
	for (i=1; i<=11; i++)
	{
		rad = pi*2 / 12 * FLOAT(i);
		Point direction { radius * sin(rad), radius * cos(rad) };
		XY2::drawLine(direction * HM_O_DIA, direction * HM_I_DIA, straight);
	}

	// draw hour marker for 12 o'clock: (a triangle)
	FLOAT w = radius * (HM_O_DIA - HM_I_DIA) / 3;	// width/2 of the triangle
	triangle[0] = Point( 0, radius * HM_I_DIA);		// tip
	triangle[1] = Point(-w, radius * HM_O_DIA);
	triangle[2] = Point(+w, radius * HM_O_DIA);
	XY2::drawPolygon(3, triangle, straight);				// start drawing at tip

	// draw the handles starting and ending at the clock center to minimize jumps:

	// draw second handle:
	rad = pi*2 / 60 * second;
	Point direction { radius * sin(rad), radius * cos(rad) };
	//XY2::drawLine(direction * SH_I_DIA,  direction * SH_LENGTH, straight);
	XY2::drawLine(direction * SH_LENGTH, direction * SH_I_DIA, straight);

	// draw minute handle:
	triangle[0] = Point( 0.0,      MH_LENGTH );		// tip
	triangle[1] = Point(-MH_WIDTH, MH_I_DIA);
	triangle[2] = Point(+MH_WIDTH, MH_I_DIA);

	rad = pi*2 / 60 * minute;
	FLOAT sinus = sin(rad) * radius;
	FLOAT cosin = cos(rad) * radius;
	for (i=0; i<3; i++) { triangle[i].rotate_cw(sinus,cosin); }
	XY2::drawPolygon(3,triangle, straight);		// start drawing at base

	// draw hour handle:
	triangle[0] = Point( 0.0,      HH_LENGTH );		// tip
	triangle[1] = Point(-HH_WIDTH, HH_I_DIA);
	triangle[2] = Point(+HH_WIDTH, HH_I_DIA);

	rad = pi*2 / 12 * hour;
	sinus = sin(rad) * radius;
	cosin = cos(rad) * radius;
	for (i=0; i<3; i++) { triangle[i].rotate_cw(sinus,cosin); }
	XY2::drawPolygon(3, triangle, straight);	// start drawing at base

	// draw inner ring: (axis)
	XY2::drawEllipse(bbox * I_RING_DIA, FLOAT(0.0), 8, rounded);
}
#endif


#ifdef XY2_IMPLEMENT_LISSAJOUS_DEMO
void drawLissajous (FLOAT width, FLOAT height, LissajousData& p, const LaserSet& set)
{
	p.x_scale = width / 2;
	p.y_scale = height / 2;

	XY2::drawPolyLine(10000, [&p]()
	{
		p.f_ratio_angle += p.f_ratio_angle_step;
		if (p.f_ratio_angle >= pi) p.f_ratio_angle -= pi*2;
		FLOAT f_ratio = p.f_ratio_center + sin(p.f_ratio_angle) * p.f_ratio_scale;

		p.x_angle += p.x_step;
		if (p.x_angle >= pi) p.x_angle -= pi*2;
		p.y_angle += p.x_step * f_ratio;
		if (p.y_angle >= pi) p.y_angle -= pi*2;

		FLOAT x = sin(p.x_angle) * p.x_scale;
		FLOAT y = cos(p.y_angle) * p.y_scale;

		return Point{x,y};
	},
	set, POLYLINE_INFINITE);
}

LissajousData::LissajousData (FLOAT min_f_ratio, FLOAT max_f_ratio, FLOAT steps_per_rot, FLOAT rots_per_sweep)
{
	steps_per_rot = minmax(4, steps_per_rot, 1000);					// steps for full circle in x direction
	rots_per_sweep = minmax(4, rots_per_sweep, 1000000);			// circles for full rotation min_fy/fx <-> max_fy/fx
	min_f_ratio = minmax(FLOAT(0.5), min_f_ratio, FLOAT(10.05));	// min. fy/fx ratio
	max_f_ratio = minmax(min_f_ratio, max_f_ratio, FLOAT(10.05));	// max. fy/fx ratio

	x_scale = 100;
	y_scale = 100;
	x_step  = 2*pi / steps_per_rot;
	x_angle = 0;
	y_angle = 0;
	f_ratio_center = sqrt(min_f_ratio * max_f_ratio);
	f_ratio_angle_step = 2*pi / steps_per_rot / rots_per_sweep;
	f_ratio_angle = 0;
	f_ratio_scale = (max_f_ratio-min_f_ratio)/2;
}
#endif

