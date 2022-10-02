#include <avr/io.h>

class I2SDriver {
private:
  void configureTimer0() {
    // Fast PWM (mode 7, continues below), prescaler set to 8
    TCCR0B = bit(WGM02) | bit(CS01);
    // OC0A toggle mode operation, OC0B normal mode operation, fast PWM (mode 7)
    TCCR0A = bit(COM0A0) | bit(WGM01) | bit(WGM00);
    TCNT0 = 0;
    OCR0A = 0;
  }

  void configureTimer2() {
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
    OCR2A = 15;
    // Clear the interrupt flags
    TIFR2 = 0;
    // SAFE PROCEDURE END (we don't need interrupts)
  }
public:
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
    DDRB |= bit(DDD3) | bit(DDD4);
    bitClear(GTCCR, TSM);
    /*
     * Based on some experiments, after stopping synchronization mode, both 
     * timers stay still for 16 CPU cycles, then their respective counters
     * get increased (with overflow detection). Moreover, both output compare
     * pins are set to 0 within the first 16 cycles. In fact:
     * - if we change the input line as soon as timer 2's counter reads an odd
     *   number (and prescaler 0), the audio starts glitching (because that's
     *   when timer 0 outputs a rising edge, thus we're violating the minimum
     *   setup and hold time);
     * - if we start sending bytes right after starting the timers, then send
     *   nothing after timer 2 overflows, then sending back stuff when it
     *   overflows again, etc., we get sounds only on the left channel (word
     *   select set to 0 => left channel data), except then the very first bit
     *   is set to 1 (that bit is actually the LSB of the value sent to the right
     *   channel);
     * - as for the 16 cycles initial delay, I couldn't figure out the reason for
     *   the first 8 cycles, but the last 8 make sense, since that's the time
     *   needed for the prescalers to produce a new timer tick; if TCNT0 is set
     *   to 255 the 16 cycles delay still applies (after the very first timer
     *   tick, the counter overflows, goes back to 0 and the output pin is
     *   toggled), while setting TCNT0 to 254 brings the delay to 24 cycles (the
     *   first timer tick moves the counter to 255, so there's no overflow, while
     *   the second tick makes the counter overflow and the output pin toggle).
     * We got lucky: when the word select changes, the bit clock line goes low,
     * as the I2S protocol specifies.
     */
  }
};
