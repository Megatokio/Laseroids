/*
  SSD1306.cpp - Library for SSD1306 simple debug output on Raspberry Pico C++.
  Created by Joe Jackson, February 10, 2021.
  Released into the public domain.
*/
#include <stdint.h>
#include <string.h>
#include <cstdint>
#include <cstdlib>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <stdio.h>

SSD1306::SSD1306()
{
	_width = 128;
	_height = 32;
	_pages = _height / 8;
	_i2cAddr = 0x3C;
	_external_vcc = false;
	_i=0;
	_extfont = false;
}

SSD1306::SSD1306(uint8_t w, uint8_t h, uint8_t addr, bool extfont = false)
{
	_width = w;
	_height = h;
	_pages = _height / 8;
	_i2cAddr = addr;
	_external_vcc = false;
	_i=0;
	_extfont = extfont;
}

void SSD1306::writeln(char* v)
{
	SSD1306_println(v);
	display();
	_i++;
	if (_i >= 8)
	{
		_i = 0;
		sleep_ms(500);
		clearDisplay();
		setCursor(0, 0);
	}
}

void SSD1306::print(const char* v)
{
	//clearDisplay();
	//setCursor(0, 0);
	SSD1306_print(v);
	display();
}

void SSD1306::init_i2c(i2c_inst* port, uint8_t sda_pin, uint8_t scl_pin)
{
	_port = port;
	_sda_pin = sda_pin;
	_scl_pin = scl_pin;
	i2c_init(_port, 400 * 1000);
	gpio_set_function(sda_pin, GPIO_FUNC_I2C);
	gpio_set_function(scl_pin, GPIO_FUNC_I2C);
	gpio_pull_up(sda_pin);
	gpio_pull_up(scl_pin);
}

void SSD1306::init_display()
{
	/* // these are the default initialization commands by adafruit.
	u8 cmds[] = {
		SSD1306_DISPLAYOFF,         // 0xAE
		SSD1306_SETDISPLAYCLOCKDIV, // 0xD5
		0x80, // the suggested ratio 0x80
		SSD1306_SETMULTIPLEX,  // display off 0x0E | 0x00
		_height - 1,
		SSD1306_SETDISPLAYOFFSET, // 0xD3
		0x0,                      // no offset
		SSD1306_SETSTARTLINE | 0x0, // line #0
		SSD1306_CHARGEPUMP,  // horizontal
		(vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14,
		SSD1306_MEMORYMODE, // 0x20
		0x00, // 0x0 act like ks0108
		SSD1306_SEGREMAP | 0x1,
		SSD1306_COMSCANDEC,
		SSD1306_SETCOMPINS,
		0x12,
		SSD1306_SETCONTRAST,
		(vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF,
		SSD1306_SETPRECHARGE,
		(vccstate == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1,

		SSD1306_SETVCOMDETECT, // 0xDB
		0x40,
		SSD1306_DISPLAYALLON_RESUME, // 0xA4
		SSD1306_NORMALDISPLAY,       // 0xA6
		SSD1306_DEACTIVATE_SCROLL,
		SSD1306_DISPLAYON,
	};
	*/
u8 cmds[] = {
		SET_DISP | 0x00,  // display off 0x0E | 0x00

		SET_MEM_ADDR, // 0x20
		0x00,  // horizontal

		//# resolution and layout
		SET_DISP_START_LINE | 0x00, // 0x40
		SET_SEG_REMAP | 0x01,  //# column addr 127 mapped to SEG0

		SET_MUX_RATIO, // 0xA8
		u8(_height - 1),

		SET_COM_OUT_DIR | 0x08,  //# scan from COM[N] to COM0  (0xC0 | val)
		SET_DISP_OFFSET, // 0xD3
		0x00,

		//SET_COM_PIN_CFG, // 0xDA
		//0x02 if self.width > 2 * self.height else 0x12,
		//width > 2*height ? 0x02 : 0x12,
		//SET_COM_PIN_CFG, height == 32 ? 0x02 : 0x12,

		//# timing and driving scheme
		SET_DISP_CLK_DIV, // 0xD5
		0x80,

		SET_PRECHARGE, // 0xD9
		//0x22 if self.external_vcc else 0xF1,
		u8(_external_vcc ? 0x22 : 0xF1),

		SET_VCOM_DESEL, // 0xDB
		//0x30,  //# 0.83*Vcc
		0x40, // changed by mcarter

		//# display
		SET_CONTRAST, // 0x81
		0xFF,  //# maximum

		SET_ENTIRE_ON,  //# output follows RAM contents // 0xA4
		SET_NORM_INV,  //# not inverted 0xA6

		SET_CHARGE_PUMP, // 0x8D
		//0x10 if self.external_vcc else 0x14,
		u8(_external_vcc ? 0x10 : 0x14),

		SET_DISP | 0x01
	};


	// write all the commands
	for(uint i=0; i<sizeof(cmds); i++)
	{
		write_cmd(cmds[i]);
	}
	fill_scr(0);
	show_scr();
}

void SSD1306::write_cmd(u8 cmd)
{
	send(0x80, cmd);
}

void SSD1306::send(u8 v1, u8 v2)
{
	u8 buf[2];
	buf[0] = v1;
	buf[1] = v2;
	i2c_write_blocking(_port, _i2cAddr, buf, 2, false);
}

void SSD1306::clearDisplay()
{
	fill_scr(0);
	//sleep_ms(25);
	//display();
	//sleep_ms(250);
}

void SSD1306::display()
{
	show_scr();
}

void SSD1306::fill_scr(u8 v)
{
	memset(buffer, v, sizeof(buffer));
}

void SSD1306::show_scr()
{
	/*
	u8 cmds[] =
	{
		SSD1306_PAGEADDR,
		0,                      // Page start address
		0xFF,                   // Page end (not really, but works here)
		SSD1306_COLUMNADDR,
		0,
		_width-1
	};

	for(int i=0; i<sizeof(cmds); i++)
		write_cmd(cmds[i]);
	*/

	write_cmd(SET_MEM_ADDR); // 0x20
	write_cmd(0b01); // vertical addressing mode

	write_cmd(SET_COL_ADDR); // 0x21
	write_cmd(0);
	write_cmd(127);

	write_cmd(SET_PAGE_ADDR); // 0x22
	write_cmd(0);
	write_cmd(_pages-1);
	buffer[0] = 0x40; // the data instruction
	i2c_write_blocking(_port, _i2cAddr, buffer, _width * ((_height+7)/8), false);
}

void SSD1306::contrast(bool contrast)
{
	write_cmd(SSD1306_SETCONTRAST); write_cmd((contrast)? 0 : 0x8F);
}

void SSD1306::invert(bool invert)
{
	write_cmd(invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

void SSD1306::draw_pixel(int16_t x, int16_t y, int color)
{
	if(x<0 || x >= _width || y<0 || y>= _height) return;

	int _page = y/8;
	int bit = 1<<(y % 8);
	u8* ptr = buffer + x*8 + _page  + 1;

	switch (color) {
		case 1: // white
			*ptr |= bit;
			break;
		case 0: // black
			*ptr &= ~bit;
			break;
		case -1: //inverse
			*ptr ^= bit;
			break;
	}

}
u8 SSD1306::get_height()
{
	return _height;
}
u8 SSD1306::get_width()
{
	return _width;
}
void SSD1306::draw_bitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
							  int16_t h, uint16_t color) {

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  for (int16_t j = 0; j < h; j++, y++) {
	for (int16_t i = 0; i < w; i++) {
	  if (i & 7)
		byte <<= 1;
	  else
		byte = bitmap[j * byteWidth + i / 8];
	  if (byte & 0x80)
		draw_pixel(x + i, y, color);
	}
  }
}

void SSD1306::draw_letter_at(u8 x, u8 y, char c)
{
	if(c< ' ' || c>  0x7F) c = '?'; // 0x7F is the DEL key
	if (!_extfont){
		int offset = 4 + (c - ' ' )*6;
		for(u8 col = 0 ; col < 6; col++) {
			u8 line =  ssd1306_font6x8[offset+col];
			for(u8 row=0; row <8; row++) {
				draw_pixel(x+col, y+row, line & 1);
				line >>= 1;
			}
		}
	} else {
		for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
			uint8_t line = glcd5x7ascii[c*5+i];
			for (int8_t j = 0; j < 8; j++, line >>= 1)
			{
				draw_pixel(x + i, y + j, line & 1);
			}
		}
	}
	for(u8 row = 0; row<8; row++) {
		draw_pixel(x+6, y+row, 0);
		draw_pixel(x+7, y+row, 0);
	}

}

void SSD1306::setCursor(u8 x, u8 y){
	_cursorx = x;
	_cursory = y;
}

void SSD1306::draw_letter(char c)
{
	draw_letter_at(_cursorx, _cursory, c);
}

void SSD1306::pixel(int x, int y)
{
	int page = y/8;
	u8 patt = u8(1<<(y%8));
	buffer[1+ x*8 + page] |= patt;

}

void SSD1306::SSD1306_println(const char* str)
{
	while(char c = *str) {
		str++;
		if(c == '\n') {
			_cursorx = 0;
			_cursory += 8;
			continue;
		}
		draw_letter_at(_cursorx, _cursory, c);
		_cursorx += 8;
	}
	_cursorx = 0;
	_cursory += 8;
}

void SSD1306::SSD1306_print(const char* str)
{
	while(char c = *str) {
		str++;
		if(c == '\n') {
			_cursorx = 0;
			_cursory += 8;
			continue;
		}
		draw_letter_at(_cursorx, _cursory, c);
		_cursorx += 8;
	}
}
