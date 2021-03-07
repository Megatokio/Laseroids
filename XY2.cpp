// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
//#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
//#include "pico/multicore.h"
#include "cdefs.h"
#include "XY2.h"
#include "XY2-100.pio.h"


//static inline FLOAT sin (FLOAT a) { return sinf(a); }
//static inline FLOAT cos (FLOAT a) { return cosf(a); }
static const FLOAT pi = FLOAT(3.1415926538);


// application defined parameter sets, set 0 is used for jump
#define FAST SCANNER_MAX_SPEED
#define SLOW SCANNER_MAX_SPEED*2/3
LaserSet laser_set[8] =
{
	#define A LASER_ON_DELAY
	#define E LASER_OFF_DELAY
	#define M LASER_MIDDLE_DELAY
	#define J LASER_JUMP_DELAY

	LaserSet{.speed=FAST, .pattern=0x003, .delay_a=0, .delay_m=0, .delay_e=J},	// jump
	LaserSet{.speed=FAST, .pattern=0x3FF, .delay_a=A, .delay_m=M, .delay_e=E},	// fast straight
	LaserSet{.speed=SLOW, .pattern=0x3FF, .delay_a=A, .delay_m=M, .delay_e=E},	// slow straight
	LaserSet{.speed=FAST, .pattern=0x3FF, .delay_a=A, .delay_m=0, .delay_e=E},	// fast rounded
	LaserSet{.speed=SLOW, .pattern=0x3FF, .delay_a=A, .delay_m=0, .delay_e=E},	// slow rounded
};


#define pio PIO_XY2

//static
uint XY2::heart_beat_counter = 1000;
uint XY2::heart_beat_state   = 0;
Point XY2::pos0{};
LaserQueue laser_queue;    // command queue
Transformation XY2::transformation;

static bool core1_running = false;
#define c2c_syncflag 0xf287affe



// core0: push to laser_queue:
void XY2::moveTo (const Point& p)
{
	laser_queue.push(CMD_MOVETO);
	laser_queue.push(p);
}

void XY2::drawTo (const Point& p, const LaserSet& set)
{
	laser_queue.push(CMD_DRAWTO);
	laser_queue.push(&set);
	laser_queue.push(p);
}

void XY2::drawLine (const Point& p1, const Point& p2, const LaserSet& set)
{
	laser_queue.push(CMD_LINE);
	laser_queue.push(&set);
	laser_queue.push(p1);
	laser_queue.push(p2);
}

void XY2::drawRect (const Rect& rect, const LaserSet& set)
{
	laser_queue.push(CMD_RECT);
	laser_queue.push(&set);
	laser_queue.push(rect);
}

void XY2::drawEllipse (const Rect& bbox, FLOAT angle, uint steps, const LaserSet& set)
{
	Point center = bbox.center();
	FLOAT fx = bbox.width()/2;
	FLOAT fy = bbox.height()/2;

	FLOAT step = 2*pi / FLOAT(steps);

	drawPolyLine(steps,[center,fx,fy,step,&angle]()
	{
		FLOAT a = angle;
		angle += step;
		return center + Dist(fx*cos(a),fy*sin(a));
	},
	set, POLYLINE_CLOSED);
}

void XY2::drawPolyLine (uint count, std::function<Point()> nextPoint, const LaserSet& set,
						PolyLineOptions flags)
{
	laser_queue.push(CMD_POLYLINE);
	laser_queue.push(&set);
	laser_queue.push(flags);
	laser_queue.push(count);
	for (uint i=0; i<count; i++) laser_queue.push(nextPoint());
}

void XY2::drawPolyLine (uint count, const Point points[], const LaserSet& set,
						PolyLineOptions flags)
{
	laser_queue.push(CMD_POLYLINE);
	laser_queue.push(&set);
	laser_queue.push(flags);
	laser_queue.push(count);
	for (uint i=0; i<count; i++) laser_queue.push(points[i]);
}

void XY2::drawPolygon (uint count, std::function<Point()> nextPoint, const LaserSet& set)
{
	drawPolyLine(count,nextPoint,set,POLYLINE_CLOSED);
}

void XY2::drawPolygon (uint count, const Point points[], const LaserSet& set)
{
	drawPolyLine(count,points,set,POLYLINE_CLOSED);
}

void XY2::setRotationCW (FLOAT rad)
{
	laser_queue.push(CMD_SET_ROTATION_CW);
	laser_queue.push(rad);
}

void XY2::setScale (FLOAT f)
{
	laser_queue.push(CMD_SET_SCALE);
	laser_queue.push(f);
	laser_queue.push(f);
}

void XY2::setScale (FLOAT fx, FLOAT fy)
{
	laser_queue.push(CMD_SET_SCALE);
	laser_queue.push(fx);
	laser_queue.push(fy);
}

void XY2::setOffset (FLOAT dx, FLOAT dy)
{
	laser_queue.push(CMD_SET_OFFSET);
	laser_queue.push(dx);
	laser_queue.push(dy);
}

void XY2::setShear (FLOAT sx, FLOAT sy)
{
	laser_queue.push(CMD_SET_SHEAR);
	laser_queue.push(sx);
	laser_queue.push(sy);
}

void XY2::setProjection (FLOAT px, FLOAT py, FLOAT pz)
{
	laser_queue.push(CMD_SET_PROJECTION);
	laser_queue.push(px);
	laser_queue.push(py);
	laser_queue.push(pz);
}

void XY2::setScaleAndRotationCW (FLOAT fx, FLOAT fy, FLOAT rad)
{
	laser_queue.push(CMD_SET_SCALEandROTATION_CW);
	laser_queue.push(fx);
	laser_queue.push(fy);
	laser_queue.push(rad);
}

void XY2::resetTransformation()
{
	laser_queue.push(CMD_RESET_TRANSFORMATION);
}

void XY2::setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy)
{
	laser_queue.push(CMD_SET_TRANSFORMATION);
	laser_queue.push(fx);
	laser_queue.push(fy);
	laser_queue.push(sx);
	laser_queue.push(sy);
	laser_queue.push(dx);
	laser_queue.push(dy);
}

void XY2::setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy,
							 FLOAT px, FLOAT py, FLOAT pz)
{
	laser_queue.push(CMD_SET_TRANSFORMATION_3D);
	laser_queue.push(fx);
	laser_queue.push(fy);
	laser_queue.push(sx);
	laser_queue.push(sy);
	laser_queue.push(dx);
	laser_queue.push(dy);
	laser_queue.push(px);
	laser_queue.push(py);
	laser_queue.push(pz);
}

void XY2::setTransformation (const Transformation& t)
{
	laser_queue.push(CMD_SET_TRANSFORMATION_3D);
	laser_queue.push(t.fx);
	laser_queue.push(t.fy);
	laser_queue.push(t.sx);
	laser_queue.push(t.sy);
	laser_queue.push(t.dx);
	laser_queue.push(t.dy);
	laser_queue.push(t.px);
	laser_queue.push(t.py);
	laser_queue.push(t.pz);
}


void XY2::start_core1()
{
	if (core1_running) return;
	core1_running = true;

	multicore_launch_core1(XY2::worker);
	uint32 f = multicore_fifo_pop_blocking();
	if (f != c2c_syncflag) { printf("core%i: wrong flag\n",0); for(;;); }
	multicore_fifo_push_blocking(c2c_syncflag);
	printf("core1 up and running\n");
}


void XY2::worker()
{
	multicore_fifo_push_blocking(c2c_syncflag);
	uint32 f = multicore_fifo_pop_blocking();
	if (f != c2c_syncflag) { printf("core%i: wrong flag\n",1); for(;;); }

	pio_send_data(FLOAT(0),FLOAT(0), 0x000);

	for(;;)
	{
		DrawCmd cmd = laser_queue.pop().cmd;

		switch(cmd)
		{
		default:
		{
			gpio_put(led_error,1);
			for(;;);
		}
		case CMD_END:
		{
			return;
		}
		case CMD_MOVETO:	// Point
		{
			Point p1 = laser_queue.pop_Point();
			move_to(p1);
			continue;
		}
		case CMD_DRAWTO:	// LaserSet, Point
		{
			const LaserSet* set = laser_queue.pop().set;
			Point p1 = laser_queue.pop_Point();
			uint laser_on_delay=0;
			draw_to(p1,set->speed,set->pattern,laser_on_delay,set->delay_m);
			continue;
		}
		case CMD_LINETO:	// LaserSet, Point
		{
			const LaserSet* set = laser_queue.pop().set;
			Point p1 = laser_queue.pop_Point();
			line_to(p1,*set);
			continue;
		}
		case CMD_LINE:      // LaserSet, 2*Point
		{
			const LaserSet* set = laser_queue.pop().set;
			Point p1 = laser_queue.pop_Point();
			Point p2 = laser_queue.pop_Point();
			draw_line(p1,p2,*set);
			continue;
		}
		case CMD_RECT:      // LaserSet, Rect
		{
			const LaserSet* set = laser_queue.pop().set;
			Rect rect = laser_queue.pop_Rect();
			draw_rect(rect,*set);
			continue;
		}
		case CMD_POLYLINE:  // LaserSet, flags, n, n*Point
		{
			const LaserSet* set = laser_queue.pop().set;
			uint flags = laser_queue.pop().u;
			uint count = laser_queue.pop().u;
			draw_polyline(count, [](){return laser_queue.pop_Point();}, *set, flags);
			continue;
		}
		case CMD_SET_ROTATION_CW:		// rad
		{
			FLOAT rad = laser_queue.pop().f;
			transformation.setRotationCW(rad);
			continue;
		}
		case CMD_SET_SCALE:				// fx, fy
		{
			FLOAT fx = laser_queue.pop().f;
			FLOAT fy = laser_queue.pop().f;
			transformation.setScale(fx,fy);
			continue;
		}
		case CMD_SET_OFFSET:			// dx, dy
		{
			FLOAT dx = laser_queue.pop().f;
			FLOAT dy = laser_queue.pop().f;
			transformation.setShift(dx,dy);
			continue;
		}
		case CMD_SET_SHEAR:				// sx, sy
		{
			FLOAT sx = laser_queue.pop().f;
			FLOAT sy = laser_queue.pop().f;
			transformation.setShear(sx,sy);
			continue;
		}
		case CMD_SET_PROJECTION:		// px, py, pz
		{
			FLOAT px = laser_queue.pop().f;
			FLOAT py = laser_queue.pop().f;
			FLOAT pz = laser_queue.pop().f;
			transformation.setProjection(px,py,pz);
			continue;
		}
		case CMD_SET_SCALEandROTATION_CW:// fx, fy, rad
		{
			FLOAT fx = laser_queue.pop().f;
			FLOAT fy = laser_queue.pop().f;
			FLOAT rad = laser_queue.pop().f;
			transformation.setScaleAndRotationCW(fx,fy,rad);
			continue;
		}
		case CMD_SET_TRANSFORMATION:	// fx fy sx sy dx dy
		{
			FLOAT fx = laser_queue.pop().f;
			FLOAT fy = laser_queue.pop().f;
			FLOAT sx = laser_queue.pop().f;
			FLOAT sy = laser_queue.pop().f;
			FLOAT dx = laser_queue.pop().f;
			FLOAT dy = laser_queue.pop().f;
			transformation.set(fx,fy,sx,sy,dx,dy);
			continue;
		}
		case CMD_SET_TRANSFORMATION_3D:	// fx fy sx sy dx dy px py pz
		{
			FLOAT fx = laser_queue.pop().f;
			FLOAT fy = laser_queue.pop().f;
			FLOAT sx = laser_queue.pop().f;
			FLOAT sy = laser_queue.pop().f;
			FLOAT dx = laser_queue.pop().f;
			FLOAT dy = laser_queue.pop().f;

			FLOAT px = laser_queue.pop().f;
			FLOAT py = laser_queue.pop().f;
			FLOAT pz = laser_queue.pop().f;
			transformation.set(fx,fy,sx,sy,dx,dy, px,py,pz);
			continue;
		}
		case CMD_RESET_TRANSFORMATION:	// --
			{
				transformation.reset();
				continue;
			}
		}
	}
}


void XY2::init()
{
	// initialize pio and state machines

	// initialize LEDs:
	gpio_init(led_heartbeat);  gpio_set_dir(led_heartbeat, GPIO_OUT);  gpio_put(led_heartbeat,0);
	gpio_init(led_core0_idle); gpio_set_dir(led_core0_idle, GPIO_OUT); gpio_put(led_core0_idle,0);
	gpio_init(led_core1_idle); gpio_set_dir(led_core1_idle, GPIO_OUT); gpio_put(led_core1_idle,0);
	gpio_init(led_error); 	   gpio_set_dir(led_error, GPIO_OUT);	   gpio_put(led_error,0);


	// initialize pio:
	// for some reason this is done using a state machine:

	// set initial pin states in the pio:
	// set all to 1 except laser:
	uint mask  = (1<<pin_laser)+(1<<pin_sync_xy)+(3<<pin_clock)+(3<<pin_sync)+(3<<pin_x)+(3<<pin_y);
	uint value = ~(1u<<pin_laser);
	pio_sm_set_pins_with_mask(pio, sm_x, value, mask);

	// set i/o direction for pins in the pio:
	// set all to output:
	pio_sm_set_pindirs_with_mask(pio, sm_x, -1u, mask);

	// set gpio to use pio output for pins:
	// for some reason there is no convenience method:
	for(uint i=0;i<32;i++) { if (mask & (1<<i)) pio_gpio_init(pio, i); }


	// initialize state machines:

	// load CLOCK program into pio:
	uint offset = pio_add_program(pio, &xy2_clock_program);

	// configure the SM:
	pio_sm_config c = xy2_clock_program_get_default_config(offset);
	sm_config_set_sideset_pins(&c, pin_clock);
	sm_config_set_clkdiv(&c, float(MAIN_CLOCK) / XY2_SM_CLOCK);
	pio_sm_init(pio, sm_clock, offset + xy2_clock_offset_start, &c);

	// load DATA program into pio:
	offset = pio_add_program(pio, &xy2_data_program);

	// configure state machines:
	c = xy2_data_program_get_default_config(offset);
	sm_config_set_clkdiv(&c, float(MAIN_CLOCK) / XY2_SM_CLOCK);
	sm_config_set_out_shift(&c, false/*shift left not right*/, false /*!autopull*/, 32 /*pull_threshold*/);
	sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
	sm_config_set_jmp_pin(&c, pin_sync_xy);

	sm_config_set_sideset_pins(&c, pin_x);      // pins for 'side setting' of x+ and x-
	pio_sm_init(pio, sm_x, offset + xy2_data_offset_start, &c);

	sm_config_set_sideset_pins(&c, pin_y);      // pins for 'side setting' of y+ and y-
	pio_sm_init(pio, sm_y, offset + xy2_data_offset_start, &c);

	// load LASER program into pio:
	offset = pio_add_program(pio, &xy2_laser_program);

	// configure state machines:
	c = xy2_laser_program_get_default_config(offset);
	sm_config_set_clkdiv(&c, float(MAIN_CLOCK) / XY2_SM_CLOCK);
	sm_config_set_out_shift(&c, true/*shift right*/, false /*!autopull*/, 10 /*pull_threshold*/);
	sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
	sm_config_set_mov_status(&c, STATUS_TX_LESSTHAN, 1);	// for MOV X,STATUS
	sm_config_set_out_pins(&c, pin_laser, 1);        		// pin used to shift out laser data
	sm_config_set_sideset_pins(&c, pin_sync_xy);      		// pin used for fifo synchronization
	pio_sm_init(pio, sm_laser, offset + xy2_laser_offset_start, &c);


	// final configuration of GPIOs:
	gpio_set_outover(pin_laser,GPIO_OVERRIDE_INVERT); 		// invert output: '1' = ON => pin low
}


void XY2::start()
{
	pio_sm_set_enabled(pio, sm_laser, false);
	pio_sm_set_enabled(pio, sm_clock, false);
	pio_sm_set_enabled(pio, sm_x, false);
	pio_sm_set_enabled(pio, sm_y, false);

	pio_sm_clear_fifos(pio, sm_laser);
	pio_sm_clear_fifos(pio, sm_x);
	pio_sm_clear_fifos(pio, sm_y);

	// Enable multiple PIO state machines synchronizing their clock dividers
	pio_enable_sm_mask_in_sync(pio, (1<<sm_clock)+(1<<sm_x)+(1<<sm_y)+(1<<sm_laser));

	assert(!core1_running);
	assert(laser_queue.avail() == 0);
	start_core1();
}


void XY2::draw_to (Point dest, FLOAT speed, uint laser_on_pattern, uint& laser_on_delay, uint end_delay)
{
	// draw line to dest with speed
	// while laser_on_delay > 0 use laser_off_pattern
	// thereafter use laser_on_pattern
	// at end of line wait delay

	dest.transform(transformation);

	Dist dist = dest - pos0;
	FLOAT line_length = dist.length();			// SQRT
	Dist step = dist * (speed / line_length);

	if (laser_on_delay)
	{
		uint laser_off_pattern = laser_set[0].pattern;

		while (line_length > speed)
		{
			line_length -= speed;
			send_data_blocking(pos0+step, laser_off_pattern);
			if (--laser_on_delay == 0) goto a;
		}

		if (pos0 != dest)
		{
			send_data_blocking(dest, laser_off_pattern);
			if (--laser_on_delay == 0) goto c;
		}

		while (end_delay--)
		{
			send_data_blocking(dest, laser_off_pattern);
			if (--laser_on_delay == 0) goto c;
		}
	}
	else
	{
		a: while (line_length > speed)
		{
			line_length -= speed;
			send_data_blocking(pos0+step, laser_on_pattern);
		}

		if (pos0 != dest)
		{
			send_data_blocking(dest, laser_on_pattern);
		}

		c: while (end_delay--)
		{
			send_data_blocking(dest, laser_on_pattern);
		}
	}
}

void XY2::line_to (Point dest, const LaserSet& set)
{
	const FLOAT speed = set.speed;
	const uint laser_on_pattern = set.pattern;
	const uint delay = set.delay_e;
	uint laser_on_delay = set.delay_a;
	draw_to(dest,speed,laser_on_pattern,laser_on_delay,delay);
}

void XY2::draw_polyline (uint count, std::function<Point()> next_point, const LaserSet& set, uint flags)
{
	// draw polygon or polyline with 'count' points
	//
	// support for drawing long lines in chunks:
	// if flags = no_start then resume drawing previous line without jump to first point
	// if flags = no_end   then don't wait for line end but for middle point only after last point
	// if flags = closed   then draw closing line for a polygon


	bool closed   = flags == POLYLINE_CLOSED;	// => POLYGON
	bool no_start = flags & POLYLINE_NO_START;
	bool no_end   = flags & POLYLINE_NO_END;

	// get starting point and jump to it:
	Point start;
	if (!no_start && count--) { start = next_point(); move_to(start); }
	if (count == 0) return;

	uint laser_on_delay = no_start ? 0 : set.delay_a;
	const FLOAT speed = set.speed;
	const uint laser_on_pattern  = set.pattern;
	uint delay = set.delay_m;

	while (count--)
	{
		Point dest = next_point();

		if (count==0 && !no_end) delay = set.delay_e;
		draw_to(dest,speed,laser_on_pattern,laser_on_delay,delay);
	}

	if (closed) draw_to(start,speed,laser_on_pattern,laser_on_delay,set.delay_e);
}

void XY2::draw_line (const Point& start, const Point& dest, const LaserSet& set)
{
	move_to(start);
	line_to(dest,set);
}

void XY2::draw_rect (const Rect& bbox, const LaserSet& set)
{
	move_to(bbox.top_left());
	line_to(bbox.top_right(), set);
	line_to(bbox.bottom_right(), set);
	line_to(bbox.bottom_left(), set);
	line_to(bbox.top_left(), set);
}



















