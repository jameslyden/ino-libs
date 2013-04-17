/* This is a stripped down version of the Adafruit RGB 16x2 LCD Shield library
*/

#include "LCDlite.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <Wire.h>

#include "Arduino.h"

LCDlite::LCDlite() {
	_i2cAddr = 0;

	_displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;

	// the I/O expander pinout
	_rs_pin = 15;
	_rw_pin = 14;
	_enable_pin = 13;
	_data_pins[0] = 12;  // really d4
	_data_pins[1] = 11;  // really d5
	_data_pins[2] = 10;  // really d6
	_data_pins[3] = 9;  // really d7

	_button_pins[0] = 0;
	_button_pins[1] = 1;
	_button_pins[2] = 2;
	_button_pins[3] = 3;
	_button_pins[4] = 4;
	// we can't begin() yet :(
}

void LCDlite::init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
		uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7) {
	_rs_pin = rs;
	_rw_pin = rw;
	_enable_pin = enable;

	_data_pins[0] = d0;
	_data_pins[1] = d1;
	_data_pins[2] = d2;
	_data_pins[3] = d3; 
	_data_pins[4] = d4;
	_data_pins[5] = d5;
	_data_pins[6] = d6;
	_data_pins[7] = d7; 

	_i2cAddr = 255;

	_pinMode(_rs_pin, OUTPUT);
	// we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
	if (_rw_pin != 255) { 
		_pinMode(_rw_pin, OUTPUT);
	}
	_pinMode(_enable_pin, OUTPUT);


	if (fourbitmode)
		_displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	else 
		_displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;

	begin(16, 1);  
}

void LCDlite::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
	// check if i2c
	if (_i2cAddr != 255) {
		//_i2c.begin(_i2cAddr);
		Wire.begin();
		_i2c.begin();

		_i2c.pinMode(8, OUTPUT);
		_i2c.pinMode(6, OUTPUT);
		_i2c.pinMode(7, OUTPUT);
		setBacklight(0x7);

		if (_rw_pin)
			_i2c.pinMode(_rw_pin, OUTPUT);

		_i2c.pinMode(_rs_pin, OUTPUT);
		_i2c.pinMode(_enable_pin, OUTPUT);
		for (uint8_t i=0; i<4; i++) 
			_i2c.pinMode(_data_pins[i], OUTPUT);

		for (uint8_t i=0; i<5; i++) {
			_i2c.pinMode(_button_pins[i], INPUT);
			_i2c.pullUp(_button_pins[i], 1);
		}
	}

	if (lines > 1) {
		_displayfunction |= LCD_2LINE;
	}
	_numlines = lines;
	_currline = 0;

	// for some 1 line displays you can select a 10 pixel high font
	if ((dotsize != 0) && (lines == 1)) {
		_displayfunction |= LCD_5x10DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	delayMicroseconds(50000); 
	// Now we pull both RS and R/W low to begin commands
	_digitalWrite(_rs_pin, LOW);
	_digitalWrite(_enable_pin, LOW);
	if (_rw_pin != 255) { 
		_digitalWrite(_rw_pin, LOW);
	}

	//put the LCD into 4 bit or 8 bit mode
	if (! (_displayfunction & LCD_8BITMODE)) {
		// this is according to the hitachi HD44780 datasheet
		// figure 24, pg 46

		// we start in 8bit mode, try to set 4 bit mode
		write4bits(0x03);
		delayMicroseconds(4500); // wait min 4.1ms

		// second try
		write4bits(0x03);
		delayMicroseconds(4500); // wait min 4.1ms

		// third go!
		write4bits(0x03); 
		delayMicroseconds(150);

		// finally, set to 8-bit interface
		write4bits(0x02); 
	} else {
		// this is according to the hitachi HD44780 datasheet
		// page 45 figure 23

		// Send function set command sequence
		command(LCD_FUNCTIONSET | _displayfunction);
		delayMicroseconds(4500);  // wait more than 4.1ms

		// second try
		command(LCD_FUNCTIONSET | _displayfunction);
		delayMicroseconds(150);

		// third go
		command(LCD_FUNCTIONSET | _displayfunction);
	}

	// finally, set # lines, font size, etc.
	command(LCD_FUNCTIONSET | _displayfunction);  

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;  
	display();

	// clear it off
	clear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	command(LCD_ENTRYMODESET | _displaymode);

}

/********** high level commands, for the user! */
void LCDlite::clear() {
	command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
	delayMicroseconds(2000);  // this command takes a long time!
}

void LCDlite::setCursor(uint8_t col, uint8_t row) {
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if ( row > _numlines ) {
		row = _numlines-1;    // we count rows starting w/0
	}

	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void LCDlite::display() {
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

/*********** mid level commands, for sending data/cmds */

inline void LCDlite::command(uint8_t value) {
	send(value, LOW);
}

inline size_t LCDlite::write(uint8_t value) {
	send(value, HIGH);
	return 1;
}

/************ low level data pushing commands **********/

// little wrapper for i/o writes
void  LCDlite::_digitalWrite(uint8_t p, uint8_t d) {
	if (_i2cAddr != 255) {
		// an i2c command
		_i2c.digitalWrite(p, d);
	} else {
		// straightup IO
		digitalWrite(p, d);
	}
}

// Allows to set the backlight, if the LCD backpack is used
void LCDlite::setBacklight(uint8_t status) {
	// check if i2c or SPI
	_i2c.digitalWrite(8, ~(status >> 2) & 0x1);
	_i2c.digitalWrite(7, ~(status >> 1) & 0x1);
	_i2c.digitalWrite(6, ~status & 0x1);
}

// little wrapper for i/o directions
void  LCDlite::_pinMode(uint8_t p, uint8_t d) {
	if (_i2cAddr != 255) {
		// an i2c command
		_i2c.pinMode(p, d);
	} else {
		// straightup IO
		pinMode(p, d);
	}
}

// write either command or data, with automatic 4/8-bit selection
void LCDlite::send(uint8_t value, uint8_t mode) {
	_digitalWrite(_rs_pin, mode);

	// if there is a RW pin indicated, set it low to Write
	if (_rw_pin != 255) { 
		_digitalWrite(_rw_pin, LOW);
	}

	if (_displayfunction & LCD_8BITMODE) {
		write8bits(value); 
	} else {
		write4bits(value>>4);
		write4bits(value);
	}
}

void LCDlite::pulseEnable(void) {
	_digitalWrite(_enable_pin, LOW);
	delayMicroseconds(1);    
	_digitalWrite(_enable_pin, HIGH);
	delayMicroseconds(1);    // enable pulse must be >450ns
	_digitalWrite(_enable_pin, LOW);
	delayMicroseconds(100);   // commands need > 37us to settle
}

void LCDlite::write4bits(uint8_t value) {
	if (_i2cAddr != 255) {
		uint16_t out = 0;

		out = _i2c.readGPIOAB();

		// speed up for i2c since its sluggish
		for (int i = 0; i < 4; i++) {
			out &= ~_BV(_data_pins[i]);
			out |= ((value >> i) & 0x1) << _data_pins[i];
		}

		// make sure enable is low
		out &= ~ _BV(_enable_pin);

		_i2c.writeGPIOAB(out);

		// pulse enable
		delayMicroseconds(1);
		out |= _BV(_enable_pin);
		_i2c.writeGPIOAB(out);
		delayMicroseconds(1);
		out &= ~_BV(_enable_pin);
		_i2c.writeGPIOAB(out);   
		delayMicroseconds(100);

	} else {
		for (int i = 0; i < 4; i++) {
			_pinMode(_data_pins[i], OUTPUT);
			_digitalWrite(_data_pins[i], (value >> i) & 0x01);
		}
		pulseEnable();
	}
}

void LCDlite::write8bits(uint8_t value) {
	for (int i = 0; i < 8; i++) {
		_pinMode(_data_pins[i], OUTPUT);
		_digitalWrite(_data_pins[i], (value >> i) & 0x01);
	}

	pulseEnable();
}

uint8_t LCDlite::readButtons(void) {
	uint8_t reply = 0x1F;

	for (uint8_t i=0; i<5; i++) {
		reply &= ~((_i2c.digitalRead(_button_pins[i])) << i);
	}
	return reply;
}
