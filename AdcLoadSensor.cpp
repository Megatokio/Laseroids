// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/regs/intctrl.h"		// Scheiß Suchspiel
#include "AdcLoadSensor.h"
#include "utilities.h"


AdcLoadSensor load_sensor;

static_assert (ADC_CORE0_IDLE == 0, "");
static_assert (ADC_CORE1_IDLE == 1, "");
static_assert (ADC_TEMPERATURE == 4, "");


void __not_in_flash_func(AdcLoadSensor::adc_irq_handler) () noexcept
//void adc_irq_handler () noexcept
{
	while (!adc_fifo_is_empty())
	{
		uint value = adc_fifo_get();
		bool error = value & 1<<12;

		#define L load_sensor

		if (error)
		{
			L.adc_errors++;
		}
		else switch (L.adc_current_channel)
		{
		case 0:
			L.adc_core0_count++;
			L.adc_core0_sum += value;
			L.adc_core0_min = min(L.adc_core0_min,value);
			L.adc_core0_max = max(L.adc_core0_max,value);
			break;
		case 1:
			L.adc_core1_count++;
			L.adc_core1_sum += value;
			L.adc_core1_min = min(L.adc_core1_min,value);
			L.adc_core1_max = max(L.adc_core1_max,value);
			break;
		default:
			L.adc_temperature_count++;
			L.adc_temperature_sum += value;
			break;
		}

		static uint8 nxch[] = {1,4,0,0,0};
		L.adc_current_channel = nxch[L.adc_current_channel];
	}
}


void AdcLoadSensor::init()
{
	printf("ADC sample frequency = %u Hz (all channels)\n", uint(adc_clock+FLOAT(0.5)));
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
	adc_gpio_init(ADC_PIN_CORE0_IDLE);
	adc_gpio_init(ADC_PIN_CORE1_IDLE);
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

}


void AdcLoadSensor::getCore0Load (FLOAT& min, FLOAT& avg, FLOAT& max)
{
	uint adc_cnt;
	uint adc_sum;

	do
	{
		adc_cnt = L.adc_core0_count;
		adc_sum = L.adc_core0_sum;
	}
	while (adc_cnt != L.adc_core0_count);

	do
	{
		L.adc_core0_count = 0;
		L.adc_core0_sum = 0;
	}
	while(L.adc_core0_count);

	uint adc_max = L.adc_core0_max;
	uint adc_min = L.adc_core0_min;
	L.adc_core0_max = 0;
	L.adc_core0_min = 1<<12;

	min = map_range(FLOAT(adc_max), 0,1<<12, FLOAT(100), FLOAT(0));
	max = map_range(FLOAT(adc_min), 0,1<<12, FLOAT(100), FLOAT(0));

	FLOAT adc_core0_avg = FLOAT(adc_sum) / FLOAT(adc_cnt);
	avg = map_range(adc_core0_avg, 0,1<<12, FLOAT(100), FLOAT(0));
}

void AdcLoadSensor::getCore1Load (FLOAT& min, FLOAT& avg, FLOAT& max)
{
	uint adc_cnt;
	uint adc_sum;

	do
	{
		adc_cnt = L.adc_core1_count;
		adc_sum = L.adc_core1_sum;
	}
	while (adc_cnt != L.adc_core1_count);

	do
	{
		L.adc_core1_count = 0;
		L.adc_core1_sum = 0;
	}
	while(L.adc_core1_count);

	uint adc_max = L.adc_core1_max;
	uint adc_min = L.adc_core1_min;
	L.adc_core1_max = 0;
	L.adc_core1_min = 1<<12;

	min = map_range(FLOAT(adc_max), 0,1<<12, FLOAT(100), FLOAT(0));
	max = map_range(FLOAT(adc_min), 0,1<<12, FLOAT(100), FLOAT(0));

	FLOAT adc_core0_avg = FLOAT(adc_sum) / FLOAT(adc_cnt);
	avg = map_range(adc_core0_avg, 0,1<<12, FLOAT(100), FLOAT(0));
}

FLOAT AdcLoadSensor::getTemperature()
{
	uint adc_cnt;
	uint adc_sum;

	do
	{
		adc_cnt = L.adc_temperature_count;
		adc_sum = L.adc_temperature_sum;
	}
	while (adc_cnt != L.adc_temperature_count);

	do
	{
		L.adc_temperature_count = 0;
		L.adc_temperature_sum = 0;
	}
	while(L.adc_temperature_count);

	FLOAT adc_avg_temperature = FLOAT(adc_sum) / FLOAT(adc_cnt);
	FLOAT voltage = map_range(adc_avg_temperature, 0, 1<<12, FLOAT(0), FLOAT(3.3));
	return map_range(voltage, 0.706f, 0.706f-0.001721f, FLOAT(27), FLOAT(28));
}














