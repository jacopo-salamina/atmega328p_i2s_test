# atmega328p_i2s_test

Test I2S implementation on the ATmega328P microcontroller

## Preface

The ATMega328P lacks a hardware I2S transmitter, so I2S has to be implemented
via software. This program attempts to recreate a valid signal, which can then
be read by any compatible receiver.

As explained below, the initial design involves two timers for the bit clock and
word select signals, while the data is pushed one bit at a time via software (as
of version 1). In later versions, I'll be experimenting with new techniques, in
the hopes of finding the least CPU-intensive one.

No matter the techniques employed, the microcontroller has plenty of computing
power for a simple I2S signal. Thus, the program will also attempt to generate
various audio signals, using more or less CPU-intensive algorithms. Overall, the
idea behind this project is to see how far we can push the ATmega328P in terms
of computing power, while still generating a meaningful I2S signal.

Lastly, this project serves as a testbed for my embedded programming skills.

## Requirements

- Arduino IDE version 1.8.19, for compiling and uploading the program;
- AVR boards already installed on Arduino IDE;
- any board compatible with Arduino Uno R3, for generating the signal;
- any component compatible with I2S and 5V logic, for receiving the signal.

## Setup

- Connect the following pins of the board to the I2S receiver:
  - pin 6: serial clock;
  - pin 11: word select;
  - pin 12: serial data;
- make sure the microcontroller is using the external 16 MHz oscillator as its
  clock source;
  - if you've never programmed the microcontroller's fuse bits, this is the
    default option and you don't need to do anything;
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

## Technical details

### Timers configuration and pin usage

On the microcontroller, timer 0 is used to generate the serial clock signal, and
is configured like this:

- fast PWM mode (with `OCR0A` register set as the counter's top value);
- `OCR0A` set to 0;
- counter set to 0;
- `OC0A` pin configured in toggle mode (its value toggles every time the counter
  is reset);
- `OC0B` pin free for other uses;
- prescaler used, divider set to 8.

This produces a bit clock signal of 1 MHz, with a duty cycle of 50%. Pin 6 is
reserved as the output compare pin, and that's the reason why that pin is used
for the serial clock.

Timer 2 is used to generate the serial clock signal, and was configured
similarly:

- asynchronous mode disabled;
- fast PWM mode (with `OCR2A` register set as the counter's top value);
- `OCR2A` set to 15;
- `OC2A` pin configured in toggle mode (its value toggles every time the counter
  is reset);
- `OC2B` pin free for other uses;
- prescaler used, divider set to 8.

This produces a word select signal of 62.5 kHz, with a duty cycle of 50%.  With
this configuration, pin 11 is reserved as the output compare pin, and that's the
reason why that pin was used for the word select.

The data signal is produced via software, by manually setting or clearing a pin.
In theory, any pin could be used for the data signal, except 6 and 11 (since
they're already busy for the clock signals). Pin 12 was chosen because that pin
is reserved for the MOSI line when using the hardware-implemented SPI interface,
and one of the project's future design changes might involve using the SPI for
the data signal.

According to I2S' specifications, the word select signal must change during the
bit clock's falling edge. New revisions will ensure this by configuring the
output compare registers before starting the timers.

### Timers synchronization

During the early tests I managed to configure both timers in fast PWM mode
without any prescaler. This choice, however, had a problem: timers'
synchronization was not trivial.

As soon as a timer is assigned the regular system clock as source, it starts
counting. Now, when both timers are being configured without prescaler, the
second timer's clock source has to be selected after the first one's, but the
latter has already started counting. To work around this, early implementations
had to set the second timer's counter to the number of CPU cycles it took to
start that timer.

Things change if a timer is assigned the prescaler as source instead (no matter
the divider): the timer starts counting, unless the bit `TSM` on `GTCCR` is set.
We can use this to start both timers at the exact same time:

- set `TSM` on `GTCCR` (this will stop every prescaler);
- set both `PSRASY` and `PSRSYNC` on `GTCCR` (this will reset the prescalers'
  internal counters);
- configure the timers, assigning the prescalers as clock source (each timer can
  be configured with a different divider);
- clear `TSM` on `GTCCR` (this will start both prescalers, and timers as well).

### Audio quality

The word select signal's period lasts enough for transmitting 16 bits, 8 when
the signal stays low, 8 when the signal stays high. This means we're producing
an audio signal with 8 bit samples, 2 channels and a sampling rate of 62.5 kHz.
The generated audio is loud, and attempting to generate audio signals with a
lower volume severely limits the samples' range; also, the sampling rate is well
beyond the usual 44.1 kHz rate (the human ear cannot tell the difference).
Future versions will attempt to lower the sample rate and increase the samples'
range.

### Other details

After setting up and starting the timers, the instructions which set/clear the
data pin have to be perfectly synchronized with timer 0, because of I2S'
requirements. This involves a lot of cycle counting and manual delays, and
the delays' implementation could be improved in terms of readability.

Besides `boards.local.txt`, there's another file which is not directly involved
with the compilation: `disassembler-output.txt`. This file is the disassembled
version of the executable generated, mixed with some actual code lines as a
reference. Changes made to the program will be reflected in the disassembly, and
keeping track of that helps spotting potential bugs or synchronization issues.
This file is generated by running `avr-objdump` on the executable and using the
flags `-dSz`; moreover, the above program's output is piped through `tail -n +3` in
order to strip the first 2 lines off (they contain the executable path, which is
usually a temporary file whose path changes from time to time, and thus is not
relevant to us).

Another objective I initially planned was ditching the Arduino IDE in favor of a
more direct usage of the AVR toolchain. However, there were a couple of
problems. First, early attempts at using the toolchain involved creating a
couple of shell scripts which merely reproduced the commands generated by the
IDE; this wasn't viable in the long run, as other people would be more
accustomed to advanced tools such as make and CMake. Second, the codebase still
heavily relies on important features of the IDE (macros for bit manipulation,
implicit includes), and reworking it would take additional time. Overall, the
objective, while interesting in the long run, would take a lot of time which
could be instead spent on improving the synthesizer itself.
