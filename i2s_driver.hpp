#pragma once

#include <avr/io.h>
#include "delay_in_cycles.hpp"
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
template<
  uint8_t HALF_BIT_PERIOD,
  uint8_t DELAY_CYCLES_BEFORE_1ST_TRANSMISSION,
  enable_if_t<HALF_BIT_PERIOD && HALF_BIT_PERIOD <= 8, int> = 0
>
class I2SDriver {
private:
  static void configureTimer0(const uint8_t cyclesForNextTransmission) {
    // Fast PWM (mode 7, continues below), don't start the timer yet
    TCCR0B = bit(WGM02);
    // OC0A toggle mode operation, OC0B normal mode operation, fast PWM (mode 7)
    TCCR0A = bit(COM0A0) | bit(WGM01) | bit(WGM00);
    OCR0A = SAMPLE_PERIOD - 1;
    TCNT0 = FULL_BIT_PERIOD - cyclesForNextTransmission - 2;
  }

  static void configureUSART() {
    // Set UBRR0 to 0 before enabling MSPIM mode
    UBRR0 = 0;
    /*
     * - MSPIM mode;
     * - MSB sent first;
     * - sample first (on rising edge), setup then (on falling edge).
     */
    UCSR0C = bit(UMSEL01) | bit(UMSEL00);
    // Only transmitter enabled.
    UCSR0B = bit(TXEN0);
    // UBRR0 finally configured; the internal counter starts counting down now.
    UBRR0 = HALF_BIT_PERIOD - 1;
  }

public:
  static const uint8_t FULL_BIT_PERIOD = HALF_BIT_PERIOD * 2;
  /**
   * The number of CPU cycles between two consecutive audio samples (keep in
   * mind that a sample has to be produced for each channel)
   */
  static const uint16_t SAMPLE_PERIOD = FULL_BIT_PERIOD * 16;
  /**
   * The number of CPU cycles between two consecutive audio frames (a frame
   * consists of a pair of audio samples which will be played at the same time;
   * the first sample is for the left channel and the second one is for the
   * right channel)
   */
  static const uint16_t FRAME_PERIOD = SAMPLE_PERIOD * 2;

  static constexpr uint16_t getCyclesForNextTransmissionStart(
    const uint16_t elapsedCycles
  ) {
    uint16_t elapsedCyclesPlusBufferDelay = elapsedCycles + 2;
    constexpr uint8_t USARTFirstDelay = HALF_BIT_PERIOD + 1;
    if (elapsedCyclesPlusBufferDelay <= USARTFirstDelay) {
      return USARTFirstDelay - elapsedCyclesPlusBufferDelay;
    } else {
      uint16_t cyclesBeyondFirstTransmissionStart =
        elapsedCyclesPlusBufferDelay - USARTFirstDelay - 1;
      uint16_t transmissionsStarted =
        cyclesBeyondFirstTransmissionStart / FULL_BIT_PERIOD + 1;
      return
        transmissionsStarted
        * FULL_BIT_PERIOD
        + USARTFirstDelay
        - elapsedCyclesPlusBufferDelay;
    }
  }

  I2SDriver() {
    constexpr uint8_t cyclesForNextTransmission =
      DELAY_CYCLES_BEFORE_1ST_TRANSMISSION
      + 2
      + getCyclesForNextTransmissionStart(
        DELAY_CYCLES_BEFORE_1ST_TRANSMISSION + 2 + 3
      );
    
    noInterrupts();
    /*
     * Set pins 6 (timer 0's OC0A), 4 (USART clock) and 1 (USART TX) as outputs;
     * everything else can be left as input.
     */
    DDRD = bit(DDD6) | bit(DDD4) | bit(DDD1);
    configureTimer0(cyclesForNextTransmission);
    configureUSART();
    // Start timer 0 (without prescaler)
    bitSet(TCCR0B, CS00);
  }

  /**
   * The first version of sendSample(sample) featured a for-loop where each bit
   * of sample was sent to bit n. 4 of PORTB. However, the compiler couldn't
   * optimize it very well, so I had to manually write a dedicated assembly
   * routine which uses bst and bld. Since these two instructions use constants
   * for targeting a specific bit, there was no way of using the loop variable;
   * thus, the original loop was replaced by a recursive template function.
   */
  void sendSample(const int16_t sample) {
    UDR0 = int8_t(sample >> 8);
    delayInCyclesWithNOP<1>();
    UDR0 = uint8_t(sample & 0xff);
  }
};
