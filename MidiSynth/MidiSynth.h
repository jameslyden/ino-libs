/* MidiSynth.h
 * James Lyden <james@lyden.org>
 *
 * Stripped-down version of the SFEMP3Shield library, modified to adapt the
 * VS1053 for real-time MIDI synthesis
 */

#ifndef MidiSynth_h
#define MidiSynth_h

// include built-in libraries
#include "MidiSynthPins.h"
#include "SPI.h"
#include "Arduino.h"

//Add the SdFat Libraries
#include <SDlite.h>

extern SD sd;

// Registers
#define SCI_MODE              0x00
#define SCI_CLOCKF            0x03
#define SCI_VOL               0x0B

// Masks
#define SM_EARSPEAKER_LO    0x0010
#define SM_EARSPEAKER_HI    0x0080
#define SM_SDINEW           0x0800
#define SM_LINE1            0x4000

class MidiSynth {
	public:
		uint8_t begin();
		void end();
		void setVolume(uint8_t, uint8_t);
		uint16_t getVolume();
		uint8_t getEarSpeaker();
		void setEarSpeaker(uint16_t);

	private:
		uint8_t vs_init();
		static void cs_low();
		static void cs_high();
		static void dcs_low();
		static void dcs_high();
		static void MidiWriteRegister(uint8_t, uint8_t, uint8_t);
		static void MidiWriteRegister(uint8_t, uint16_t);
		static uint16_t MidiReadRegister (uint8_t);
		uint8_t VSLoadUserCode(char*);

		// Rate of the SPI to be used with communicating to the VSdsp.
		static uint16_t spiRate; 
		// contains a local value of the VSdsp's master volume left channels
		uint8_t VolL;
		// contains a local value of the VSdsp's master volume Right channels
		uint8_t VolR;
};

// Global structure to efficiently work with 16 bit words
union twobyte {
	uint16_t word;
	uint8_t  byte[2];
} ;

#endif // MidiSynth_h
