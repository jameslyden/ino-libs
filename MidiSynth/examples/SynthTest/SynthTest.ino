/* MIDI player for Arduino, based on VS1053 DSP
 * James Lyden <james@lyden.org>
 */

// MP3 library (and dependencies) used to patch in realtime MIDI capability
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <MidiSynth.h>
// MIDI comms implemented as software serial port
#include <SoftwareSerial.h>
// I2C LCD/buttons combo
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

// Spawn global objects required by MP3 library and MIDI handlers
SdFat sd;
MidiSynth midiSynth;
SoftwareSerial midiPort(2,3); // RX pin, TX pin
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield(); // SCL=A4, SDA=A5

// constants defined for convenience
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

//----------------------------------------------------------------------------//
// helper functions for MIDI protocol

// Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
// FIXME: enforce protocol restrictions
void talkMIDI(byte cmd, byte data1, byte data2)
{
  midiPort.write(cmd);
  midiPort.write(data1);

  // Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes
  // (sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    midiPort.write(data2);
}

// Send a MIDI note-on message. Channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) 
{
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

// Send a MIDI note-off message.
void noteOff(byte channel, byte note, byte release_velocity) 
{
  talkMIDI( (0x80 | channel), note, release_velocity);
}

//----------------------------------------------------------------------------//
// Mandatory functions

void setup()
{
	int time = millis();		// track how long setup takes
	
	Serial.begin(115200);	// console I/O
	midiPort.begin(31250);	// MIDI requires 31250 baud
	lcd.begin(16, 2);			// initialize the 16x2 display

	lcd.setBacklight(WHITE);
	lcd.print(F("Initializing..."));

	// Initialize the SD card, used to load patches to DSP
	if(!sd.begin(SD_SEL, SPI_HALF_SPEED)) sd.initErrorHalt();
	if(!sd.chdir("/")) sd.errorHalt("sd.chdir");

	// Initialize the synthesizer
	int result = midiSynth.begin();
	while(result != 0) {
		lcd.setBacklight(RED);
		lcd.setCursor(0, 1);
		lcd.print(F("Error code ")); lcd.print(result);
		midiSynth.end();
		delay(60000);
		result = midiSynth.begin();
	}

	// set reasonable defaults
	talkMIDI(0xB0, 0x07, 100); // MIDI master volume (max=127)

	lcd.setBacklight(TEAL);
	time = millis() - time;
	lcd.clear();
	lcd.print(F("Took ")); lcd.print(time); lcd.print(F(" ms"));
	lcd.setCursor(0, 1);
	lcd.print(F("Free RAM = ")); lcd.print(FreeRam(), DEC);
	delay(5000);


	lcd.clear();
	lcd.setCursor(0, 0); lcd.print(F(" Bank Ins>Nte Ch"));
	lcd.setCursor(0, 1); lcd.print(F(" 0x78 1   35  0"));
	lcd.setBacklight(GREEN);
}

// initialize variables to provide sane defaults
int bank = 0x78;		// percussion=0x78, melodic=0x79, general=0x00
int instrument = 1;	// 1-127 (mapping varies by bank)
int note = 35;			// most instruments don't produce sound below 30
int channel = 0;		// 0-15, all 16 can be playing concurrently
int selected = 2;		// 0-3, which field is selected
boolean heldDown = false; // tracks if SELECT has been held down since last poll

void loop()
{
	// Handle user input
	uint8_t buttons = lcd.readButtons();

	if (buttons & BUTTON_SELECT) {
		if (!heldDown) {
			talkMIDI(0xB0, 0, bank); 			// Bank select
			talkMIDI(0xC0, instrument, 0); 	// Instrument select
			//Note on some channel, some note value (note), middle velocity (0x45):
			noteOn(channel, note, 127);
			heldDown=true;
		}
		delay(10);
	} else {
		//Turn off the note with a given off/release velocity
		noteOff(channel, note, 60);
		heldDown=false;
	}
	if (buttons & BUTTON_UP) {
		switch (selected) {
			case 0:
				switch (bank) {
					case 0x00:
						bank = 0x78;
						break;
					case 0x78:
						bank = 0x79;
						break;
					case 0x79:
						break;
				}
				break;
			case 1:
				instrument++;
				if (instrument > 127) instrument = 127;
				break;
			case 2:
				note++;
				if (note > 90) note = 90;
				break;
			case 3:
				channel++;
				if (channel > 15) channel = 15;
				break;
		}
	// Update display
	lcd.setCursor(0, 1); lcd.print(F("                "));
	lcd.setCursor(1, 1); lcd.print(F("0x")); lcd.print(bank, HEX);
	lcd.setCursor(6, 1); lcd.print(instrument);
	lcd.setCursor(10, 1); lcd.print(note);
	lcd.setCursor(14, 1); lcd.print(channel, HEX);
	delay(100);
	}
	if (buttons & BUTTON_DOWN) {
		switch (selected) {
			case 0:
				switch (bank) {
					case 0x00:
						break;
					case 0x78:
						bank = 0x00;
						break;
					case 0x79:
						bank = 0x78;
						break;
				}
				break;
			case 1:
				instrument--;
				if (instrument < 0) instrument = 0;
				break;
			case 2:
				note--;
				if (note < 0) note = 0;
				break;
			case 3:
				channel--;
				if (channel < 0) channel = 0;
				break;
		}
	// Update display
	lcd.setCursor(0, 1); lcd.print(F("                "));
	lcd.setCursor(1, 1); lcd.print(F("0x")); lcd.print(bank, HEX);
	lcd.setCursor(6, 1); lcd.print(instrument);
	lcd.setCursor(10, 1); lcd.print(note);
	lcd.setCursor(14, 1); lcd.print(channel, HEX);
	delay(100);
	}
	if (buttons & BUTTON_RIGHT) {
		switch (selected) {
			case 0:
				lcd.setCursor(0, 0); lcd.print(F(" "));
				lcd.setCursor(5, 0); lcd.print(F(">"));
				selected = 1;
				break;
			case 1:
				lcd.setCursor(5, 0); lcd.print(F(" "));
				lcd.setCursor(9, 0); lcd.print(F(">"));
				selected = 2;
				break;
			case 2:
				lcd.setCursor(9, 0); lcd.print(F(" "));
				lcd.setCursor(13, 0); lcd.print(F(">"));
				selected = 3;
				break;
			case 3:
				break;
		}
	delay(180);
	}
	if (buttons & BUTTON_LEFT) {
		switch (selected) {
			case 0:
				break;
			case 1:
				lcd.setCursor(5, 0); lcd.print(F(" "));
				lcd.setCursor(0, 0); lcd.print(F(">"));
				selected = 0;
				break;
			case 2:
				lcd.setCursor(9, 0); lcd.print(F(" "));
				lcd.setCursor(5, 0); lcd.print(F(">"));
				selected = 1;
				break;
			case 3:
				lcd.setCursor(13, 0); lcd.print(F(" "));
				lcd.setCursor(9, 0); lcd.print(F(">"));
				selected = 2;
				break;
		}
	delay(180);
	}
}

