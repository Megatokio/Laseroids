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
constexpr int32 SCANNER_WIDTH   	= 0x10000;
constexpr FLOAT SCANNER_MAX_SPEED	= SCANNER_MAX_SWIVELS * SCANNER_WIDTH / XY2_DATA_CLOCK;


// Laser settings
constexpr uint LASER_ON_DELAY = 6;		// how many steps before switching laser ON
constexpr uint LASER_OFF_DELAY = 10; 	// how many steps before switching laser OFF
constexpr uint LASER_MIDDLE_DELAY = 6; 	// how many steps to wait at poly line corners
constexpr uint LASER_JUMP_DELAY = 6;	// after jump


#define XY2_IMPLEMENT_ANALOGUE_CLOCK_DEMO
#define XY2_IMPLEMENT_CHECKER_BOARD_DEMO
#define XY2_IMPLEMENT_LISSAJOUS_DEMO


// ADC settings
#define PIN_ADC_CORE0_IDLE 26
#define PIN_ADC_CORE1_IDLE 27
#define ADC_CORE0_IDLE  0
#define ADC_CORE1_IDLE  1
#define ADC_TEMPERATURE 4

// OLED settings
#define I2C_OLED_PIN_SDA  2
#define I2C_OLED_PIN_SCK  3
#define I2C_OLED i2c1
