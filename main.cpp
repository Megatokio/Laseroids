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
}
//#include "pico/util/datetime.h"
#include "charset1.h"
#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/regs/intctrl.h"		// Scheiß Suchspiel
//#include "hardware/divider.h"
//#include "pico/divider.h"
#include "hardware/pwm.h"
#include "Laseroids.h"


static SSD1306 oled(128, 64, 0x3C, false);

void oled_chartest()
{
	char c[50];
	for (uint8 dz = 0; dz<255; dz++)
	{
		oled.setCursor(0,0);
		sprintf(c, "Char #%d: %c", dz,dz);
		oled.print(c);
		oled.display();
		sleep_ms(250);
	}
	oled.clearDisplay();
}

void oled_quickbrownfox()
{
	cstr text1 = "a quick brown fox jumped over the lazy dogs.";
	cstr text2 = "1234567890ß!\"§$%%&/()=?\\.";
	cstr text3 = "<>@,.-;:_öäÖÄüÜ#'+*~`^°";
	oled.setCursor(0,0);
	oled.print(text1);
	oled.setCursor(0,8);
	oled.print(text2);
	oled.setCursor(0,16);
	oled.print(text3);
}



static constexpr FLOAT adc_clock_divider = 0xffffu; // FLOAT(0xffffffu)/256;
static constexpr FLOAT adc_clock = FLOAT(48e6) / adc_clock_divider;

static volatile uint  adc_current_channel = 4; // ADC_TEMPERATURE;	// startup with temperature
static volatile uint  adc_errors = 0;

static volatile uint   adc_core0_count = 0;
static volatile uint   adc_core0_min = 1<<12;
static volatile uint   adc_core0_max = 0;
static volatile uint32 adc_core0_sum = 0;
static volatile uint   adc_core1_count = 0;
static volatile uint   adc_core1_min = 1<<12;
static volatile uint   adc_core1_max = 0;
static volatile uint32 adc_core1_sum = 0;
static volatile uint   adc_temperature_count = 0;
static volatile uint32 adc_temperature_sum = 0; // (1<<12) * 0.7f/3.3f;	// something around 27°C

static_assert (ADC_CORE0_IDLE == 0, "");
static_assert (ADC_CORE1_IDLE == 1, "");
static_assert (ADC_TEMPERATURE == 4, "");

void __not_in_flash_func(adc_irq_handler) () noexcept
//void adc_irq_handler () noexcept
{
	while (!adc_fifo_is_empty())
	{
		uint value = adc_fifo_get();
		bool error = value & 1<<12;

		if (error)
		{
			adc_errors++;
		}
		else switch (adc_current_channel)
		{
		case 0:
			adc_core0_count++;
			adc_core0_sum += value;
			adc_core0_min = min(adc_core0_min,value);
			adc_core0_max = max(adc_core0_max,value);
			break;
		case 1:
			adc_core1_count++;
			adc_core1_sum += value;
			adc_core1_min = min(adc_core1_min,value);
			adc_core1_max = max(adc_core1_max,value);
			break;
		default:
			adc_temperature_count++;
			adc_temperature_sum += value;
			break;
		}

		static uint8 nxch[] = {1,4,0,0,0};
		adc_current_channel = nxch[adc_current_channel];
	}
}


int main()
{
	stdio_init_all();
	gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT); gpio_put(LED_PIN,0);

	printf("charset1_init\n");
	charset1_init();
	printf("oled.init_i2c\n");
	oled.init_i2c(I2C_OLED, I2C_OLED_PIN_SDA, I2C_OLED_PIN_SCK);
	printf("oled.init_display\n");
	oled.init_display();
	printf("oled_chartest\n");
	//oled_chartest();
	oled_quickbrownfox();



	printf("init ADC\n");
	printf("sample frequency = %u Hz (all channels)\n", uint(adc_clock+FLOAT(0.5)));
	adc_init();
	if (0)	// if adc can be restarted
	{
		//adc_core0_min_load = ;
		//adc_core0_max_load = ;
		//adc_core0_avg_load = ;
		//adc_core1_min_load = ;
		//adc_core1_max_load = ;
		//adc_core1_avg_load = ;
		//adc_avg_temperature = ;
		adc_current_channel = ADC_TEMPERATURE;	// startup with temperature
		adc_errors = 0;
	}

	adc_set_temp_sensor_enabled(true);	// apply power
	adc_gpio_init(PIN_ADC_CORE0_IDLE);
	adc_gpio_init(PIN_ADC_CORE1_IDLE);
	adc_select_input(ADC_TEMPERATURE);	// start with measurement for temperature before round robbin
	adc_set_round_robin((1<<ADC_CORE0_IDLE)+(1<<ADC_CORE1_IDLE)+(1<<ADC_TEMPERATURE));
	adc_fifo_setup (true/*enable*/, true/*dreq_en*/, 1/*dreq_thresh*/, true/*err_in_fifo*/, false/*!byte_shift*/);
	irq_set_exclusive_handler (ADC_IRQ_FIFO, adc_irq_handler);
	adc_irq_set_enabled(true);
	irq_set_enabled(ADC_IRQ_FIFO,true);
	adc_set_clkdiv(adc_clock_divider);	// slowest possible: 832 measurements / sec
	adc_run(true);

	printf("start ADC\n");
	while (adc_temperature_count == 0)
	{
		if (!adc_fifo_is_empty())
		{
			printf("ADC interrupt failed to start\n");
			//for(;;);
		}
	}
	printf("...ok\n");


	// count overruns
	// it seems not possible to read back PIN_XY2_SYNC_XY directly:
	const uint gpio = PIN_XY2_SYNC_XY_READBACK;
	// Only PWM B pins (odd pin numbers) can be used as inputs:
	assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
	const uint slice_num = pwm_gpio_to_slice_num(gpio);

	// configure the PWM for counter mode:
	pwm_config cfg = pwm_get_default_config();
	pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
	pwm_init(slice_num, &cfg, true);
	gpio_set_function(gpio, GPIO_FUNC_PWM);
	gpio_set_dir(gpio,GPIO_IN);

	int overruns = 0;	// seen so far


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

	static Laseroids laseroids;
	printf("main#%u\n",__LINE__);

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

			if (uint16 d = uint16(pwm_get_counter(slice_num) - overruns))
			{
				printf("***OVERRUN***: %u frames\n", d);
				overruns += d;
			}
			else
			if (adc_errors)
			{
				printf("");
				adc_errors = 0;
			}
			else
			switch(id++)
			{
			case 0:
			{	FLOAT adc_core0_avg = FLOAT(adc_core0_sum)/FLOAT(adc_core0_count);
				while(adc_core0_count) { adc_core0_count = 0; adc_core0_sum = 0; }
				printf("load core0 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(map_range(FLOAT(adc_core0_max), 0,1<<12, 100,0)),
					   int(map_range(adc_core0_avg, 0,1<<12, 100,0)),
					   int(map_range(FLOAT(adc_core0_min), 0,1<<12, 100,0)));
				//printf("load core0 = %4.1fV %4.1fV %4.1fV (min,avg,max)\n",
				//	   double(map_range(FLOAT(adc_core0_max), 0,1<<12, 0,3.3f)),
				//	   double(map_range(adc_core0_avg, 0,1<<12, 0,3.3f)),
				//	   double(map_range(FLOAT(adc_core0_min), 0,1<<12, 0,3.3f)));
				adc_core0_max = 0;
				adc_core0_min = 1<<12;
			}	break;
			case 1:
			{	FLOAT adc_core1_avg = FLOAT(adc_core1_sum)/FLOAT(adc_core1_count);
				while(adc_core1_count) { adc_core1_count = 0; adc_core1_sum = 0; }
				printf("load core1 = %i%% %i%% %i%% (min,avg,max)\n",
					   int(map_range(FLOAT(adc_core1_max), 0,1<<12, 100,0)),
					   int(map_range(adc_core1_avg, 0,1<<12, 100,0)),
					   int(map_range(FLOAT(adc_core1_min), 0,1<<12, 100,0)));
				//printf("load core1 = %4.1fV %4.1fV %4.1fV (min,avg,max)\n",
				//	   double(map_range(FLOAT(adc_core1_max), 0,1<<12, 0,3.3f)),
				//	   double(map_range(adc_core1_avg, 0,1<<12, 0,3.3f)),
				//	   double(map_range(FLOAT(adc_core1_min), 0,1<<12, 0,3.3f)));
				adc_core1_max = 0;
				adc_core1_min = 1<<12;
			}	break;
			case 2:
			{	FLOAT adc_avg_temperature = FLOAT(adc_temperature_sum) / FLOAT(adc_temperature_count);
				while(adc_temperature_count) { adc_temperature_count = 0; adc_temperature_sum = 0; }
				FLOAT voltage = map_range(adc_avg_temperature, 0, 1<<12, FLOAT(0), FLOAT(3.3));
				FLOAT temperature = map_range(voltage, 0.706f, 0.706f-0.001721f, FLOAT(27), FLOAT(28));
				static constexpr char deg = 0xB0;
				uint temp = uint(temperature*10);
				printf("temperature = %u.%u%cC\n", temp/10, temp%10, deg);
				//printf("temperature = %.1f%cC\n", temperature, deg);
				//uint32 temp_frac,temp = divmod_u32u32_rem(uint(temperature),10,&temp_frac);
				//printf("temperature = %u.%u%cC\n", temp, temp_frac, deg);
				//printf("temperature = %.1f%cC\n", double(temperature), deg);
				//divmod_result_t temp = hw_divider_divmod_u32(uint(temperature*10), 10);
				//printf("temperature = %u.%u%cC\n", to_quotient_u32(temp),to_remainder_u32(temp), deg);

			}	break;
			default:
				//static constexpr uint num_cases = 4;
				//printf("adc conversions: %u/sec\n",adc_count / num_cases);
				//adc_count = 0;
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
				laseroids.lives = 0;		// => reset if Laseroids demo started
			}
			else if (demo == 6)	// Laseroids
			{
				laseroids.activate_shield(false);
				switch(c)
				{
				case 'q':
					laseroids.accelerate();
					break;
				case 'a':
					laseroids.decelerate();
					break;
				case 'o':
					laseroids.rotate_left();
					break;
				case 'p':
					laseroids.rotate_right();
					break;
				case ' ':
					laseroids.shoot();
					break;
				case 'y':
					laseroids.activate_shield(1);
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
			xy2.printText(Point(0,0),w/50,h/40,"LASTEROIDS!",true);
			xy2.printText(Point(0,-h/10),w/100,h/80,"Asteroids on a Laser Scanner!",true);
			break;
		case 5:
			xy2.printText(Point(0,0),w/50,h/40,"LASTEROIDS!",true,fast_straight,fast_rounded);
			xy2.printText(Point(0,-h/10),w/100,h/80,"Asteroids on a Laser Scanner!",true,fast_straight,fast_rounded);
			break;
		case 6:
			laseroids.run_1_frame();
			if (laseroids.game_over()) laseroids.init();
			break;
		}
	}
}










