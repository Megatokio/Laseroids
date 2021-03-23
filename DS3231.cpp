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

#include "DS3231.h"
#include "pico/stdlib.h"


i2c_inst* DS3231::i2c_port = RTC_I2C_PORT;


void DS3231::init_i2c()
{
	//_port = port;
	//_sda_pin = sda_pin;
	//_scl_pin = scl_pin;

	i2c_init(i2c_port, 400 * 1000);
	gpio_set_function(i2c_sda, GPIO_FUNC_I2C);
	gpio_set_function(i2c_scl, GPIO_FUNC_I2C);
	gpio_pull_up(i2c_sda);
	gpio_pull_up(i2c_scl);
}

DS3231::ErrNo DS3231::init_rtc()
{
	// Setup the clock:
	// disable alarm 1 & 2
	// output square wave (not interrupt)
	// square wave frequency = 1Hz
	// no SQW while on battery power
	// don't disable oscillator ==> enable oscillator

	return resOption(OPTIONS_ALL);
	//disableAlarms();
}

DS3231::ErrNo DS3231::i2c_seek (uint8 address)
{
	int n = i2c_write_blocking(i2c_port, i2c_addr, &address, 1, true);
	return n == 1 ? OK : I2C_ERROR;
}

DS3231::ErrNo DS3231::i2c_write_byte (uint8 address, uint8 data)
{
	uint8 buf[2];
	buf[0] = address;
	buf[1] = data;
	int n = i2c_write_blocking(i2c_port, i2c_addr, buf, 2, false);
	return n == 2 ? OK : I2C_ERROR;
}

DS3231::ErrNo DS3231::i2c_read_byte (uint8 address, uint8& data)
{
	int n = i2c_write_blocking(i2c_port, i2c_addr, &address, 1, true);
	if (n == 1) n = i2c_read_blocking(i2c_port,i2c_addr,&data,1,false);
	return n == 1 ? OK : I2C_ERROR;
}

DS3231::ErrNo DS3231::setOption (Option setmask, Option resmask)
{
	uint8 v;
	ErrNo e = i2c_read_byte(0xE, v); if (e) return e;
	return i2c_write_byte(0xE, (v | setmask) & ~resmask);
}

DS3231::ErrNo DS3231::setStatus (Status setmask, Status resmask)
{
	uint8 v;
	ErrNo e = i2c_read_byte(0xF, v); if (e) return e;
	return i2c_write_byte(0xF, (v | setmask) & ~resmask);
}

int DS3231::oscWasStopped (bool clear_osf)
{
	Status s;
	ErrNo e = getStatus(s); if (e) return -1;
	int f = s & OSF;
	if (f && clear_osf) (void)resStatus(OSF);
	return !!f;
}

float DS3231::getTemperature()
{
	uint8 bu[2];
	ErrNo e = i2c_seek(0x11); if (e) return -128;
	int n = i2c_read_blocking(i2c_port,i2c_addr,bu,2,false);
	if (n != 2) return -128;

	return float(int8(bu[0])) + float(bu[1]) / 256;
}

DS3231::ErrNo DS3231::readDate (datetime_t& current_date)
{
	// Set register start address
	ErrNo e = i2c_seek(0x00); if (e) return e;

	// Read 7 bytes: Seconds, Minutes, Hours, Day-of-Week, Day-of-Month, Month, Year
	uint8 bu[7];
	int n = i2c_read_blocking(i2c_port,i2c_addr,bu,7,false);
	if (n != 7) return I2C_ERROR;

	current_date.sec = int8(bcd2bin(bu[0]));
	current_date.min = int8(bcd2bin(bu[1]));

	// 6th Bit of hour indicates AM/PM 12 hour mode, but we save only in 24 hour mode
	uint8 x = bu[2];
	if (x & (1<<6)) { current_date.hour = int8(bcd2bin(x & 0b00011111)) + (x & (1<<5) ? 0 : 12); }
	else { current_date.hour = int8(bcd2bin(x & 0b00111111)); }

	current_date.dotw = int8(bcd2bin(bu[3]));
	current_date.day  = int8(bcd2bin(bu[4]));

	// note: bit 7 of month is century bit: ignored
	current_date.month = int8(bcd2bin(bu[5] & 0b00011111));
	current_date.year  = int16(2000+bcd2bin(bu[6]));

	return OK;
}

DS3231::ErrNo DS3231::writeDate (const datetime_t& current_date)
{
	uint8 bu[8] =
	{
		0x00,	// register address
		bin2bcd(uint8(current_date.sec)),
		bin2bcd(uint8(current_date.min)),
		bin2bcd(uint8(current_date.hour)),		// 24 hour format
		current_date.dotw ? uint8(current_date.dotw) : uint8(1),
		bin2bcd(uint8(current_date.day)),
		bin2bcd(uint8(current_date.month)),		// note: century bit ignored
		bin2bcd(uint8(current_date.year % 100)),
	};

	int n = i2c_write_blocking(i2c_port, i2c_addr, bu, 8, false);
	return n==8 ? OK : I2C_ERROR;
}

DS3231::ErrNo DS3231::clearAlarm (uint8 mask)
{
	uint8 status = 0;
	ErrNo e = i2c_read_byte(0x0F,status);
	// set mask to only include alarm bits (0b11), only those requested (mask) and only those set (status):
	mask &= status & 0b11;
	if (!e && mask) e = i2c_write_byte(0x0F, status & ~mask);
	return e;
}

int DS3231::checkAlarm (uint8 mask)
{
	// returns mask of triggered alarms, 0 for none, -1 for error

	uint8 status = 0;
	ErrNo e = i2c_read_byte(0x0F,status);
	mask &= status & 0b11;
	if (!e && mask) e = i2c_write_byte(0x0F, status & ~mask);
	return e ? -1 : mask;
}

DS3231::ErrNo DS3231::disableAlarm (uint8 mask)
{
	// disable and clear the alarms given by mask.
	// mask = ALARM_MATCH_SECOND_MINUTE_HOUR_DATE or
	//        ALARM_MATCH_MINUTE_HOUR_DATE or both
	//
	// There is no way to actually disable the alarms from triggering,
	//	 so we set them to some unreachable date.
	// note: you can disable the alarms from putting the SQW pin low,
	//	 but they still trigger in the register itself.

	static datetime_t invalid = { 0,0,0,0,31,2,0 };
	ErrNo e = OK;
	if (mask & ALARM_MATCH_SECOND_MINUTE_HOUR_DATE) e = setAlarm(invalid, ALARM_MATCH_SECOND_MINUTE_HOUR_DATE);
	if (!e && mask & ALARM_MATCH_MINUTE_HOUR_DATE)  e = setAlarm(invalid, ALARM_MATCH_MINUTE_HOUR_DATE);
	if (!e) e = clearAlarm(mask);
	return e;
}



//DS3231::ErrNo DS3231::setAlarm (const DateTime& AlarmDate, uint8 AlarmMode)
//{
//  uint8_t controlByte;
//
//  // Read the control byte, we will need to modify the alarm enable bits
//  ErrNo e = rtc_i2c_read_byte(0xE,controlByte); if (e!=OK) return e;
//
//  //if(AlarmMode >> 5 == 3) // Some custom modes we will rewrite the data and recurse with a standard mode
//  if((AlarmMode & 0B00000011) == 0B00000011) // Some custom modes we will rewrite the data and recurse with a standard mode
//  {
//
//	// If AlarmDate was not declared as a constant reference
//	// we could force these, but it comes at a significant cost
//	// in flash consumption.
//	//
//	// AlarmDate.Hour    = 0;
//	// AlarmDate.Minute  = 0;
//	// AlarmDate.Day     = 1;
//	// AlarmDate.Dow     = 1;
//
//	// Set to the equivalent Alarm Mode 2,
//	//    for Hourly this will be Match Minute,
//	//    for Daily it will be Match Minute and Match Hour,
//	//    for Weekly, Match Minute, Hour and Day-Of-Week (Dow)
//	//    for Montly, Match Minute, Hour, and Day-Of-Month (Date)
//
//	AlarmMode         = AlarmMode & 0B11111110;
//  }
//
//  Wire.beginTransmission(RTC_ADDRESS);
//  //if(((AlarmMode >> 5) & 3) == 1) // Alarm 1 Modes
//  if(AlarmMode & 0B00000001)
//  {
//	Wire.write(0x7); // Start address of data for Alarm1
//	Wire.write(bin2bcd(AlarmDate.Second) | (AlarmMode & 0B10000000));
//	controlByte = controlByte | _BV(0) | _BV(2); // Enable Alarm 1, set interrupt output on alarm.
//  }
//  else
//  {
//	Wire.write(0xB); // Start address of data for Alarm2
//	controlByte = controlByte | _BV(1) | _BV(2); // Enable Alarm 2, set interrupt output on alarm.
//  }
//  AlarmMode = AlarmMode << 1;
//
//  Wire.write(bin2bcd(AlarmDate.Minute) | (AlarmMode & 0B10000000));
//  AlarmMode = AlarmMode << 1;
//
//  Wire.write(bin2bcd(AlarmDate.Hour)   | (AlarmMode & 0B10000000));
//  AlarmMode = AlarmMode << 1;
//
//  if(AlarmMode & 0B01000000) // DOW indicator
//  {
//	Wire.write(bin2bcd(AlarmDate.Dow)   | (AlarmMode & 0B10000000) | _BV(6));
//  }
//  else
//  {
//	Wire.write(bin2bcd(AlarmDate.Day)   | (AlarmMode & 0B10000000));
//  }
//  AlarmMode = AlarmMode << 2;  // Value and Date/Day indicator
//
//  if(Wire.endTransmission()) return 0;
//
//  // Write the control byte
//  if(rtc_i2c_write_byte(0xE, controlByte)) return 0;
//
//  return AlarmMode >> 5;
//}



//DS3231::ErrNo DS3231::checkAlarms (bool PauseClock)
//{
//	uint8_t StatusByte = 0;
//	ErrNo e;
//
//	if (PauseClock)
//	{
//		e = rtc_i2c_read_byte(0xE,StatusByte);
//		if (e == OK) e = rtc_i2c_write_byte(0xE, StatusByte | (1<<7));
//		if (e) return e;
//	}
//
//	e = rtc_i2c_read_byte(0xF,StatusByte); if (e) return e;
//
//	if (StatusByte & 0x3)
//	{
//		// Clear the alarm
//		e = rtc_i2c_write_byte(0xF,StatusByte & ~0x3);
//	}
//
//	if (PauseClock)
//	{
//		if (rtc_i2c_read_byte(0xE, PauseClock))
//		{
//			rtc_i2c_write_byte(0xE, PauseClock & ~(_BV(7)));
//		}
//	}
//
//	return StatusByte & 0x3;
//}






//void DS3231::print_zero_padded(Stream &Printer, uint8_t x)
//{
//  if(x < 10) Printer.print('0');
//  Printer.print(x);
//}





//uint8_t DS3231::formatEEPROM()
//{
//  eepromWriteAddress = 0;
//  writeBytePagewizeStart();
//  for(uint16_t x = 0; x < EEPROM_BYTES; x++)
//  {
//	writeBytePagewize(0);
//  }
//  writeBytePagewizeEnd();
//
//  eepromWriteAddress = 0;
//  eepromReadAddress  = 0;
//  return 1;
//}

//uint8_t DS3231::readEEPROMByte(const uint16_t address)
//{
//  uint8_t b = 0;
//  Wire.beginTransmission(EEPROM_ADDRESS); // DUMMY WRITE
//  Wire.write((uint8_t) ((address>>8) & 0xFF));
//  Wire.write((uint8_t) ((address) & 0xFF));
//
//  if(Wire.endTransmission(false)) // Do not send STOP, just restart
//  {
//	return 0;
//  }
//
//  if(Wire.requestFrom(EEPROM_ADDRESS, (uint8_t) 1))
//  {
//	b = Wire.read();
//  }
//
//  Wire.endTransmission(); // Now send STOP
//
//  return b;
//}

//// Locate the NEXT place to store a block
//uint16_t DS3231::findEEPROMWriteAddress()
//{
//  uint8_t t = 0;
//
//  for(eepromWriteAddress = 0; eepromWriteAddress < EEPROM_BYTES; )
//  {
//	t = readEEPROMByte(eepromWriteAddress);
//
//	// If the byte read is a zero, then this is the top of the stack.
//	if(t == 0) break;
//
//	// If not zero, then this must be a start byte for the block (we will assert that
//	// blocks are always aligned to byte zero of the EEPROM, there is no "wrapping"
//	// of a block starting at the top of the EEPROM and finishing in the bottom
//	// so we will add the length of this block to x
//
//	// The upper 3 bits store the number of data bytes
//	// plus 5 header bytes
//	eepromWriteAddress = eepromWriteAddress + (t >> 5) + 5;
//  }
//
//  // If we have filled up as much as we can... reset back to the bottom as the stack top.
//  if(eepromWriteAddress >= EEPROM_BYTES-5)
//  {
//	eepromWriteAddress = 0;
//  }
//
//  return eepromWriteAddress;
//}

//// Locate the NEXT block to read from
//uint16_t DS3231::findEEPROMReadAddress()
//{
//  // This is going to be really memory hungry :-/
//  // Anybody care to think of a better way.
//  uint16_t nxtPtr, x      = 0;
//
//  DateTime currentOldest;
//  DateTime compareWith;
//  currentOldest.Year = 255; // An invalid year the highest we can go so that any valid log is older.
//
//  // Find the oldest block, that is the bottom
//  for(x = 0; x < EEPROM_BYTES; )
//  {
//	if(readEEPROMByte(x) == 0) { x++; continue; }
//
//	// readLogFrom will return the address of the next log entry if any
//	// or EEPROM_BYTES if not.
//
//	nxtPtr = readLogFrom(x, compareWith, 0, 0);
//	if(compareTimestamps(currentOldest,compareWith) > 0)
//	{
//	  currentOldest        = compareWith;
//	  eepromReadAddress    = x;
//	}
//
//	if(nxtPtr > x)
//	{
//	  x = nxtPtr;
//	}
//	else
//	{
//	  break; // no more entries
//	}
//  }
//
//  return eepromReadAddress;
//}

//// Clear some space int he EEPROM to record BytesRequired bytes, nulls
////  any overlappig blocks.
//uint8_t DS3231::makeEEPROMSpace(uint16_t Address, int8_t BytesRequired)
//{
//  if((Address+BytesRequired) >= EEPROM_BYTES)
//  {
//	return 0;  // No can do.
//  }
//  uint8_t x;
//  while(BytesRequired > 0)
//  {
//	x = readEEPROMByte(Address);
//	if(x == 0) // Already blank
//	{
//	  BytesRequired--;
//	  Address++;
//	  continue;
//	}
//	else
//	{
//	  uint16_t oldEepromWriteAddress = eepromWriteAddress;
//	  eepromWriteAddress = Address;
//
//	  writeBytePagewizeStart();
//	  for(x = ((x>>5) + 5); x > 0; x-- )
//	  {
//		writeBytePagewize(0);
//	  }
//	  writeBytePagewizeEnd();
//	  eepromWriteAddress = oldEepromWriteAddress;
//	}
//  }
//
//  return 1;
//}

//uint8_t DS3231::writeBytePagewizeStart()
//{
//  Wire.beginTransmission(EEPROM_ADDRESS);
//  Wire.write((eepromWriteAddress >> 8) & 0xFF);
//  Wire.write(eepromWriteAddress & 0xFF);
//  return 1;
//}

//uint8_t DS3231::writeBytePagewize(const uint8_t data)
//{
//  Wire.write(data);
//
//  // Because of the 32 byte buffer limitation in Wire, we are
//  //  using 4 bits as the page size for a page of 16 bytes
//  //  even though the actual page size is probably higher
//  //  (it needs to be a binary multiple for this to work).
//  eepromWriteAddress++;
//
//  if(eepromWriteAddress < EEPROM_BYTES && ((eepromWriteAddress >>4) & 0xFF) != (((eepromWriteAddress-1)>>4) & 0xFF))
//  {
//	// This is a new page, finish the previous write and start a new one
//	writeBytePagewizeEnd();
//	writeBytePagewizeStart();
//  }
//
//  return 1;
//}

//uint8_t DS3231::writeBytePagewizeEnd()
//{
//  if(Wire.endTransmission() > 0)
//  {
//	// Failure
//	return 0;
//  }
//
//  // Poll for write to complete
//  while(!Wire.requestFrom(EEPROM_ADDRESS,(uint8_t) 1));
//  return 1;
//}

//uint8_t  DS3231::writeLog( const DateTime &timestamp,   const uint8_t *data, uint8_t size )
//{
//  if(size > 7) return 0; // Limit is 7 data bytes.
//
//  if(eepromWriteAddress >= EEPROM_BYTES) findEEPROMWriteAddress();            // Uninitialized stack top, find it.
//  if((eepromWriteAddress + 5 + size) >= EEPROM_BYTES) eepromWriteAddress = 0; // Would overflow so wrap to start
//
//  if(!makeEEPROMSpace(eepromWriteAddress, 5+size))
//  {
//	return 0;
//  }
//
//  writeBytePagewizeStart();
//  writeBytePagewize((size<<5) | (timestamp.Dow<<2) | (timestamp.Year >> 6));
//  writeBytePagewize((timestamp.Year<<2)  | (timestamp.Month >> 2));
//  writeBytePagewize((timestamp.Month<<6) | (timestamp.Day << 1) | (timestamp.Hour >>4));
//  writeBytePagewize((timestamp.Hour<<4)  | (timestamp.Minute>>2));
//  writeBytePagewize(((timestamp.Minute<<6)| (timestamp.Second)) & 0xFF);
//
//  for(; size > 0; size--)
//  {
//	writeBytePagewize(*data);
//	data++;
//  }
//  writeBytePagewizeEnd();
//
//  // We must also clear any existing block in the next write address
//  //  this ensures that if the reader catches up to us that it will only
//  //  read a blank block
//  makeEEPROMSpace(eepromWriteAddress, 5);
//
//  return 1;
//}

//uint16_t DS3231::readLogFrom( uint16_t Address, DateTime &timestamp,   uint8_t *data, uint8_t size )
//{
//  uint8_t b1, b2, datalength;
//
//  b1 = readEEPROMByte(Address++);
//  b2 = readEEPROMByte(Address++);
//
//  if(!b1) return EEPROM_BYTES+1;
//
//  datalength = (b1 >> 5);
//
//  // <Timestamp> ::= 0Bzzzwwwyy yyyyyymm mmdddddh hhhhiiii iissssss
//  timestamp.Dow   =  (b1 >> 2) | 0x03;
//  timestamp.Year  =  (b1 << 6) | (b2>>2);// & 0b11111111
//
//  b1 = readEEPROMByte(Address++);
//  timestamp.Month =  ((b2 << 2) | (b1 >> 6)) & 0b00001111;
//  timestamp.Day   =  (b1 >> 1) & 0b00011111;
//
//  b2 = readEEPROMByte(Address++);
//  timestamp.Hour =  ((b1 << 4) | (b2 >> 4)) & 0b00011111;
//
//  b1 = readEEPROMByte(Address++);
//  timestamp.Minute = ((b2 << 2) | (b1 >> 6)) & 0b00111111;
//  timestamp.Second = b1 & 0b00111111;
//
//  while(datalength--)
//  {
//	// If our supplied buffer has room, copy the data byte into it
//	if(size)
//	{
//	  size--;
//	  *data = readEEPROMByte(Address);
//	  data++;
//	}
//
//	Address++;
//  }
//
//  // If we have caught up with the writer, return that as the next read
//  if(Address == eepromWriteAddress) return Address;
//
//  while(Address < EEPROM_BYTES && (readEEPROMByte(Address) == 0))
//  {
//	Address++;
//  }
//
//  if(Address == EEPROM_BYTES && eepromWriteAddress < Address)
//  {
//	// There was nothing ahead of us, and the writer is behind us
//	//  which means this is all empty unusable space we just walked
//	//  so go to zero position
//	Address = 0;
//  }
//
//  return Address;
//}

//uint8_t DS3231::readLog( DateTime &timestamp,   uint8_t *data, uint8_t size )
//{
//  // Initialize the read address
//  if(eepromReadAddress >= EEPROM_BYTES) findEEPROMReadAddress();
//
//  // Is it still empty?
//  if(eepromReadAddress >= EEPROM_BYTES)
//  {
//	// No log block was found.
//	return 0;
//  }
//
//  uint16_t nextReadAddress = readLogFrom(eepromReadAddress, timestamp, data, size);
//
//  if(nextReadAddress == EEPROM_BYTES+1)
//  {
//	// Indicates no log entry was read (0 start byte)
//	return 0;
//  }
//
//  // Was read OK so we need to kill that byte, we won't trust the user to have
//  // given the correct size here, instead read the start byte
//  makeEEPROMSpace(eepromReadAddress, (readEEPROMByte(eepromReadAddress)>>5)+5);
//
//  eepromReadAddress = nextReadAddress;
//  return 1;
//}





//void DS3231::printTo(Stream &Printer)
//{
//	printTo(Printer, read());
//}
//
//void DS3231::printTo(Stream &Printer, const DateTime &timestamp)
//{
//  printDateTo_YMD(Printer, timestamp);
//  Printer.print('T');
//  printTimeTo_HMS(Printer, timestamp);
//}
//
//void DS3231::printDateTo_DMY(Stream &Printer, const DateTime &Timestamp, const char separator)
//{
//  print_zero_padded(Printer, Timestamp.Day);
//  Printer.print(separator);
//  print_zero_padded(Printer, Timestamp.Month);
//  Printer.print(separator);
//  if(Timestamp.Year > 100)
//  {
//	Printer.print('2');
//  }
//  else
//  {
//	Printer.print(F("20"));
//  }
//  print_zero_padded(Printer, Timestamp.Year);
//}
//
//void DS3231::printDateTo_MDY(Stream &Printer, const DateTime &Timestamp, const char separator)
//{
//  print_zero_padded(Printer, Timestamp.Month);
//  Printer.print(separator);
//  print_zero_padded(Printer, Timestamp.Day);
//  Printer.print(separator);
//  if(Timestamp.Year > 100)
//  {
//	Printer.print('2');
//  }
//  else
//  {
//	Printer.print(F("20"));
//  }
//  print_zero_padded(Printer, Timestamp.Year);
//}

//void DS3231::printDateTo_YMD(Stream &Printer, const DateTime &Timestamp, const char separator)
//{
//  if(Timestamp.Year > 100)
//  {
//	Printer.print('2');
//  }
//  else
//  {
//	Printer.print(F("20"));
//  }
//  print_zero_padded(Printer, Timestamp.Year);
//  Printer.print(separator);
//  print_zero_padded(Printer, Timestamp.Month);
//  Printer.print(separator);
//  print_zero_padded(Printer, Timestamp.Day);
//}
//
//void DS3231::printTimeTo_HMS(Stream &Printer, const DateTime &Timestamp, const char hoursToMinutesSeparator , const char minutesToSecondsSeparator )
//{
//  print_zero_padded(Printer, Timestamp.Hour);
//  Printer.print(hoursToMinutesSeparator);
//  print_zero_padded(Printer, Timestamp.Minute);
//
//  if(minutesToSecondsSeparator != 0x03)
//  {
//	Printer.print(minutesToSecondsSeparator);
//	print_zero_padded(Printer, Timestamp.Second);
//  }
//}
//
//
//void DS3231::printTimeTo_HM (Stream &Printer, const DateTime &Timestamp, const char hoursToMinutesSeparator )
//{
//  printTimeTo_HMS(Printer, Timestamp, hoursToMinutesSeparator, 0x03);
//}
//
//
//void DS3231::print12HourTimeTo_HMS(Stream &Printer, const DateTime &Timestamp, const char hoursToMinutesSeparator , const char minutesToSecondsSeparator )
//{
//  if(Timestamp.Hour > 12)
//  {
//	Printer.print(Timestamp.Hour-12);
//  }
//  else
//  {
//	Printer.print(Timestamp.Hour ? Timestamp.Hour : 12); // Handle 0 hour = 12 as well
//  }
//
//  Printer.print(hoursToMinutesSeparator);
//  print_zero_padded(Printer, Timestamp.Minute);
//
//  if(minutesToSecondsSeparator != 0x03)
//  {
//	Printer.print(minutesToSecondsSeparator);
//	print_zero_padded(Printer, Timestamp.Second);
//  }
//
//  if(Timestamp.Hour > 12)
//  {
//	Printer.print(F(" PM"));
//  }
//  else
//  {
//	Printer.print(F(" AM"));
//  }
//}
//
//void DS3231::print12HourTimeTo_HM (Stream &Printer, const DateTime &Timestamp, const char hoursToMinutesSeparator )
//{
//  print12HourTimeTo_HMS(Printer, Timestamp, hoursToMinutesSeparator, 0x03);
//}

//void DS3231::promptForTimeAndDate(Stream &Serial)
//{
//  char buffer[3] = { 0 };
//  DateTime Settings;
//
//  Serial.println(F("Clock is set when all data is entered and you send 'Y' to confirm."));
//  do
//  {
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Day of Month (2 digits, 01-31): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Day = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Month (2 digits, 01-12): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Month = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Year (2 digits, 00-99): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Year = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Hour (2 digits, 24 hour clock, 00-23): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Hour = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Minute (2 digits, 00-59): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Minute = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.print(F("Enter Second (2 digits, 00-59): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 2);
//	while(Serial.available()) Serial.read();
//	Settings.Second = atoi(buffer[0] == '0' ? buffer+1 : buffer);
//
//
//	memset(buffer, 0, sizeof(buffer));
//	Serial.println();
//	Serial.println(F("Enter Day Of Week (1 digit, 1-7, arbitrarily 1 = Mon, 7 = Sun): "));
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 1);
//	while(Serial.available()) Serial.read();
//	Settings.Dow = atoi(buffer);
//
//	Serial.println();
//	Serial.print(F("Entered Timestamp: "));
//	printTo(Serial, Settings);
//	Serial.println();
//	Serial.print(F("Send 'Y' to set the clock, send 'N' to start again: "));
//
//	while(!Serial.available()) ; // Wait until bytes
//	Serial.readBytes(buffer, 1);
//	while(Serial.available()) Serial.read();
//	if(buffer[0] == 'Y' || buffer[0] == 'y')
//	{
//	  write(Settings);
//	  break;
//	}
//  } while(1);
//}













