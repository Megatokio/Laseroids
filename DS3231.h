// Simple DS3231 RTC ((and AT24C32 EEPROM)) Library
//
// Originally based on DS3231_Simple by:
//	 Copyright (C) 2016 James Sleeman
//	 @author James Sleeman, http://sparks.gogo.co.nz/
//	 @license MIT License
//
// Version for Raspberry Pico:
//	 Copyright (c) 2021 kio@little-bat.de
//	 BSD 2-clause license


#pragma once
#include "cdefs.h"
#include "hardware/i2c.h"
#include "hardware/rtc.h"




// convert between binary and BCD numbers
inline uint8 bcd2bin (uint8 val) { return ((val >> 4) * 10) + (val & 0x0F); }
inline uint8 bin2bcd (uint8 val) { return uint8((val / 10) << 4) | (val % 10); }




class DS3231
{
  public:
	static i2c_inst* i2c_port;	// = RTC_I2C_PORT;
	static const uint8 i2c_sda	= RTC_I2C_PIN_SDA;
	static const uint8 i2c_scl	= RTC_I2C_PIN_SCK;
	static const uint8 i2c_addr	= RTC_I2C_ADDR;		// 7-bit address

	enum ErrNo : int
	{
		OK,
		I2C_ERROR,
	};

	static ErrNo i2c_seek       (uint8 register_address);
	static ErrNo i2c_write_byte (uint8 register_address, uint8 value);
	static ErrNo i2c_read_byte  (uint8 register_address, uint8& value);


	static void init_i2c ();
	static ErrNo init_rtc();

	static ErrNo readDate(datetime_t&);			// returns OK or ErrNo
	static ErrNo writeDate (const datetime_t&);

	// Get the temperature accurate to within 0.25 degrees (°C)
	// the temperatur is sampled by the DS3231 every 64 seconds
	static float getTemperature();


	enum AlarmMode : uint8
	{
		ALARM_EVERY_SECOND                    = 0b11110001,
		ALARM_MATCH_SECOND                    = 0b01110001,
		ALARM_MATCH_SECOND_MINUTE             = 0b00110001,
		ALARM_MATCH_SECOND_MINUTE_HOUR        = 0b00010001,
		ALARM_MATCH_SECOND_MINUTE_HOUR_DATE   = 0b00000001,
		ALARM_MATCH_SECOND_MINUTE_HOUR_DOW    = 0b00001001,

		ALARM_EVERY_MINUTE                    = 0b01110010,

		ALARM_MATCH_MINUTE                    = 0b00110010,
		ALARM_MATCH_MINUTE_HOUR               = 0b00010010,
		ALARM_MATCH_MINUTE_HOUR_DATE          = 0b00000010,
		ALARM_MATCH_MINUTE_HOUR_DOW           = 0b00001010,

		ALARM_HOURLY                          = 0b00110011,
		ALARM_DAILY                           = 0b00010011,
		ALARM_WEEKLY                          = 0b00001011,
		ALARM_MONTHLY                         = 0b00000011,
	};


	// the alarm functions take a mask of 2 bits for the two available alarms:
	//	 ALARM_MATCH_MINUTE_HOUR_DATE	and
	//	 ALARM_MATCH_SECOND_MINUTE_HOUR_DATE

	// check & clear alarms:
	// returns mask of triggered alarms, 0 for none or -1 for error
	static int checkAlarm (uint8 mask = 0x11);

	// just clear the alarms:
	static ErrNo clearAlarm (uint8 mask = 0b11);

	// disable an alarm from firing completely:
	// this is done by setting the alarm time to an impossible datetime_t
	// this also clears any current alarm
	static ErrNo disableAlarm (uint8 mask = 0b11);

	// Sets an alarm
	// the alarm will pull the SQW pin low -> attack to an GPIO for an interrupt.
	//  @param The date/time for the alarm, as appropriate for the alarm mode
	//  for example, for ALARM_MATCH_SECOND the AlarmTime.Second will be the matching criteria.
	static ErrNo setAlarm (const datetime_t& alarm_time, AlarmMode);


	enum Option : uint8	// Control Register 0xE:
	{
		A1IE = 1<<0,	// alarm 1 interrupt enable: assert INT/SQW if alarm time match and INTCN=1
		A2IE = 1<<1,	// alarm 2 interrupt enable: assert INT/SQW if alarm time match and INTCN=1
		INTCN = 1<<2,	// interrupt control: 0: square wave; 1: interrupt if alarm 1 or alarm 2 matches

		SQW_1Hz  = 0<<3,	// square wave frequency select: set the RS1 and RS2 bits:
		SQW_1kHz = 1<<3,	// square wave frequency = 1Hz, 1.024kHz, 4.096kHz or 8.192kHz
		SQW_4kHz = 2<<3,
		SQW_8kHz = 3<<3,

		CONV = 1<<5,	// convert temperature and load TCXO capacitance registers. no effect while BSY=1
		BBSQW = 1<<6,	// enable SQW while on battery
		EOSC = 1<<7, 	// enable oscillator. note: this bit *disables* the oscillator if set to 1 !

		OPTIONS_NONE = 0,
		OPTIONS_ALL = 0xff
	};

	friend Option operator| (Option a, Option b) { return Option(uint8(a) | uint8(b)); }
	friend Option operator& (Option a, Option b) { return Option(uint8(a) & uint8(b)); }
	friend Option operator^ (Option a, Option b) { return Option(uint8(a) ^ uint8(b)); }

	enum Status : uint8	// Status Register 0xF:
	{
		A1F = 1<<0,		// alarm 1 match
		A2F = 1<<1,		// alarm 2 match
		BSY = 1<<2,		// TCXO function (temp conversion) busy
		EN32kHz = 1<<3,	// enable the 32.768kHz pin (power up: ON)
		OSF = 1<<7,		// Oscillator stop flag: set whenever the oscillator stopped

		STATUS_NONE = 0,
		STATUS_ALL = 0xff
	};

	friend Status operator^ (Status a, Status b) { return Status(uint8(a) ^ uint8(b)); }
	friend Status operator| (Status a, Status b) { return Status(uint8(a) | uint8(b)); }
	friend Status operator& (Status a, Status b) { return Status(uint8(a) & uint8(b)); }


	static ErrNo setOption (Option setmask, Option resmask);
	static ErrNo setOption (Option setmask) { return setOption(setmask,OPTIONS_NONE); }
	static ErrNo resOption (Option resmask) { return setOption(OPTIONS_NONE,resmask); }
	static ErrNo getOption (Option& value)  { return i2c_read_byte(0xE, reinterpret_cast<uint8&>(value)); }

	static ErrNo setStatus (Status setmask, Status resmask);
	static ErrNo setStatus (Status setmask) { return setStatus(setmask,STATUS_NONE); }
	static ErrNo resStatus (Status resmask) { return setStatus(STATUS_NONE,resmask); }
	static ErrNo getStatus (Status& value)  { return i2c_read_byte(0xF, reinterpret_cast<uint8&>(value)); }


	// query if RTC oscillator was stopped for some reason
	// and optionally clear this flag
	// This flag is set after first start of the chip and if the oscillator was stopped
	// due to loss of (battery) power or if loss of (system) power and EOSC was set.
	// returns 0 or 1 or -1 for error
	static int oscWasStopped (bool clear_flag=false);
};




// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// EEPROM LOGGING
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


class AT24C32
{
public:
		   const i2c_inst* _port = AT24C32_I2C_PORT;
	static const uint8  _sda_pin = AT24C32_I2C_PIN_SDA;
	static const uint8  _scl_pin = AT24C32_I2C_PIN_SCK;
	static const uint8  _i2cAddr = AT24C32_I2C_ADDR;

	static const uint16_t EEPROM_SIZE_KBIT = 32768;			// EEPROMs are sized in kilobit
	static const uint8_t  EEPROM_PAGE_SIZE = 32;			// and have a number of bytes per page
	static const uint16_t EEPROM_BYTES     = EEPROM_SIZE_KBIT/8;
	static const uint8_t  EEPROM_PAGES     = EEPROM_BYTES/EEPROM_PAGE_SIZE;



  protected:

	static const uint8_t      EEPROM_ADDRESS   = _i2cAddr;
	  // 7 Bit address, the first 4 bits are 1010, the the last 3 bits according to A2, A1 and A0
	  // On the common ZS-042 board, this corresponds to (where x is jumper open, and 1 is jumper closed)
	  // A0    A1    A2
	  //  x    x     x    0x57  (default)
	  //  1    x     x    0x56
	  //  x    1     x    0x55
	  //  1    1     x    0x54
	  //  0    0     1    0x53
	  //  1    0     1    0x52
	  //  1    1     1    0x51



	// EEPROM structure
	//  The EEPROM is used to store "log entries" which each consist of a 5 byte header and an additional 0 to 7 data bytes.
	//  The Header of each block includes a count of the data bytes and then a binary representation of the timestamp.
	//
	//  Blocks are recorded in a circular-buffer fashion in order to reduce wear on the EEPROM, that is, each next block is stored
	//  after the block preceeding it, when the top of the EEPROM is reached the next block goes to the EEPROM address zero.
	//
	//  Blocks do NOT cross EEPROM boundary (a block will not start at the top of the EEPROM and loop around to the bottom) before ending
	//  if a block doesn't fit in the available space at the top of the blocks in the EEPROM, it will be placed in address zero.
	//
	//  Blocks are zeroed completely on read or when they are to be overwritten (even partially overwritten the entire block is nuked).
	//
	//  <Block>     ::= <Header><DataBytes>
	//  <Header> ::= 0Bzzzwwwyy yyyyyymm mmdddddh hhhhiiii iissssss binary representation of DateTime, (zzz = number of data bytes following timestamp, www = day-of-week)
	//  <DataBytes> ::= DB1..7


	uint16_t                  eepromWriteAddress   = EEPROM_BYTES;               // Byte address of the "top" of the EEPROM "stack", the next
																				// "block" stored will be put here, this location may be
																				// a valid block start byte, or it may be 00000000 in which case
																				// there are zero bytes until the next block start which will be
																				// the first of the series.

	uint16_t                  eepromReadAddress = EEPROM_BYTES;                 // Byte address of the "bottom" of the EEPROM "stack", the next
																				// "block" to read is found here, this location may be
																				// a valid block start byte, or it may be 00000000 in which case
																				// there are zero bytes to read.

	/** Searches the EEPROM for the next place to store a block, sets eepromWriteAddress
	 *
	 *  @return eepromWriteAddress
	 */

	uint16_t findEEPROMWriteAddress();

	/** Delete enough complete blocks to have enough free space for the
	 *  given required number of bytes.
	 *
	 *  @return True/False for success/fail
	 */

	uint8_t  makeEEPROMSpace(uint16_t Address, int8_t BytesRequired);

	/** Find the oldest block to read (based on timestamp date), set eepromReadAddress
	 *
	 *  Note: Has to search entire EEPROM, slow.
	 *
	 *  @return eepromReadAddress
	 */

	uint16_t findEEPROMReadAddress();

	/** Read log timestamp and data from a given EEPROM address.
	 *
	 *  @param Address Byte address of log block
	 *  @param timestamp DateTime structure to put the timestamp
	 *  @param data Memory location to put the data associated with the log
	 *  @param size Max size of the data to read (any more is discarded)
	 */

	uint16_t readLogFrom(uint16_t Address, datetime_t &timestamp, uint8_t *data, uint8_t size = 0);

	/** Start a "pagewize" write at the eepromWriteAddress.
	 *
	 *  Follow this call with 1 or more calls to writeBytePagewize()
	 *  Finish with a call to writeBytePagewizeEnd()
	 *
	 *  Writes in the same page (or rather, 16 byte sections of a page)
	 *  are performed as a single write, thence a new write is started
	 *  for the next section.
	 *
	 *  This solves 2 problems, first the 32 byte Wire buffer which isn't
	 *  large enough for a whole page of bytes plus the address bytes.
	 *  Second the fact that the EEPROM loops back to the start of page
	 *  when you run off the end of the page while writing (instead of
	 *  to the start of the next page).
	 *
	 *  Note that you must not modify the eepromWriteAddress between
	 *  a writeBytePagewizeStart() and a writeBytePagewizeEnd()
	 *  or it's all going to get out of kilter.
	 *
	 *  @return Success (boolean) 1/0, presently this method always reports success.
	 *
	 */
	uint8_t writeBytePagewizeStart();

	/** Write a byte during a pagewize operation.
	 *
	 *  Note that this function increments the eepromWriteAddress.
	 *
	 *  @param data Byte to write.
	 *  @see DS3231::writeBytePagewizeStart()
	 *  @return Success (boolean) 1/0, presently this method always reports success.
	 */

	uint8_t writeBytePagewize(const uint8_t data);

	/** End a pagewize write operation.
	 *
	 *  @see DS3231::writeBytePagewizeStart()
	 *  @return Success (boolean) 1/0, this method will return 0 if Wire.endTransmission reports an error.
	 */

	uint8_t writeBytePagewizeEnd();

	/** Read a byte from the EEPROM
	 *
	 *  @param Address The address of the EEPROM to read from.
	 *  @return The data byte read.
	 *  @note   There is limited error checking, if you provide an invalid address, or the EEPROM is not responding etc behaviour is undefined (return 0, return 1, might or might not block...).
	 */
	uint8_t  readEEPROMByte(const uint16_t Address);


  public:
	/** Erase the EEPROM ready for storing log entries. */

	uint8_t  formatEEPROM();

	/** Write a log entry to the EEPROM, having current timestamp, with an attached data of arbitrary datatype (7 bytes max).
	 *
	 *  This method allows you to record a "log entry" which you can later retrieve.
	 *
	 *  The full timestamp is recorded along with one piece of data.  The type of data you provide
	 *  is up to you, as long as your data is less than 7 bytes (byte, int, float, char,
	 *  or a 6 character string would all be fine).
	 *
	 *  To store more than one piece of data in a log, use a struct, again, in total your structure
	 *  needs to occupy 7 bytes or less of memory.
	 *
	 *  Examples:
	 *     Clock.writeLog(analogRead(A0));
	 *     Clock.writeLog( MyTimeAndDate, MyFloatVariable );
	 *
	 *     // Remember, 7 bytes max!
	 *     struct MyDataStructure
	 *     {
	 *        unsigned int AnalogValue1;
	 *        unsigned int AnalogValue2;
	 *     };
	 *     MyDataStructure MyData; MyData.AnalogValue1 = 123; MyData.AnalogValue2 = 456;
	 *     Clock.writeLog(MyDataStructure);
	 *
	 *
	 *  @param data  The data to store, any arbitrary scalar or structur datatype consisting not more than 7 bytes.
	 *  @note  To store a string or other pointer contents, you probably want to use `DS3231::writeLog(const DateTime, const uint8_t *data, uint8_t size)`
	 *
	 */

	//template <typename datatype>
	//  uint8_t  writeLog( const datatype &data  )   {
	//	 return writeLog(read(), (uint8_t *) &data, (uint8_t)sizeof(datatype));
	//  }

	/** Write a log entry to the EEPROM, having supplied timestamp, with an attached data of arbitrary datatype (7 bytes max).
	 *
	 * @see DS3231::writeLog(const datatype &   data)
	 * @param timestamp The timestamp to associate with the log entry.
	 * @param data  The data to store, any arbitrary datatype consisting not more than 7 bytes.
	 */

	//template <typename datatype>
	//  uint8_t  writeLog( const datetime_t &timestamp,  const datatype &data  )   {
	//	 return writeLog(timestamp, (uint8_t *) &data, (uint8_t)sizeof(datatype));
	//  }

	/** Write a log entry to the EEPROM, having supplied timestamp, with an attached data.
	 *
	 * @param timestamp The timestamp to associate with the log entry.
	 * @param data  Pointer to the data to store
	 * @param size  Length of data to store - max length is 7 bytes, this is not checked for compliance!
	 */

	uint8_t  writeLog( const datetime_t &timestamp,  const uint8_t *data, uint8_t size = 1 );


	/** Read the oldest log entry and clear it from EEPROM.
	 *
	 *  @param timestamp Variable to put the timestamp of the log into.
	 *  @param data      Variable to put the data.
	 *
	 *  @note No check is made as to if the type of "data" and the type of
	 *    the data which is read is the same, there is nothing to stop you
	 *    doing "writeLog( myFloat );" and subsequently "readLog( myInt );"
	 *    but if you do so, results are going to be not what you expect!
	 *
	 */

	template <typename datatype>
	  uint8_t  readLog( datetime_t &timestamp,  datatype &data  )   {
		 return readLog(timestamp, (uint8_t *) &data, (uint8_t)sizeof(datatype));
	  }

	/** Read the oldest log entry and clear it from EEPROM.
	 *
	 *  @param timestamp Variable to put the timestamp of the log into.
	 *  @param data      Pointer to buffer to put data associated with the log.
	 *  @param size      Size of the data buffer.  Maximum 7 bytes.
	 *
	 *  @note If the data in the log entry is larger than the buffer, it will be truncated.
	 *
	 */

	uint8_t  readLog( datetime_t &timestamp,         uint8_t *data,       uint8_t size = 1 );


};




