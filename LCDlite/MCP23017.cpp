/* This is a library for the MCP23017 i2c port expander
*/

#include <Wire.h>
#include <avr/pgmspace.h>
#include "MCP23017.h"
#include "Arduino.h"

static inline void wiresend(uint8_t x) {
	Wire.write((uint8_t)x);
}

static inline uint8_t wirerecv(void) {
	return Wire.read();
}

void MCP23017::begin(uint8_t addr) {
	if (addr > 7) {
		addr = 7;
	}
	i2caddr = addr;

	Wire.begin();


	// set defaults!
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(MCP23017_IODIRA);
	wiresend(0xFF);  // all inputs on port A
	Wire.endTransmission();

	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(MCP23017_IODIRB);
	wiresend(0xFF);  // all inputs on port B
	Wire.endTransmission();
}

void MCP23017::begin(void) {
	begin(0);
}

void MCP23017::pinMode(uint8_t p, uint8_t d) {
	uint8_t iodir;
	uint8_t iodiraddr;

	// only 16 bits!
	if (p > 15)
		return;

	if (p < 8)
		iodiraddr = MCP23017_IODIRA;
	else {
		iodiraddr = MCP23017_IODIRB;
		p -= 8;
	}

	// read the current IODIR
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(iodiraddr);	
	Wire.endTransmission();

	Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
	iodir = wirerecv();

	// set the pin and direction
	if (d == INPUT) {
		iodir |= 1 << p;
	} else {
		iodir &= ~(1 << p);
	}

	// write the new IODIR
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(iodiraddr);
	wiresend(iodir);
	Wire.endTransmission();
}

uint16_t MCP23017::readGPIOAB() {
	uint16_t ba = 0;
	uint8_t a;

	// read the current GPIO output latches
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(MCP23017_GPIOA);
	Wire.endTransmission();

	Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 2);
	a = wirerecv();
	ba = wirerecv();
	ba <<= 8;
	ba |= a;

	return ba;
}

void MCP23017::writeGPIOAB(uint16_t ba) {
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(MCP23017_GPIOA);
	wiresend(ba & 0xFF);
	wiresend(ba >> 8);
	Wire.endTransmission();
}

void MCP23017::digitalWrite(uint8_t p, uint8_t d) {
	uint8_t gpio;
	uint8_t gpioaddr, olataddr;

	// only 16 bits!
	if (p > 15)
		return;

	if (p < 8) {
		olataddr = MCP23017_OLATA;
		gpioaddr = MCP23017_GPIOA;
	} else {
		olataddr = MCP23017_OLATB;
		gpioaddr = MCP23017_GPIOB;
		p -= 8;
	}

	// read the current GPIO output latches
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(olataddr);
	Wire.endTransmission();

	Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
	gpio = wirerecv();

	// set the pin and direction
	if (d == HIGH) {
		gpio |= 1 << p;
	} else {
		gpio &= ~(1 << p);
	}

	// write the new GPIO
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(gpioaddr);
	wiresend(gpio);
	Wire.endTransmission();
}

void MCP23017::pullUp(uint8_t p, uint8_t d) {
	uint8_t gppu;
	uint8_t gppuaddr;

	// only 16 bits!
	if (p > 15)
		return;

	if (p < 8)
		gppuaddr = MCP23017_GPPUA;
	else {
		gppuaddr = MCP23017_GPPUB;
		p -= 8;
	}


	// read the current pullup resistor set
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(gppuaddr);
	Wire.endTransmission();

	Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
	gppu = wirerecv();

	// set the pin and direction
	if (d == HIGH) {
		gppu |= 1 << p; 
	} else {
		gppu &= ~(1 << p);
	}

	// write the new GPIO
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(gppuaddr);
	wiresend(gppu);
	Wire.endTransmission();
}

uint8_t MCP23017::digitalRead(uint8_t p) {
	uint8_t gpioaddr;

	// only 16 bits!
	if (p > 15)
		return 0;

	if (p < 8)
		gpioaddr = MCP23017_GPIOA;
	else {
		gpioaddr = MCP23017_GPIOB;
		p -= 8;
	}

	// read the current GPIO
	Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
	wiresend(gpioaddr);
	Wire.endTransmission();

	Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
	return (wirerecv() >> p) & 0x1;
}
