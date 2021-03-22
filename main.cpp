// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "XY2-100.pio.h"
#include "cdefs.h"
#include "XY2.h"
#include "utilities.h"
#include "demos.h"
extern "C" {
#include "hardware/rtc.h"
#include <pico/bootrom.h>
}
//#include "pico/util/datetime.h"
//#include "hardware/divider.h"
//#include "pico/divider.h"
#include "hardware/pwm.h"
#include "Laseroids.h"
#include "OledDisplay.h"
#include "AdcLoadSensor.h"
#include "FlashDrive.h"
#include "HiScore.h"

const FLOAT pi = FLOAT(3.1415926538);
static HiScores hiscores;


int main()
{
	stdio_init_all();
	gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT); gpio_put(LED_PIN,0);

	printf("\nLasteroids - Asteroids on LaserScanner\n");
	//while(getchar()!=13);

	printf("init OLED display\n");
	oled.init();

	printf("init ADC\n");
	load_sensor.init();

	printf("read hiscores\n");
	uint err = hiscores.readFromFlash();
	if(err) printf("failed with error %u\n",err);
	else hiscores.print();


	char charbuffer[256] = "";
	printf("please enter size (%%), HH:MM:SS, YYYY/MM/DD (all optional)\n> ");
	uint i=0;
	while(getchar_timeout_us(0)>=0){}
	for(;;)
	{
		int c = getchar_timeout_us(60*1000*1000);
		if (c<0 || c==13) break;
		if (c=='@')	{ reset_usb_boot(1<<25,0); }
		if ((c==8 || c==127) && i>0) { printf("\x08 \x08"); i--; continue; }
		if (i<NELEM(charbuffer)-1) { putchar(c); charbuffer[i++] = char(c); }
	}
	charbuffer[i] = 0;
	putchar('\n');

	i = 0;
	int size   = parseInteger(charbuffer, i, 5, 30, 100);
	int hour   = parseInteger(charbuffer, i, 0, 10, 23);
	int minute = parseInteger(charbuffer, i, 0,  0, 59);
	int second = parseInteger(charbuffer, i, 0,  0, 59);
	int year   = parseInteger(charbuffer, i, 0, 2020, 2999);
	int month  = parseInteger(charbuffer, i, 1,  7, 12);
	int day    = parseInteger(charbuffer, i, 1, 20, 31);

	datetime_t t =
	{
			.year  = int16(year),
			.month = int8(month),	// 1..12
			.day   = int8(day),
			.dotw  = 0,             // day of the week: 0 is Sunday. TODO...
			.hour  = int8(hour),
			.min   = int8(minute),
			.sec   = int8(second)
	};

	printf("start RTC\n");
	rtc_init();
	bool f = rtc_set_datetime(&t);
	if (!f) printf("failded. illegal date.\n");


	// Lissajous Demo:
	//int size  = parseInteger(m_conBuffer, idx, 5, 30, 100);
	FLOAT fmin  = FLOAT(1.49);//parseFloat  (charbuffer, i, 0.95, 1.49, 10.05);
	FLOAT fmax  = FLOAT(1.51);//parseFloat  (charbuffer, i, fmin, 1.51, 10.05);
	FLOAT steps = 100; //parseInteger(charbuffer, i, 10, 100, 1000);
	FLOAT rots  = 5000;//parseInteger(charbuffer, i, 100, 5000, 1000000);
	printf("Lissajous demo:\n");
	printf("size=%i%%, fmin=%.3f, fmax=%.3f, steps/rot=%i, rots/sweep=%i\n", size,double(fmin),double(fmax),int(steps),int(rots));
	LissajousData data(fmin, fmax, steps, rots);


	printf("Starting XY2-100 interface\n");
	XY2 xy2;
	xy2.init();
	xy2.start();


	FLOAT w = SCANNER_WIDTH * FLOAT(size) / 100;
	FLOAT h = w;
	Rect bbox{h/2, -w/2, -h/2, w/2};

	//FLOAT rad = 0;
	//FLOAT sx=0,sy=0,dsx=0.003f;
	//FLOAT px=0,py=0,dpx=0.0000002f;


	enum State
	{
		BOOT_GEOMETRY_TEST,
		LASEROIDS_ANIMATION,
		MAIN_MENU,
		CHECKERBOARD_DEMO,
		CLOCK_DEMO,
		LISSAJOUS_DEMO,
		LASEROIDS_GAME,
		LASEROIDS_NEW_HIGHSCORE,	// big message
		LASEROIDS_ENTER_HISCORE,
	};

	State state = BOOT_GEOMETRY_TEST;
	FLOAT state_countdown = 10;


	uint32 time_last_run_us = time_us_32();
	uint32 adc_last_time_us = time_last_run_us;
	FLOAT rad = 0;		// => rotating demos

	HiScore hiscore;	// hiscore input
	int idx = 0;		// hiscore input

	while (1)
	{
		uint32 now_us = time_us_32();
		FLOAT elapsed_time = min(now_us - time_last_run_us, uint32(100*1000)) * FLOAT(1e-6);
		time_last_run_us = now_us;

		// switch off underrun indicator:
		gpio_put(LED_PIN, 0);

		// display load stats:
		if ((now_us - adc_last_time_us) >= 1000*1000)
		{
			adc_last_time_us += 1000*1000;

			static int id = 0;
			FLOAT min,avg,max;
			static constexpr FLOAT r=0.5;

			if (uint16 d = xy2.getUnderruns())
			{
				printf("***UNDERRUN***: %u data frames\n", d);
			}
			else if (load_sensor.adc_errors)
			{
				printf("ADC conversion error\n");
				load_sensor.adc_errors = 0;
			}
			else
			switch(id++)
			{
			case 0:
				load_sensor.getCore0Load(min,avg,max);
				printf("load core0 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(min+r), int(avg+r), int(max+r));
				break;
			case 1:
				load_sensor.getCore1Load(min,avg,max);
				printf("load core0 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(min+r), int(avg+r), int(max+r));
				break;
			case 2:
				printf("temperature = %.1f°C\n", double(load_sensor.getTemperature()));
				break;
			default:
				id=0;
				break;
			}
		}


		switch(state)
		{
		case BOOT_GEOMETRY_TEST:
		{
			FLOAT w = SCANNER_WIDTH * FLOAT(size) / 100;
			FLOAT h = w;
			Rect bbox{h/2, -w/2, -h/2, w/2};

			XY2::resetTransformation();
			drawCheckerBoard(bbox, 4, fast_straight);

			if (--state_countdown <= 0)
			{
				state = LASEROIDS_ANIMATION;
				state_countdown = FLOAT(1e30);
			}
			continue;
		}
		case LASEROIDS_ANIMATION:
		{
			xy2.setRotation(-rad); rad += pi/180; if (rad>=2*pi) rad -= 2*pi;
			//xy2.printText(Point(0,0),w/25,h/20,"LASEROIDS!",true);
			xy2.printText(Point(0,0),w/25,h/20,"LASEROIDS!",true,fast_straight,fast_rounded);
			xy2.printText(Point(0,-h/8),w/100,h/80,"on your Laser Scanner!",true,fast_straight,fast_rounded);

			if (getchar_timeout_us(0) > 0) state = MAIN_MENU;
			state_countdown -= elapsed_time;
			if (rad<=pi/180 && state_countdown <= 0) state = MAIN_MENU;
			continue;
		}
		case MAIN_MENU:
		{
			Transformation t{350,350,0,0,0,0};
			xy2.setTransformation(t);
			xy2.printText(Point(-30,+20),1,1,"1 Start",false,fast_straight,fast_rounded);
			xy2.printText(Point(-30,+10),1,1,"2 HiScores",false,fast_straight,fast_rounded);
			xy2.printText(Point(-30, 0),1,1,"3 Options",false,fast_straight,fast_rounded);
			xy2.printText(Point(-30,-10),1,1,"4-6 Demos",false,fast_straight,fast_rounded);
			xy2.printText(Point(-30,-20),1,1,"9 Stats",false,fast_straight,fast_rounded);
			xy2.resetTransformation();

			int c = getchar_timeout_us(0);
			switch(c)
			{
			case '1':	// start game
				laseroids.startNewGame();
				state = LASEROIDS_GAME;
				continue;
			case '2':	// hiscores
				//TODO
				continue;
			case '3':	// Options
				//TODO
				continue;
			case '4':	// Checkerboard
				state = CHECKERBOARD_DEMO;
				continue;
			case '5':	// Clock
				state = CLOCK_DEMO;
				continue;
			case '6':	// Lissajous
				state = LISSAJOUS_DEMO;
				continue;
			case '9':	// show stats
				//TODO
				continue;
			case '@':	// reboot to BOOTSEL mode (USB)
				reset_usb_boot(1<<25,0);
			}
			continue;
		}
		case CHECKERBOARD_DEMO:
		{
			xy2.setRotation(-rad); rad += pi/180; if (rad>pi) rad -= 2*pi;

			drawCheckerBoard(bbox, 4, fast_straight);
			if (getchar_timeout_us(0) > 0) { state = MAIN_MENU; }
			continue;
		}
		case CLOCK_DEMO:
		{
			rtc_get_datetime(&t);
			second = t.hour*60*60 + t.min*60 + t.sec;

			drawClock (bbox, uint(second), slow_straight, fast_rounded);
			if (getchar_timeout_us(0) > 0) { state = MAIN_MENU; }
			continue;
		}
		case LISSAJOUS_DEMO:
		{
			xy2.setRotation(-rad); rad += pi/180; if (rad>pi) rad -= 2*pi;

			drawLissajous (w,h, data, fast_rounded);
			if (getchar_timeout_us(0) > 0) { state = MAIN_MENU; }
			continue;
		}
		case LASEROIDS_GAME:
		{
			int c = getchar_timeout_us(0);
			while(getchar_timeout_us(0)>=0){}
			switch(c)
			{
			case 'o':
				laseroids.rotateLeft();
				break;
			case 'p':
				laseroids.rotateRight();
				break;
			case 'q':
				laseroids.accelerateShip();
				break;
			case ' ':
				laseroids.shootCannon();
				break;
			case 'a':
				laseroids.activateShield();
				break;
			}

			laseroids.runOneFrame();
			if (laseroids.isGameOver())
			{
				if (hiscores.isHiscore(laseroids.getScore()))
				{
					state = LASEROIDS_NEW_HIGHSCORE;
					state_countdown = FLOAT(2.0);
				}
				else
				{
					state = MAIN_MENU;
				}
			}
			continue;
		}
		case LASEROIDS_NEW_HIGHSCORE:
		{
			XY2::resetTransformation();
			XY2::printText(Point(0,0),SCANNER_WIDTH/50,SCANNER_WIDTH/40,"HISCORE!",true);

			state_countdown -= elapsed_time;
			if (state_countdown <= 0)
			{
				uint16 score = uint16(laseroids.getScore());
				uint16 secs  = uint16(laseroids.getPlaytime());
				rtc_get_datetime(&t);
				new(&hiscore) HiScore("",score,secs,t);
				idx = 0;
				state = LASEROIDS_ENTER_HISCORE;
			}
			continue;
		}
		case LASEROIDS_ENTER_HISCORE:
		{
			static constexpr int name_len = NELEM(hiscore.name);

			#define SIZE SCANNER_WIDTH

			static constexpr FLOAT ch0 = SIZE/80;	// char height for 1px (char height = 8px)
			static constexpr FLOAT cw0 = SIZE/90;	// char width  for 1px (char height = 8px)
			XY2::resetTransformation();
			XY2::printText(Point(0, ch0), cw0, ch0, "Your Name:", true);

			static constexpr FLOAT ch = SIZE/100;	// char height for 1px (char height = 8px)
			static constexpr FLOAT cw = SIZE/100;	// char width  for 1px (char height = 8px)

			idx = (idx+name_len) % name_len;

			// draw underscores:
			for (int i=name_len; i--; )
			{
				Point pos{((i*2-name_len+1)*8/2)*cw, -12*ch};
				Point a{pos.x-3*cw,pos.y-2*ch};
				Point e{pos.x+3*cw,pos.y-2*ch};
				if (i==idx)
				{
					static int flicker = 0;
					if (++flicker & 3)
					{
						XY2::drawLine(e,a,slow_straight); a.y = e.y += ch/8;
						XY2::drawLine(a,e,fast_straight); a.y = e.y += ch/8;
						XY2::drawLine(e,a,slow_straight);
					}
				}
				else
				{
					XY2::drawLine(e,a,fast_straight);
				}
			}

			// print name so far:
			for (int i=0; i<name_len; i++)
			{
				Point pos{((i*2-name_len+1)*8/2)*cw, -12*ch};
				char text[2] = {hiscore.name[i],0};
				XY2::printText(pos, cw, ch, text, true);
				if (i==idx)
				{
					pos.x += cw/8;
					pos.y += ch/8;
					XY2::printText(pos, cw, ch, text, true);
				}
			}

			// read char from player and process it:
			static constexpr int ESC = 27;
			int c;
			while ((c = getchar_timeout_us(0)) >= 0)
			{
				if (c>='a' && c<='z') c += 'A'-'a';
				if ((c>='A' && c<='Z') || (c>='0' && c<= '9') || c=='.' || c=='-' || c==' ')
				{
					hiscore.name[idx++] = char(c);
				}
				else if (c==ESC)
				{
					esc: c = getchar_timeout_us(5000);

					if (c=='[')
					{
						c = getchar_timeout_us(5000);
						if (c=='D') { idx--; }
						else if (c=='C') { idx++; }
						else if (c==ESC) goto esc;
						else if (c>0) printf("unhandled escape sequence: ESC '[' 0x%02x\n",c);
					}
					else if (c==ESC) goto esc;
					else if (c>0) printf("unhandled escape sequence: ESC 0x%02x\n",c);
				}
				else if (c==127)
				{
					hiscore.name[idx] = ' ';
					idx--;
				}
				else if (c==8)
				{
					idx--;
				}
				else if (c==13)
				{
					hiscores.addHiscore(hiscore);
					xy2.suspend();
					uint err = Flash::writeFlashData(&hiscores);
					xy2.resume();
					if (err) printf("writing hiscores to flash failed: %u\n", err);
					else printf("hiscore written to flash\n");
					state = LASEROIDS_ANIMATION;
					state_countdown = FLOAT(4.0);
					break;
				}
				else if (c>0)
				{
					printf("unhandled char: 0x%02x ('%c')\n", c, c>=32&&c<127?c:' ');
				}
			}
			continue;
		} //case
		} // switch(state)
	} // while(1)
} // main()

































