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


int main()
{
	stdio_init_all();
	gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT); gpio_put(LED_PIN,0);

	printf("init OLED display\n");
	oled.init();

	printf("init ADC\n");
	load_sensor.init();




	printf("\nLasteroids - Asteroids on LaserScanner\n");
	//while(getchar()!=13);

	char charbuffer[256] = "";
	printf("please enter size (%%), HH:MM:SS, YYYY/MM/DD (all optional)\n> ");
	uint i=0;
	while(getchar_timeout_us(0)>=0){}
	for(;;)
	{
		int c = getchar_timeout_us(60*1000*1000);
		if (c<0 || c==13) break;
		if (c=='@')	{ reset_usb_boot(25,0); }
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


	// Start on Friday 5th of June 2020 15:45:00
	datetime_t t = {
			.year  = int16(year),
			.month = int8(month),
			.day   = int8(day),
			.dotw  = 0,             // 0 is Sunday. TODO...
			.hour  = int8(hour),
			.min   = int8(minute),
			.sec   = int8(second)
	};

	// Start the RTC
	rtc_init();
	rtc_set_datetime(&t);


	//int size  = parseInteger(m_conBuffer, idx, 5, 30, 100);
	FLOAT fmin  = FLOAT(1.49);//parseFloat  (charbuffer, i, 0.95, 1.49, 10.05);
	FLOAT fmax  = FLOAT(1.51);//parseFloat  (charbuffer, i, fmin, 1.51, 10.05);
	FLOAT steps = 100; //parseInteger(charbuffer, i, 10, 100, 1000);
	FLOAT rots  = 5000;//parseInteger(charbuffer, i, 100, 5000, 1000000);
	printf("Lissajous demo:\n");
	printf("size=%i%%, fmin=%.3f, fmax=%.3f, steps/rot=%i, rots/sweep=%i\n", size,double(fmin),double(fmax),int(steps),int(rots));


	printf("Starting XY2-100 interface\n");
	printf("SCANNER_MAX_SPEED = %u\n", uint(SCANNER_MAX_SPEED));
	XY2 xy2;
	xy2.init();
	xy2.start();

	FLOAT w = SCANNER_WIDTH * FLOAT(size) / 100;
	FLOAT h = w;
	Rect bbox{h/2, -w/2, -h/2, w/2};

	for (int i=0;i<10;i++)
	{
		//xy2.drawRect(bbox);
		drawCheckerBoard(bbox, 4, fast_straight);
	}


	//bool led_on=0;
	FLOAT rad = 0;
	//FLOAT sx=0,sy=0,dsx=0.003f;
	//FLOAT px=0,py=0,dpx=0.0000002f;
	const FLOAT pi = FLOAT(3.1415926538);
	LissajousData data(fmin, fmax, steps, rots);


	while (1)
	{
		//sleep_ms(125);
		//gpio_put(LED_PIN, led_on=!led_on);
		gpio_put(LED_PIN, 0);

		rtc_get_datetime(&t);
		second = t.hour*60*60 + t.min*60 + t.sec;

		static int adc_last_second=0;
		static int id = 0;

		if (second != adc_last_second)
		{
			adc_last_second = second;

			#define L load_sensor
			FLOAT min,avg,max;
			static constexpr FLOAT r=0.5;

			if (uint16 d = xy2.getUnderruns())
			{
				printf("***OVERRUN***: %u frames\n", d);
			}
			else if (L.adc_errors)
			{
				printf("ADC conversion error\n");
				L.adc_errors = 0;
			}
			else
			switch(id++)
			{
			case 0:
			{
				load_sensor.getCore0Load(min,avg,max);
				printf("load core0 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(min+r), int(avg+r), int(max+r));
			}	break;
			case 1:
			{
				load_sensor.getCore1Load(min,avg,max);
				printf("load core0 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(min+r), int(avg+r), int(max+r));
			}	break;
			case 2:
			{
				FLOAT temperature = load_sensor.getTemperature();
				printf("temperature = %.1f°C\n", double(temperature));
			}	break;
			default:
				id=0;
				break;
			}
		}

		static int demo = 2;
		int c = getchar_timeout_us(0);
		if (c > 0)
		{
			if (c>='1' && c<='6')
			{
				demo = c-'0';
				XY2::resetTransformation();
				if (demo==6) { laseroids.startNewGame(); }
			}
			else if (c=='?')
			{
				printf("1 checkerboard\n");
				printf("2 clock\n");
				printf("3 lissajous\n");
				printf("4,5 title\n");
				printf("6 Laseroids\n");
				printf("@ reset as USB stick\n");
			}
			else if (c=='@')	// reboot to BOOTSEL mode (USB)
			{
				reset_usb_boot(25,0);
			}
			else if (demo == 6)	// Laseroids
			{
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
			}
		}

		xy2.setRotation(-rad); rad += pi/180; if (rad>pi) rad -= 2*pi;
		//xy2.setShear(sx,sy); sx+=dsx; if(sx>=0.5f || sx<=-0.5f) dsx *= -1;
		//xy2.setProjection(px,py); px+=dpx; if(px>0.00003f || px<-0.00003f) dpx *= -1;

		switch(demo)
		{
		case 1:
			drawCheckerBoard(bbox, 4, fast_straight);
			break;
		case 2:
			drawClock (bbox, uint(second), slow_straight, fast_rounded);
			break;
		case 3:
			drawLissajous (w,h, data, fast_rounded);
			break;
		case 4:
			xy2.printText(Point(0,0),w/25,h/20,"LASEROIDS!",true);
			//xy2.printText(Point(0,-h/10),w/100,h/80,"Asteroids on a Laser Scanner!",true);
			break;
		case 5:
			xy2.printText(Point(0,0),w/25,h/20,"LASEROIDS!",true,fast_straight,fast_rounded);
			xy2.printText(Point(0,-h/10),w/100,h/80,"Asteroids on a Laser Scanner!",true,fast_straight,fast_rounded);
			break;
		case 6:
			laseroids.runOneFrame();
			if (laseroids.isGameOver()) laseroids.startNewGame();
			break;
		}
	}
}










