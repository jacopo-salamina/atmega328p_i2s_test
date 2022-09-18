# atmega328p_i2s_test

Test I2S implementation on the ATmega328P microcontroller

## Preface

The ATMega328P lacks a hardware I2S transmitter, so I2S has to be implemented
via software. This program attempts to recreate a valid signal, which can then
be read by any compatible receiver.

As explained below, the initial design involves two timers for the bit clock and
word select signals, while the data will be pushed one bit at a time via
software (as of version 1). In later versions, I'll be experimenting with new
techniques, in the hopes of finding the least CPU-intensive one.

No matter the techniques employed, the microcontroller has plenty of computing
power for a simple I2S signal. Thus, the program will also attempt to generate
various audio signals, using more or less CPU-intensive algorithms. Overall, the
idea behind this project is to see how far we can push the ATmega328P in terms
of computing power, while still generating a meaningful I2S signal.

Lastly, this project serves as a testbed for my embedded programming skills.

## Requirements

- Arduino IDE version 1.8.19, for compiling and uploading the program;
- any board compatible with Arduino Uno R3, for generating the signal;
- any component compatible with I2S and 5V logic, for receiving the signal.

## Setup

- Connect the following pins of the board to the I2S receiver:
  - pin 6: serial clock;
  - pin 11: word select;
  - pin 12: serial data;
- make sure the AVR boards have been installed on Arduino IDE;
- before starting Arduino IDE, copy `boards.local.txt` to the AVR board's
  folder;
  - on Linux the folder is usually
    `~/.arduino15/packages/arduino/hardware/avr/<board-version>`;
  - when _Arduino Uno_ is selected as a board, `boards.local.txt` will add two
    new options under _Tools_: _C++ standard_ and _Optimization flags_;
- when you open the project, make sure the following options from _Tools_ are
  selected:
  - _Board_: _Arduino Uno_;
  - _C++ standard_: _C++17_;
  - _Optimization flags_: _Speed (-O3)_;
  - _Programmer_: _AVRISP mkII_.

## Compiling/running

Simply connect the board to the PC, then upload the sketch using Arduino IDE.
