arduino-libs
============

These are lightweight implementations of common library functions, customized to only include the set of functions required for a particular project. This approach allows trading off between capability, performance, and total size.

Current libraries include:
* LCDlite: control 2-line LCD and 5 input buttons over I2C
* SDlite: Provide read-only access to the FAT filesystem on an SD card
* MidiSynth: Utilize a VS1053 DSP as a real-time MIDI synthesizer

A performance comparison between stock libraries and these lightweight replacements can be seen in stripped-lib-performance.md.
