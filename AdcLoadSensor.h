// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "settings.h"
#include "cdefs.h"


class AdcLoadSensor
{
public:
	AdcLoadSensor()=default;
	void init();

	void getCore0Load (FLOAT& min, FLOAT& avg, FLOAT& max);	// 0 .. 1
	void getCore1Load (FLOAT& min, FLOAT& avg, FLOAT& max);	// 0 .. 1
	FLOAT getTemperature ();	// °C

	static constexpr FLOAT adc_clock_divider = 0xffffu;
	static constexpr FLOAT adc_clock = FLOAT(48e6) / adc_clock_divider;

	volatile uint   adc_errors = 0;

	volatile uint   adc_core0_count = 0;
	volatile uint   adc_core0_min = 1<<12;
	volatile uint   adc_core0_max = 0;
	volatile uint32 adc_core0_sum = 0;
	volatile uint   adc_core1_count = 0;
	volatile uint   adc_core1_min = 1<<12;
	volatile uint   adc_core1_max = 0;
	volatile uint32 adc_core1_sum = 0;
	volatile uint   adc_temperature_count = 0;
	volatile uint32 adc_temperature_sum = 0;

private:
	static void adc_irq_handler () noexcept;
	volatile uint   adc_current_channel = 4; // ADC_TEMPERATURE: startup with temperature
};



