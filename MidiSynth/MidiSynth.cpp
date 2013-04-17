/* MidiSynth.cpp
 * James Lyden <james@lyden.org>
 *
 * Stripped-down version of the SFEMP3Shield library, modified to adapt the
 * VS1053 for real-time MIDI synthesis
 */

#include "MidiSynth.h"
#include "SPI.h"

// Init static variable
uint16_t MidiSynth::spiRate;

// Initialization and powerdown -- vs_init() does the heavy lifting for begin()
uint8_t  MidiSynth::begin() {

	pinMode(MIDI_DREQ, INPUT);
	pinMode(MIDI_XCS, OUTPUT);
	pinMode(MIDI_XDCS, OUTPUT);
	pinMode(MIDI_RESET, OUTPUT);

	cs_high();  // MIDI_XCS, Init Control Select to deselected
	dcs_high(); // MIDI_XDCS, Init Data Select to deselected
	digitalWrite(MIDI_RESET, LOW); // Put VS1053 into hardware reset

	uint8_t result = vs_init();
	if(result) {
		return result;
	}

	return 0;
}

void MidiSynth::end() {

	cs_high();  // MIDI_XCS, Init Control Select to deselected
	dcs_high(); // MIDI_XDCS, Init Data Select to deselected

	// most importantly...
	digitalWrite(MIDI_RESET, LOW); // Put VS1053 into hardware reset
}

uint8_t MidiSynth::vs_init() {
	// Reset if not already
	delay(100); // keep clear of anything prior
	digitalWrite(MIDI_RESET, LOW); // Shut down VS1053
	delay(100);

	// Bring out of reset
	digitalWrite(MIDI_RESET, HIGH); // Bring up VS1053

	// set initial mp3's spi to safe rate
	spiRate = SPI_CLOCK_DIV16; // initial contact with VS10xx at slow rate
	delay(10);

	// Let's check the status of the VS1053
	int MP3Mode = MidiReadRegister(SCI_MODE);

	if(MP3Mode != (SM_LINE1 | SM_SDINEW)) return 4;

	// Now that we have the VS1053 up and running, increase the internal clock multiplier and up our SPI rate
	MidiWriteRegister(SCI_CLOCKF, 0x6000); // Set multiplier to 3.0x
	// Internal clock multiplier is now 3x.
	// Therefore, max SPI speed is 52MgHz.
	spiRate = SPI_CLOCK_DIV4; // use safe SPI rate of (16MHz / 4 = 4MHz)
	delay(10); // settle time

	// test reading after data rate change
	int MP3Clock = MidiReadRegister(SCI_CLOCKF);
	if(MP3Clock != 0x6000) return 5;

	setVolume(25, 25);	// In -dB, so lower is louder (0 = max volume)
	setEarSpeaker(3);	// datasheet-recommended setting for realtime MIDI

	if(VSLoadUserCode("rtmidi.053")) return 6;

	delay(100); // just a good idea to let settle.

	return 0; // indicating all was good.
}

// Load patches into DSP memory from SD card
uint8_t MidiSynth::VSLoadUserCode(char* fileName){

	union twobyte val;
	union twobyte addr;
	union twobyte n;
	SDfile patch;

	if(!digitalRead(MIDI_RESET)) return 3;

	// Open the file in read mode.
	if(!patch.open(fileName, O_READ)) return 2;
	while(1) {
		if(!patch.read(addr.byte, 2)) break;
		if(!patch.read(n.byte, 2)) break;
		if(n.word & 0x8000U) {
			n.word &= 0x7FFF;
			if(!patch.read(val.byte, 2)) break;
			while(n.word--) {
				MidiWriteRegister(addr.word, val.word);
			}
		} else {
			while(n.word--) {
				if(!patch.read(val.byte, 2))   break;
				MidiWriteRegister(addr.word, val.word);
			}
		}
	}
	patch.close(); // Close out this patch
	// playing_state = ready;
	return 0;
}

// Manipulate master volume
void MidiSynth::setVolume(uint8_t leftchannel, uint8_t rightchannel){

	VolL = leftchannel;
	VolR = rightchannel;
	MidiWriteRegister(SCI_VOL, leftchannel, rightchannel);
}

uint16_t MidiSynth::getVolume() {
	uint16_t MP3SCI_VOL = MidiReadRegister(SCI_VOL);
	return MP3SCI_VOL;
}

// Manipulate EarSpeaker (synthetic spatial positioning)
uint8_t MidiSynth::getEarSpeaker() {
	uint8_t result = 0;
	uint16_t MP3SCI_MODE = MidiReadRegister(SCI_MODE);

	// SM_EARSPEAKER bits are not adjacent hence need to add them together
	if(MP3SCI_MODE & SM_EARSPEAKER_LO) {
		result += 0b01;
	}
	if(MP3SCI_MODE & SM_EARSPEAKER_HI) {
		result += 0b10;
	}
	return result;
}

void MidiSynth::setEarSpeaker(uint16_t EarSpeaker) {
	uint16_t MP3SCI_MODE = MidiReadRegister(SCI_MODE);

	// SM_EARSPEAKER bits are not adjacent hence need to add them individually
	if(EarSpeaker & 0b01) {
		MP3SCI_MODE |=  SM_EARSPEAKER_LO;
	} else {
		MP3SCI_MODE &= ~SM_EARSPEAKER_LO;
	}

	if(EarSpeaker & 0b10) {
		MP3SCI_MODE |=  SM_EARSPEAKER_HI;
	} else {
		MP3SCI_MODE &= ~SM_EARSPEAKER_HI;
	}
	MidiWriteRegister(SCI_MODE, MP3SCI_MODE);
}

// Toggle SPI control channel
void MidiSynth::cs_low() {
	SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(spiRate); // Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
	digitalWrite(MIDI_XCS, LOW);
}

void MidiSynth::cs_high() {
	digitalWrite(MIDI_XCS, HIGH);
}

// Toggle SPI data channel
void MidiSynth::dcs_low() {
	SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(spiRate); // Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
	digitalWrite(MIDI_XDCS, LOW);
}

void MidiSynth::dcs_high() {
	digitalWrite(MIDI_XDCS, HIGH);
}

// Directly manipulate VS1053 registers
void MidiSynth::MidiWriteRegister(uint8_t addressbyte, uint16_t data) {
	union twobyte val;
	val.word = data;
	MidiWriteRegister(addressbyte, val.byte[1], val.byte[0]);
}

void MidiSynth::MidiWriteRegister(uint8_t addressbyte, uint8_t highbyte, uint8_t lowbyte) {

	// skip if the chip is in reset.
	if(!digitalRead(MIDI_RESET)) return;

	// Wait for DREQ to go high indicating IC is available
	while(!digitalRead(MIDI_DREQ)) ;
	// Select control
	cs_low();

	// SCI consists of instruction byte, address byte, and 16-bit data word.
	SPI.transfer(0x02); // Write instruction
	SPI.transfer(addressbyte);
	SPI.transfer(highbyte);
	SPI.transfer(lowbyte);
	while(!digitalRead(MIDI_DREQ)) ; // Wait for DREQ to go high indicating command is complete
	cs_high(); // Deselect Control
}

uint16_t MidiSynth::MidiReadRegister (uint8_t addressbyte){

	union twobyte resultvalue;

	// skip if the chip is in reset.
	if(!digitalRead(MIDI_RESET)) return 0;

	while(!digitalRead(MIDI_DREQ)) ; // Wait for DREQ to go high indicating IC is available
	cs_low(); // Select control

	// SCI consists of instruction byte, address byte, and 16-bit data word.
	SPI.transfer(0x03);  // Read instruction
	SPI.transfer(addressbyte);

	resultvalue.byte[1] = SPI.transfer(0xFF); // Read the first byte
	while(!digitalRead(MIDI_DREQ)) ; // Wait for DREQ to go high indicating command is complete
	resultvalue.byte[0] = SPI.transfer(0xFF); // Read the second byte
	while(!digitalRead(MIDI_DREQ)) ; // Wait for DREQ to go high indicating command is complete

	cs_high(); // Deselect Control

	return resultvalue.word;
}

