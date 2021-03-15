// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "ssd1306.h"


class OledDisplay
{
public:
	OledDisplay()=default;
	void init();

	void oled_chartest();
	void oled_quickbrownfox();

	SSD1306 oled;
};

