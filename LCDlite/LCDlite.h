/* This is a stripped down version of the Adafruit RGB 16x2 LCD Shield library
*/

#ifndef LCDlite_h
#define LCDlite_h

#include <inttypes.h>
#include "Print.h"
#include <MCP23017.h>

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// button masks
#define BUTTON_UP 0x08
#define BUTTON_DOWN 0x04
#define BUTTON_LEFT 0x10
#define BUTTON_RIGHT 0x02
#define BUTTON_SELECT 0x01

// backlight colors
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

class LCDlite : public Print {
	public:
		LCDlite();

		void init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
				uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
				uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

		void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS);

		void clear();
		void home();

		void noDisplay();
		void display();
		void noBlink();
		void blink();
		void noCursor();
		void cursor();
		void scrollDisplayLeft();
		void scrollDisplayRight();
		void leftToRight();
		void rightToLeft();
		void autoscroll();
		void noAutoscroll();

		// only if using backpack
		void setBacklight(uint8_t status); 

		void createChar(uint8_t, uint8_t[]);
		void setCursor(uint8_t, uint8_t); 
		virtual size_t write(uint8_t);
		void command(uint8_t);
		uint8_t readButtons();

	private:
		void send(uint8_t, uint8_t);
		void write4bits(uint8_t);
		void write8bits(uint8_t);
		void pulseEnable();
		void _digitalWrite(uint8_t, uint8_t);
		void _pinMode(uint8_t, uint8_t);

		uint8_t _rs_pin; // LOW: command.  HIGH: character.
		uint8_t _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
		uint8_t _enable_pin; // activated by a HIGH pulse.
		uint8_t _data_pins[8];
		uint8_t _button_pins[5];
		uint8_t _displayfunction;
		uint8_t _displaycontrol;
		uint8_t _displaymode;

		uint8_t _initialized;

		uint8_t _numlines,_currline;

		uint8_t _i2cAddr;
		MCP23017 _i2c;
};

#endif
