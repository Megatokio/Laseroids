// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "standard_types.h"

// our float type is float32:
using FLOAT = float;

// clocks:
constexpr uint32 MAIN_CLOCK 	= 125 * 1000000;
constexpr uint32 XY2_DATA_CLOCK	= 100 * 1000;


// LEDs:
constexpr uint LED_PIN  = 25;	// on-board LED
constexpr uint LED_BEAT = 18;	// xy2 heart beat
constexpr uint LED_CORE0_IDLE = 19;	// idle indicator core 0
constexpr uint LED_CORE1_IDLE = 20;	// idle indicator core 1
constexpr uint LED_ERROR = 21;	// error happened


// XY2 data lines:
constexpr uint PIN_XY2_CLOCK      = 8;
constexpr uint PIN_XY2_CLOCK_NEG  = PIN_XY2_CLOCK + 1;
constexpr uint PIN_XY2_SYNC       = PIN_XY2_CLOCK + 2;
constexpr uint PIN_XY2_SYNC_NEG   = PIN_XY2_CLOCK + 3;

constexpr uint PIN_XY2_Y        = 12;
constexpr uint PIN_XY2_Y_NEG    = PIN_XY2_Y + 1;

constexpr uint PIN_XY2_X        = 14;
constexpr uint PIN_XY2_X_NEG    = PIN_XY2_X + 1;

constexpr uint PIN_XY2_SYNC_XY  = 16; // 30 or 31 geht nicht
constexpr uint PIN_XY2_SYNC_XY_READBACK = 17;   // externally connected to PIN_XY2_SYNC_XY for PWM input
constexpr uint PIN_XY2_LASER	= 22; // laser on/off


// PIO settings:
#define   PIO_XY2 pio0


// Scanner data:
constexpr FLOAT SCANNER_MAX_SWIVELS  = 15000/120;	// data sheet: 15m/s @ 12cm

constexpr int32 SCANNER_MIN			= 0;
constexpr int32 SCANNER_MAX			= 0xffff;
constexpr int32 SCANNER_WIDTH   	= 0x10000u;
constexpr FLOAT SCANNER_MAX_SPEED	= SCANNER_MAX_SWIVELS * SCANNER_WIDTH / XY2_DATA_CLOCK;


// Laser settings
constexpr uint LASER_QUEUE_DELAY = 14;	// delay for laser power values
constexpr uint LASER_ON_DELAY = 0;		// how many steps before switching laser ON
constexpr uint LASER_OFF_DELAY = 0; 	// how many steps before switching laser OFF
constexpr uint LASER_MIDDLE_DELAY = 6; 	// how many steps to wait at poly line corners
constexpr uint LASER_JUMP_DELAY = 20;	// after jump


// use a distortion correction map?
#define XY2_USE_MAP 1
#define XY2_USE_MAP_PICO_INTERP 1
#define XY2_MAP_BITS 6
#define XY2_LOW_BITS 10		// 6+10 = scanner width

#define XY2_IMPLEMENT_ANALOGUE_CLOCK_DEMO
#define XY2_IMPLEMENT_CHECKER_BOARD_DEMO
#define XY2_IMPLEMENT_LISSAJOUS_DEMO


// ADC settings
extern class AdcLoadSensor load_sensor;
#define ADC_PIN_CORE0_IDLE 26
#define ADC_PIN_CORE1_IDLE 27
#define ADC_CORE0_IDLE  0
#define ADC_CORE1_IDLE  1
#define ADC_TEMPERATURE 4

// OLED settings
extern class OledDisplay oled;
#define OLED_I2C_ADDR	  0x3C
#define OLED_I2C_PIN_SDA  2
#define OLED_I2C_PIN_SCK  3
#define OLED_I2C_PORT     i2c1
#define OLED_WIDTH		  128
#define OLED_HEIGHT		  64
#define OLED_EXTERNAL_VCC false

// RTC settings
#define RTC_I2C_ADDR	 0x68
#define RTC_I2C_PIN_SDA  2
#define RTC_I2C_PIN_SCK  3
#define RTC_I2C_PORT     i2c1

// AT24C32 Flash (on RTC module)
#define AT24C32_I2C_ADDR	0x57
#define AT24C32_I2C_PIN_SDA RTC_I2C_PIN_SDA
#define AT24C32_I2C_PIN_SCK RTC_I2C_PIN_SCK
#define AT24C32_I2C_PORT    RTC_I2C_PORT





