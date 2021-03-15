// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "OledDisplay.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "charset1.h"


OledDisplay oled;


void OledDisplay::oled_chartest()
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

void OledDisplay::oled_quickbrownfox()
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


void OledDisplay::init()
{
	charset1_init();
	oled.init_i2c(OLED_I2C_PORT, OLED_I2C_PIN_SDA, OLED_I2C_PIN_SCK);
	oled.init_display();
	oled_quickbrownfox();
}

