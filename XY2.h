// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once

#include "hardware/pio.h"
#include "cdefs.h"
#include "basic_geometry.h"
#include <functional>
#include "Queue.h"
#include "pico/multicore.h"
#if XY2_USE_MAP
#include "Interpolator.h"
#endif


struct LaserSet
{
	FLOAT speed; 	// max. dist/step
	uint pattern;	// line stipple
	uint delay_a;	// steps to wait after start before laser ON
	uint delay_m;	// steps to wait at middle points in polygon lines
	uint delay_e;	// steps to wait with laser ON after end of line
};
extern LaserSet laser_set[8]; // application defined parameter sets, set 0 is used for jump
static constexpr LaserSet& fast_straight = laser_set[1];	// preset with static initializer
static constexpr LaserSet& slow_straight = laser_set[2];	// ""
static constexpr LaserSet& fast_rounded  = laser_set[3];	// ""
static constexpr LaserSet& slow_rounded  = laser_set[4];	// ""


enum DrawCmd
{
	CMD_END = 0,	// arguments:
	CMD_MOVETO,		// Point
	CMD_DRAWTO,		// LaserSet, Point		as with no_start and delay_m
	CMD_LINETO,		// LaserSet, Point		uses delay_a and delay_e
	CMD_LINE,       // LaserSet, 2*Point
	CMD_RECT,       // LaserSet, Rect
	CMD_POLYLINE,   // LaserSet, flags, n, n*Point
	CMD_PRINT_TEXT,	// 2*LaserSet, Point, 2*FLOAT, n*char, 0

	CMD_RESET_TRANSFORMATION,	// --
	CMD_SET_TRANSFORMATION,		// fx fy sx sy dx dy
	CMD_SET_TRANSFORMATION_3D,	// fx fy sx sy dx dy px py pz
};
union Data32
{
	DrawCmd cmd;
	const LaserSet* set;
	FLOAT f;
	uint  u;
	int   i;
	Data32(DrawCmd c) : cmd(c){}
	Data32(FLOAT f)   : f(f)  {}
	Data32(uint  u)   : u(u)  {}
	Data32(int   i)   : i(i)  {}
	Data32(const LaserSet* s) : set(s) {}
	Data32(){}
	~Data32(){}
};

enum PolyLineOptions
{
	POLYLINE_DEFAULT  = 0,
	POLYLINE_NO_START = 1,
	POLYLINE_NO_END   = 2,
	POLYLINE_INFINITE = 3,	// no start and no end
	POLYLINE_CLOSED   = 4
};

class LaserQueue : public Queue<Data32,256>
{
	// Note:
	// push only on core 0
	// pop only on core 1

public:
	void push (Data32 data)
	{
		while (!free()) { gpio_put(LED_CORE0_IDLE,1); }
		gpio_put(LED_CORE0_IDLE,0);
		putc(data);
	}

	void push (const Point& p)
	{
		push(p.x);
		push(p.y);
	}

	void push (const Rect& r)
	{
		push(r.top_left());
		push(r.bottom_right());
	}

	Data32 pop ()
	{
		while (!avail()) {}		// __wfe()
		return getc();
	}

	Point pop_Point ()
	{
		FLOAT x = pop().f;
		FLOAT y = pop().f;
		return Point(x,y);
	}

	Rect pop_Rect ()
	{
		Point p1 = pop_Point();
		Point p2 = pop_Point();
		return Rect(p1,p2);
	}
};

extern LaserQueue laser_queue;    // command queue core0 -> core1


class XY2
{
public:
	// the pio to use:
	// can't use a constexpr because of static cast addr -> ptr
	#define pio PIO_XY2

	// assignment of state machines:
	static constexpr uint sm_laser  = 0;
	static constexpr uint sm_clock  = 1;
	static constexpr uint sm_x      = 2;
	static constexpr uint sm_y      = 3;

	// output pins of the XY2 interface:
	static constexpr uint pin_laser = PIN_XY2_LASER;
	static constexpr uint pin_clock = PIN_XY2_CLOCK;
	static constexpr uint pin_sync  = PIN_XY2_CLOCK + 2;
	static constexpr uint pin_x     = PIN_XY2_X;
	static constexpr uint pin_y     = PIN_XY2_Y;
	static constexpr uint pin_sync_xy = PIN_XY2_SYNC_XY;

	// indicators:
	static constexpr uint led_heartbeat  = LED_BEAT;
	static constexpr uint led_error      = LED_ERROR;
	static constexpr uint led_core0_idle = LED_CORE0_IDLE;
	static constexpr uint led_core1_idle = LED_CORE1_IDLE;

	// monitoring:
	static constexpr uint pin_sync_xy_readback = PIN_XY2_SYNC_XY_READBACK;
	static uint pwm_slice_num;
	static int  pwm_underruns;

	static uint heart_beat_counter;
	static uint heart_beat_state;

	#if XY2_USE_MAP
	static Point2D map[];
	#endif

	static Point pos0;		// current scanner position (after transformation)

	static Transformation transformation0;			// transformation used by core0 (App)
	static Transformation transformation1;			// transformation used by core1 (scanner backend)
	static Transformation transformation_stack[];	// push stack maintained by core0
	static uint transformation_stack_index;

	XY2(){}
	static void init();
	static void start();
	static void suspend();
	static void resume();
	#if XY2_USE_MAP
	// setup distortion correction map
	// function is called from nested loops:
	//		for(y=0…1<<map_bits)for(x=0…1<<map_bits){fu(x,y)}
	//		returned coordinates must be in range SCANNER_MIN to SCANNER_MAX
	//		except for x=1<<map_bits and y=1<<max_bits which may by SCANNER_MAX + 1
	static void init_map(std::function<Point2D(uint x, uint y)> fu);
	#endif

	// core0: push to laser_queue:
	static void moveTo (const Point& dest);
	static void drawTo (const Point& dest, const LaserSet&);
	static void drawLine (const Point& start, const Point& dest, const LaserSet&);
	static void drawRect (const Rect& rect, const LaserSet&);
	static void drawEllipse (const Rect& bbox, FLOAT angle0, uint steps, const LaserSet&);
	static void drawPolyLine (uint count, std::function<Point()> nextPoint, const LaserSet&, PolyLineOptions=POLYLINE_DEFAULT);
	static void drawPolyLine (uint count, const Point points[], const LaserSet&, PolyLineOptions=POLYLINE_DEFAULT);
	static void drawPolygon (uint count, std::function<Point()> nextPoint, const LaserSet&);
	static void drawPolygon (uint count, const Point points[], const LaserSet&);
	static void printText (Point start, FLOAT scale_x, FLOAT scale_y, cstr text, bool centered = false,
						   const LaserSet& = slow_straight, const LaserSet& = slow_rounded);

	static void resetTransformation();
	static void pushTransformation();
	static void popTransformation();

	static void setRotation (FLOAT rad);
	static void setRotationAndScale (FLOAT rad, FLOAT fx, FLOAT fy);
	static void setScale (FLOAT f);
	static void setScale (FLOAT fx, FLOAT fy);
	static void setOffset(FLOAT dx, FLOAT dy);
	static void setOffset(const Point& p) { setOffset(p.x,p.y); }
	static void setOffset(const Dist& d)  { setOffset(d.dx,d.dy); }
	static void setShear (FLOAT sx, FLOAT sy);
	static void setProjection (FLOAT px, FLOAT py, FLOAT pz=1);
	static void setTransformation (const Transformation& transformation);
	static void setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy);
	static void setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy, FLOAT px, FLOAT py, FLOAT pz=1);

	static void rotate (FLOAT rad);
	static void rotateAndScale (FLOAT rad, FLOAT fx, FLOAT fy);
	static void scale (FLOAT f);
	static void scale (FLOAT fx, FLOAT fy);
	static void addOffset (FLOAT dx, FLOAT dy);
	static void addOffset (const Point& p) { addOffset(p.x,p.y); }
	static void addOffset (const Dist& d)  { addOffset(d.dx,d.dy); }
	static void transform (const Transformation& transformation);
	static void transform (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy);

	// Monitoring:
	static uint16 getUnderruns();	// since last call


private:
	static void worker();		// on core1

	static void update_transformation ();
	static uint delayed_laser_value (uint value);

	static uint pio_avail() { return pio_sm_get_tx_fifo_level(pio,sm_x); }
	static uint pio_free() { return 8 - pio_sm_get_tx_fifo_level(pio,sm_x);}

	static void pio_wait_free ()
	{
		// we must test the X (or Y) data fifo, not the laser fifo,
		// because they may be filled differently because they are
		// not read at the same time by the SMs!

		if (pio_sm_is_tx_fifo_full(pio,sm_x))
		{
			gpio_put(led_core1_idle,1);
			while (pio_sm_is_tx_fifo_full(pio,sm_x)) {}
			gpio_put(led_core1_idle,0);
		}
		else if (pio_sm_is_tx_fifo_empty(pio,sm_x))    // TODO: optional
		{
			// fifo underrun can be seen directly if a led is attached to pin_sync_xy
			// which faintly flashes.
			// toggling the on-board led improves visibility of underruns:
			//gpio_xor_mask(1<<LED_PIN);
			gpio_put(LED_PIN,1);
		}
	}

	static void pio_send_data (FLOAT x, FLOAT y, uint32 laser)
	{
		pos0.x = x;
		pos0.y = y;
		laser = delayed_laser_value(laser);

	#if XY2_USE_MAP
		Point2D p{uint32(0x8000 + int32(x)), uint32(0x8000 - int32(y))};
	  #if XY2_USE_MAP_PICO_INTERP
		p = map2D_Pico<XY2_MAP_BITS,XY2_LOW_BITS>(p,map);
	  #else
		p = map2D<XY2_MAP_BITS,XY2_LOW_BITS>(p,map);
	  #endif
		pio_sm_put(pio, sm_x, p.x);
		pio_sm_put(pio, sm_y, p.y);
		pio_sm_put(pio, sm_laser, laser);
	#else
		uint32 ix = 0x8000 + int32(x);
		uint32 iy = 0x8000 - int32(y);
		if (ix>>16) ix = int32(ix)<0 ? 0 : 0xffff;
		if (iy>>16) iy = int32(iy)<0 ? 0 : 0xffff;
		pio_sm_put(pio, sm_x, ix);
		pio_sm_put(pio, sm_y, iy);
		pio_sm_put(pio, sm_laser, laser);
	#endif

		if (--heart_beat_counter == 0)
		{
			heart_beat_counter = XY2_DATA_CLOCK / 2;    // => 1 Hz
			heart_beat_state = !heart_beat_state;
			gpio_put(led_heartbeat, heart_beat_state);
		}
	}

	static void send_data_blocking (const Point& p, uint32 laser)
	{
		pio_wait_free();
		pio_send_data(p.x,p.y,laser);
	}

	static void draw_to (Point dest, FLOAT speed, uint laser_on_pattern, uint& laser_on_delay, uint end_delay);
	static void move_to (const Point& dest) { line_to(dest,laser_set[0]); }
	static void line_to (const Point& dest, const LaserSet&);
	static void draw_line (const Point& start, const Point& dest, const LaserSet&);
	static void draw_rect (const Rect& rect, const LaserSet&);
	static void draw_polyline (uint count, std::function<Point()> readNextPoint, const LaserSet&, uint options);
	static void print_char (Point& textpos, FLOAT scale_x, FLOAT scale_y, const LaserSet& straight, const LaserSet& rounded, uint8& rmask, char c);
};




#undef pio	// PIO_XY




















