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
#include "pico/multicore.h"
#include "cdefs.h"
#include "XY2.h"
#include "XY2-100.pio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include <string.h>


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

	#undef E
	#undef A
	#undef M
	#undef J
};


#define pio PIO_XY2

//static
uint XY2::heart_beat_counter = 1000;
uint XY2::heart_beat_state   = 0;
Point XY2::pos0{};
LaserQueue laser_queue;    // command queue
Transformation XY2::transformation0;		// transformation used by core0
Transformation XY2::transformation1;		// transformation used by core1
Transformation XY2::transformation_stack[8];// transformation used by core0 and push stack
uint XY2::transformation_stack_index = 0;
static constexpr uint transformation_stack_mask = NELEM(XY2::transformation_stack) - 1;
uint XY2::pwm_slice_num;
int XY2::pwm_underruns;
static uint laser_delay_queue[16] = {0};
static uint laser_delay_size = LASER_QUEUE_DELAY;
static uint laser_delay_index = 0;
static_assert(LASER_QUEUE_DELAY <= NELEM(laser_delay_queue),"");

static volatile bool core1_running = false;   // core1 was started. only ever set.
static volatile bool core1_suspend = false;   // request core1 to suspend & detach from flash
static volatile bool core1_suspended = false; // core1 is suspended & detached from flash

#if XY2_USE_MAP
	Point2D XY2::map[((1<<XY2_MAP_BITS)+1)*((1<<XY2_MAP_BITS)+1)];

	void XY2::init_map (std::function<Point2D(uint x, uint y)> fu)
	{
		static constexpr uint bits = XY2_MAP_BITS;

		Point2D* p = XY2::map;
		for (uint y = 0; y <= 1<<bits; y++)
		for (uint x = 0; x <= 1<<bits; x++)
		{
			*p++ = fu(x,y);
		}
	}

	static void init_map2D()
	{
		// setup 1:1 mapping

		static constexpr uint bits = XY2_LOW_BITS;
		XY2::init_map([](uint x,uint y){return Point2D(x<<bits,y<<bits);});
	}
#endif


// store new value in laser_delay_queue[] and return old value
uint XY2::delayed_laser_value (uint value)
{
	if (laser_delay_index >= laser_delay_size) laser_delay_index = 0;
	std::swap(value,laser_delay_queue[laser_delay_index++]);
	return value;
}

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

#include "vt_vector_font.h"			// int8 vt_font_data[];

static uint vt_font_col1[256];		// index in vt_font_data[]
static int8 vt_font_width[256];		// character print width (including +1 for line width but no spacing)


static void vt_init_vector_font()
{
	for (uint i = 0, c=' '; c<NELEM(vt_font_col1) && i<NELEM(vt_font_data); c++)
	{
		vt_font_col1[c] = i;
		i += 2;						// left-side mask and right-side mask

		int8 width = 0;
		while (vt_font_data[i++] > E)
		{
			while (vt_font_data[i] < E)
			{
				width = max(width,vt_font_data[i]);
				i += 2;
			}
		}
		vt_font_width[c] = width + 1;

		assert(vt_font_data[i-1] == E);
	}
}


FLOAT printWidth (cstr s)
{
	// calculate print width for string
	// as printed by drawing command DrawText

	int width = 0;
	uint8 mask = 0;

	while (uint8 c = uint8(*s++))
	{
		width += vt_font_width[c];
		int8* p = vt_font_data + vt_font_col1[c];
		if (mask & uint8(*p)) width++;	// +1 if glyphs would touch
		mask = uint8(*(p+1));			// remember for next
	}
	return FLOAT(width);
}

void XY2::printText (Point start, FLOAT scale_x, FLOAT scale_y, cstr text, bool centered,
					 const LaserSet& straight, const LaserSet& rounded)
{
	// CMD_PRINT_TEXT, 2*LaserSet, Point, 2*FLOAT, n*char, 0

	if (centered) start.x -= printWidth(text) * scale_x / 2;

	laser_queue.push(CMD_PRINT_TEXT);
	laser_queue.push(&straight);
	laser_queue.push(&rounded);
	laser_queue.push(start);
	laser_queue.push(scale_x);
	laser_queue.push(scale_y);
	char c; do { laser_queue.push(c = *text++); } while (c);
}


void XY2::update_transformation ()
{
	static_assert (sizeof(Data32)==sizeof(FLOAT), "booboo");
	laser_queue.push(transformation0.is_projected ? CMD_SET_TRANSFORMATION_3D : CMD_SET_TRANSFORMATION);
	uint n = transformation0.is_projected ? 9 : 6;
	while (laser_queue.free() < n) { gpio_put(LED_CORE0_IDLE,1); }
	gpio_put(LED_CORE0_IDLE,0);
	Data32* data = reinterpret_cast<Data32*>(&transformation0);
	laser_queue.write(data,n);
}

void XY2::resetTransformation()
{
	transformation0.reset();
	laser_queue.push(CMD_RESET_TRANSFORMATION);
}

void XY2::pushTransformation()
{
	transformation_stack[--transformation_stack_index & transformation_stack_mask] = transformation0;
}

void XY2::popTransformation()
{
	transformation0 = transformation_stack[transformation_stack_index++ & transformation_stack_mask];
	update_transformation();
}

void XY2::setRotation (FLOAT rad)	// and reset scale and shear
{
	transformation0.setRotation(rad);
	update_transformation();
}

void XY2::setScale (FLOAT f)
{
	transformation0.setScale(f);
	update_transformation();
}

void XY2::setScale (FLOAT fx, FLOAT fy)
{
	transformation0.setScale(fx,fy);
	update_transformation();
}

void XY2::setOffset(FLOAT dx, FLOAT dy)
{
	transformation0.setOffset(dx,dy);
	update_transformation();
}

void XY2::setShear (FLOAT sx, FLOAT sy)
{
	transformation0.setShear(sx,sy);
	update_transformation();
}

void XY2::setProjection (FLOAT px, FLOAT py, FLOAT pz)
{
	transformation0.setProjection(px,py,pz);
	update_transformation();
}

void XY2::setRotationAndScale (FLOAT rad, FLOAT fx, FLOAT fy)
{
	transformation0.setRotationAndScale(rad,fx,fy);
	update_transformation();
}

void XY2::setTransformation (const Transformation& transformation)
{
	transformation0 = transformation;
	update_transformation();
}

void XY2::setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy)
{
	new(&transformation0) Transformation(fx,fy,sx,sy,dx,dy);
	update_transformation();
}

void XY2::setTransformation (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy, FLOAT px, FLOAT py, FLOAT pz)
{
	new(&transformation0) Transformation(fx,fy,sx,sy,dx,dy,px,py,pz);
	update_transformation();
}

void XY2::rotate (FLOAT rad)
{
	transformation0.rotate(rad);
	update_transformation();
}

void XY2::rotateAndScale (FLOAT rad, FLOAT fx, FLOAT fy)
{
	transformation0.rotateAndScale(rad,fx,fy);
	update_transformation();
}

void XY2::scale (FLOAT f)
{
	transformation0.scale(f);
	update_transformation();
}

void XY2::scale (FLOAT fx, FLOAT fy)
{
	transformation0.scale(fx,fy);
	update_transformation();
}

void XY2::addOffset(FLOAT dx, FLOAT dy)
{
	transformation0.addOffset(dx,dy);
	update_transformation();
}

void XY2::transform (const Transformation& transformation)
{
	transformation0.addTransformation(transformation);
	update_transformation();
}

void XY2::transform (FLOAT fx, FLOAT fy, FLOAT sx, FLOAT sy, FLOAT dx, FLOAT dy)
{
	transformation0.addTransformation(fx,fy,sx,sy,dx,dy);
	update_transformation();
}


void XY2::start()
{
	if (!core1_running) multicore_launch_core1(XY2::worker);
	while (!core1_running) {}
	printf("core1 up and running\n");
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

	// count underruns:
	// => setup a counter to count pulses on PIN_XY2_SYNC_XY
	// which should be high all the time but is low on fifo empty state.
	// it seems not possible to read back PIN_XY2_SYNC_XY directly:
	const uint gpio = pin_sync_xy_readback;
	// Only PWM B pins (odd pin numbers) can be used as inputs:
	assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
	pwm_slice_num = pwm_gpio_to_slice_num(gpio);

	// configure the PWM for counter mode:
	pwm_config cfg = pwm_get_default_config();
	pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
	pwm_init(pwm_slice_num, &cfg, true);
	gpio_set_function(gpio, GPIO_FUNC_PWM);
	gpio_set_dir(gpio,GPIO_IN);
	pwm_underruns = pwm_get_counter(pwm_slice_num);

	// misc.:
	vt_init_vector_font();
	#if XY2_USE_MAP
	init_map2D();
	#endif
}

void __no_inline_not_in_flash_func(suspend_no_flash) ()
{
	core1_suspended = true;
	do { __wfe(); } while (core1_suspend);
	core1_suspended = false;
}

void XY2::suspend()
{
	core1_suspend = true;
	do { __sev(); } while (!core1_suspended);
}

void XY2::resume()
{
	core1_suspend = false;
	do { __sev(); } while (core1_suspended);
}

void XY2::worker()
{
	while (laser_queue.avail()) (void)laser_queue.pop();

	core1_running = true;
	core1_suspended = false;

	#if XY2_USE_MAP && XY2_USE_MAP_PICO_INTERP
	interp_config cfg = interp_default_config();
	interp_config_set_blend(&cfg, true);
	interp_set_config(interp0, 0, &cfg);				// reset lane 0

	cfg = interp_default_config();
	interp_config_set_shift(&cfg,(XY2_LOW_BITS-8)&31);	// right shift!
	interp_config_set_mask(&cfg,0,7);					// not neccessary
	// note: signed is required, if input values can be both positive and negative.
	// we could always use signed except when using left-aligned full 32 bit unsigned coordinates.
	interp_config_set_signed(&cfg, SCANNER_MIN!=0);
	interp_set_config(interp0, 1, &cfg);
	#endif

	// start the state machines:
	pio_sm_set_enabled(pio, sm_laser, false);
	pio_sm_set_enabled(pio, sm_clock, false);
	pio_sm_set_enabled(pio, sm_x, false);
	pio_sm_set_enabled(pio, sm_y, false);

	pio_sm_clear_fifos(pio, sm_laser);
	pio_sm_clear_fifos(pio, sm_x);
	pio_sm_clear_fifos(pio, sm_y);

	// Enable multiple PIO state machines synchronizing their clock dividers
	pio_enable_sm_mask_in_sync(pio, (1<<sm_clock)+(1<<sm_x)+(1<<sm_y)+(1<<sm_laser));

	// send first point to center and switch off laser
	memset(laser_delay_queue,0,sizeof(laser_delay_queue));
	pio_send_data(FLOAT(0),FLOAT(0), 0x000);

	for(;;)
	{
		if (core1_suspend)
		{
			uint old_state = save_and_disable_interrupts();
			suspend_no_flash();
			restore_interrupts(old_state);
		}

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
		case CMD_PRINT_TEXT: // 	2*LaserSet, Point, 2*FLOAT, n*char, 0
		{
			const LaserSet* straight = laser_queue.pop().set;
			const LaserSet* rounded  = laser_queue.pop().set;
			Point start   = laser_queue.pop_Point();
			FLOAT scale_x = laser_queue.pop().f;
			FLOAT scale_y = laser_queue.pop().f;

			uint8 rmask = 0;
			while (char c = char(laser_queue.pop().u))
			{
				print_char (start, scale_x, scale_y, *straight, *rounded, rmask, c);
			}
			continue;
		}
		case CMD_RESET_TRANSFORMATION:	// --
		{
			transformation1.reset();
			continue;
		}
		case CMD_SET_TRANSFORMATION:		// fx fy sx sy dx dy
		{
			while (laser_queue.avail()<6) {} // __wfi
			Data32* dest = reinterpret_cast<Data32*>(&transformation1);
			laser_queue.read(dest,6);
			transformation1.is_projected = false;
			continue;
		}
		case CMD_SET_TRANSFORMATION_3D:		// fx fy sx sy dx dy px py pz
		{
			while (laser_queue.avail()<9) {} // __wfi
			Data32* dest = reinterpret_cast<Data32*>(&transformation1);
			laser_queue.read(dest,9);
			transformation1.is_projected = true;
			continue;
		}
		}
	}
}


uint16 XY2::getUnderruns()
{
	int d = pwm_get_counter(pwm_slice_num) - pwm_underruns;
	pwm_underruns += d;
	return uint16(d);
}


void __not_in_flash_func(XY2::draw_to) (Point dest, FLOAT speed, uint laser_on_pattern, uint& laser_on_delay, uint end_delay)
{
	// draw line to dest with speed
	// while laser_on_delay > 0 use laser_off_pattern
	// thereafter use laser_on_pattern
	// at end of line wait delay

	transformation1.transform(dest);

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

void XY2::line_to (const Point& dest, const LaserSet& set)
{
	const FLOAT speed = set.speed;
	const uint laser_on_pattern = set.pattern;
	const uint delay = set.delay_e;
	uint laser_on_delay = set.delay_a;
	draw_to(dest,speed,laser_on_pattern,laser_on_delay,delay);
}

void __not_in_flash_func(XY2::draw_polyline) (uint count, std::function<Point()> next_point, const LaserSet& set, uint flags)
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

void XY2::print_char (Point& p0, FLOAT scale_x, FLOAT scale_y, const LaserSet& straight, const LaserSet& rounded, uint8& rmask, char c)
{
	int8* p = vt_font_data + vt_font_col1[uchar(c)];

	uint lmask = uint8(*p++);
	if (rmask & lmask) p0.x += scale_x;		// apply kerning
	rmask = uint8(*p++);					// for next kerning

	while (*p != E)
	{
		int line_type = *p++;

		const LaserSet& set = line_type == L ? straight : rounded;

		Point pt;
		pt.x = p0.x + *p++ * scale_x;
		pt.y = p0.y + *p++ * scale_y;
		move_to(pt);

		uint delay_a = set.delay_a;
		while (*p < E)
		{
			pt.x = p0.x + *p++ * scale_x;
			pt.y = p0.y + *p++ * scale_y;
			draw_to(pt, set.speed, set.pattern, delay_a, *p<E ? set.delay_m : set.delay_e);
		}
	}

	p0.x += vt_font_width[uchar(c)] * scale_x;	// update print position
}



















