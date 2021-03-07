/*
  SSD1306.h - Library for SSD1306 simple debug output on Raspberry Pico C++.
  Created by Joe Jackson, February 10, 2021.
  Released into the public domain.
*/
#ifndef SSD1306_h
#define SSD1306_h

#include <stdint.h>
#include <string.h>
#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "font.h"
#include "font2.h"

#define SET_CONTRAST 0x81
#define SET_ENTIRE_ON 0xA4
#define SET_NORM_INV 0xA6
#define SET_DISP 0xAE
#define SET_MEM_ADDR 0x20
#define SET_COL_ADDR 0x21
#define SET_PAGE_ADDR 0x22
#define SET_DISP_START_LINE 0x40
#define SET_SEG_REMAP 0xA0
#define SET_MUX_RATIO 0xA8
#define SET_COM_OUT_DIR 0xC0
#define SET_DISP_OFFSET 0xD3
#define SET_COM_PIN_CFG 0xDA
#define SET_DISP_CLK_DIV 0xD5
#define SET_PRECHARGE 0xD9
#define SET_VCOM_DESEL 0xDB
#define SET_CHARGE_PUMP 0x8D

#ifndef NO_ADAFRUIT_SSD1306_COLOR_COMPATIBILITY
#define BLACK SSD1306_BLACK     ///< Draw 'off' pixels
#define WHITE SSD1306_WHITE     ///< Draw 'on' pixels
#define INVERSE SSD1306_INVERSE ///< Invert pixels
#endif
/// fit into the SSD1306_ naming scheme
#define SSD1306_BLACK 0   ///< Draw 'off' pixels
#define SSD1306_WHITE 1   ///< Draw 'on' pixels
#define SSD1306_INVERSE 2 ///< Invert pixels

#define SSD1306_MEMORYMODE 0x20          ///< See datasheet
#define SSD1306_COLUMNADDR 0x21          ///< See datasheet
#define SSD1306_PAGEADDR 0x22            ///< See datasheet
#define SSD1306_SETCONTRAST 0x81         ///< See datasheet
#define SSD1306_CHARGEPUMP 0x8D          ///< See datasheet
#define SSD1306_SEGREMAP 0xA0            ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON 0xA5        ///< Not currently used
#define SSD1306_NORMALDISPLAY 0xA6       ///< See datasheet
#define SSD1306_INVERTDISPLAY 0xA7       ///< See datasheet
#define SSD1306_SETMULTIPLEX 0xA8        ///< See datasheet
#define SSD1306_DISPLAYOFF 0xAE          ///< See datasheet
#define SSD1306_DISPLAYON 0xAF           ///< See datasheet
#define SSD1306_COMSCANINC 0xC0          ///< Not currently used
#define SSD1306_COMSCANDEC 0xC8          ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET 0xD3    ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5  ///< See datasheet
#define SSD1306_SETPRECHARGE 0xD9        ///< See datasheet
#define SSD1306_SETCOMPINS 0xDA          ///< See datasheet
#define SSD1306_SETVCOMDETECT 0xDB       ///< See datasheet

#define SSD1306_SETLOWCOLUMN 0x00  ///< Not currently used
#define SSD1306_SETHIGHCOLUMN 0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE 0x40  ///< See datasheet

#define SSD1306_EXTERNALVCC 0x01  ///< External display voltage source
#define SSD1306_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V

#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26              ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27               ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL 0x2E                    ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL 0x2F                      ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3             ///< Set scroll range

typedef uint8_t u8;


class SSD1306
{
public:
	SSD1306();
	SSD1306(u8 w, u8 h, u8 addr, bool extfont);
	void writeln(char* v);
	void print(const char* v);
	void begin();
	void init_i2c(i2c_inst* port, u8 sda_pin, u8 scl_pin);
	void send(u8 v1, u8 v2);
	void init_display();
	void write_cmd(u8 cmd);
	void fill_scr(u8 v);
	void show_scr();
	void contrast(bool contrast);
	void invert(bool invert);
	void draw_pixel(int16_t x, int16_t y, int color);
	void draw_bitmap(int16_t x, int16_t y, const u8 bitmap[], int16_t w,
							  int16_t h, uint16_t color);
	void draw_letter_at(u8 x, u8 y, char c);
	void draw_letter(char c);
	void pixel(int x, int y);
	void SSD1306_println(const char* str);
	void SSD1306_print(const char* str);
	void display();
	void clearDisplay();
	void setCursor(u8 x, u8 y);
	u8 get_height();
	u8 get_width();
private:
	u8 _width;
	u8 _height;
	u8 _pages;
	u8 _cursorx;
	u8 _cursory;
	i2c_inst* _port;
	u8 _sda_pin;
	u8 _scl_pin;
	u8 _i2cAddr;
	int _i;
	bool _external_vcc;
	bool _extfont;
	u8 i2caddr, vccstate, page_end;
	u8 buffer[1025];
};
#endif
