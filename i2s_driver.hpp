#pragma once

#include <avr/io.h>
#include "type_traits_clone.hpp"


/**
 * Template class used to configure and start the I2S-related clock signals (bit
 * clock and word select).
 * 
 * The clock signals are produced using two timers; timer 0 generates the bit
 * clock signal, while timer 2 generates the word select signal. Both timers
 * run with a clock divider of 8, and their output compare units work in toggle
 * mode (each unit's state is toggled once its timer's counter goes back to 0).
 * Moreover, timer 0 is configured with OCR0A = 0, which gives us a clock
 * signal of 1 MHz.
 * The above configuration was chosen for producing a fixed bitrate of 1.000.000
 * bits/s, which lets us produce audio signals with an acceptable sample range
 * and sample rate; moreover, since both timers are using the prescaler,
 * starting and stopping them is rather straightforward, thanks to the GTCCR
 * register.
 * 
 * At the moment, the only external configuration parameter is SAMPLE_WIDTH,
 * which is the number of bits available for a single sample (per single
 * channel). Since the bitrate is fixed, raising SAMPLE_WIDTH will always lower
 * the sample rate, and viceversa. I usually set this value to 12 bits, which
 * gives me a sample rate of 1.000.000 / (2 * 12) ~= 41.6 kHz.
 * 
 * Additionally, this class provides some constants which represent various
 * clock-related periods (in CPU cycles) and come in handy during cycle
 * counting and generating signals with an accurate frequency.
 */
template<uint8_t SAMPLE_WIDTH>
class I2SDriver {
private:
  uint8_t currentPortB;
  
  static void configureTimer0() {
    // Fast PWM (mode 7, continues below), prescaler set to 8
    TCCR0B = bit(WGM02) | bit(CS01);
    // OC0A toggle mode operation, OC0B normal mode operation, fast PWM (mode 7)
    TCCR0A = bit(COM0A0) | bit(WGM01) | bit(WGM00);
    TCNT0 = 0;
    OCR0A = 0;
  }

  static void configureTimer2() {
    /*
     * For timer 2 we'll be using a "safe procedure" described in section 17.9
     * for switching to/from asynchronous mode.
     */
    // SAFE PROCEDURE START
    // Disable interrupts.
    TIMSK2 = 0;
    // Enable synchronous mode
    bitClear(ASSR, AS2);
    // Configure TCNT2, OCR2x and TCCR2x
    // Fast PWM (mode 7, continues below), prescaler set to 8
    TCCR2B = bit(WGM22) | bit(CS21);
    // OC2A toggle mode operation, OC2B normal operation mode, fast PWM (mode 7)
    TCCR2A = bit(COM2A0) | bit(WGM21) | bit(WGM20);
    TCNT2 = 0;
    OCR2A = SAMPLE_WIDTH * 2 - 1;
    // Clear the interrupt flags
    TIFR2 = 0;
    // SAFE PROCEDURE END (we don't need interrupts)
  }

  /*
   * All it does is just:
   * destinationByte =
   *   destinationByte
   *   & ~bit(DESTINATION_BIT)
   *   | (bitRead(sourceByte, SOURCE_BIT) << DESTINATION_BIT);
   * but using just a couple of ASM instructions (since apparently the compiler
   * doesn't understand it...).
   */
  template<
    uint8_t DESTINATION_BIT,
    uint8_t SOURCE_BIT,
    enable_if_t<DESTINATION_BIT < 8 && SOURCE_BIT < 8, int>
      = 0
  >
  static inline void copyBitInAssembly(
    uint8_t& destinationByte, const uint8_t sourceByte
  ) {
    asm volatile (
      "bst %1, %3" "\n\t"
      "bld %0, %2" "\n\t"
      : "+r" (destinationByte)
      : "r" (sourceByte), "I" (DESTINATION_BIT), "I" (SOURCE_BIT)
    );
  }
  
  template<uint8_t BITS = 7>
  inline void sendBitsInAssembly(const uint8_t sample) {
    copyBitInAssembly<PORTB4, BITS>(currentPortB, sample);
    PORTB = currentPortB;
    if (BITS > 0) {
      delayInCycles<BIT_PERIOD - 3>();
      sendBitsInAssembly<(BITS - 1) & 0x07>(sample);
    }
  }
public:
  /// The number of CPU cycles between two consecutive bits
  static const uint8_t BIT_PERIOD = 16;
  /**
   * The number of CPU cycles between two consecutive audio samples (keep in
   * mind that a sample has to be produced for each channel)
   */
  static const uint16_t SAMPLE_PERIOD = BIT_PERIOD * SAMPLE_WIDTH;
  /**
   * The number of CPU cycles between two consecutive audio frames (a frame
   * consists of a pair of audio samples which will be played at the same time;
   * the first sample is for the left channel and the second one is for the
   * right channel)
   */
  static const uint16_t FRAME_PERIOD = SAMPLE_PERIOD * 2;

  /**
   * Note that 'currentPortB' was default-initialized on purpose: PORTB's value
   * is retrieved at the very end of the constructor, when the port is already
   * configured.
   */
  I2SDriver() {
    noInterrupts();
    GTCCR = bit(TSM) | bit(PSRASY) | bit(PSRSYNC);
    configureTimer0();
    configureTimer2();
    // Set pin n. 6 as output (for data clock)
    bitSet(DDRD, DDD6);
    /*
     * Set pin n. 11 and 12 as output; the former is needed for word select
     * clock, while the latter for sending the actual data.
     */
    DDRB |= bit(DDB3) | bit(DDB4);
    currentPortB = PORTB;
  }

  /**
   * Based on some experiments, after stopping synchronization mode, both timers
   * stay still for 16 CPU cycles, then their respective counters get increased
   * (with overflow detection). Moreover, both output compare pins are set to 0
   * within the first 16 cycles. In fact:
   * - if we change the input line as soon as timer 2's counter reads an odd
   *   number (and prescaler 0), the audio starts glitching (because that's when
   *   timer 0 outputs a rising edge, thus we're violating the minimum setup and
   *   hold time);
   * - if we start sending bytes right after starting the timers, then send
   *   nothing after timer 2 overflows, then sending back stuff when it
   *   overflows again, etc., we get sounds only on the left channel (word
   *   select set to 0 => left channel data), except then the very first bit is
   *   set to 1 (that bit is actually the LSB of the value sent to the right
   *   channel);
   * - as for the 16 cycles initial delay, I couldn't figure out the reason for
   *   the first 8 cycles, but the last 8 make sense, since that's the time
   *   needed for the prescalers to produce a new timer tick; if TCNT0 is set to
   *   255 the 16 cycles delay still applies (after the very first timer tick,
   *   the counter overflows, goes back to 0 and the output pin is toggled),
   *   while setting TCNT0 to 254 brings the delay to 24 cycles (the first timer
   *   tick moves the counter to 255, so there's no overflow, while the second
   *   tick makes the counter overflow and the output pin toggle).
   * We got lucky: when the word select changes, the bit clock line goes low,
   * as the I2S protocol specifies.
   */
  void start() {
    bitClear(GTCCR, TSM);
  }

  /**
   * The first version of 'sendSample(sample)' featured a for-loop where each
   * bit of 'sample' was sent to bit n. 4 of PORTB. However, the compiler
   * couldn't optimize it very well, so I had to manually write a dedicated
   * assembly routine which uses 'bst' and 'bld'. Since these two instructions
   * use constants for targeting a specific bit, there was no way of using the
   * loop variable; thus, the original loop was replaced by a recursive template
   * function.
   */
  void sendSample(const uint8_t sample) {
    sendBitsInAssembly(sample);
    delayInCycles<BIT_PERIOD - 2>();
    bitClear(currentPortB, PORTB4);
    PORTB = currentPortB;
  }
};
