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
     * Based on some experiments, both timers instantly switch their pin to high
     * as soon as we stop synchronization mode (by default). In fact:
     * - if we change the input line as soon as timer 2's counter reads an even
     *   number (and prescaler 0), the audio starts glitching (because we're
     *   violating the minimum setup and hold time);
     * - if we start sending bytes right after starting the timers, then send
     *   nothing after timer 2 overflows, then sending back stuff when it
     *   overflows again, etc., we get sounds only on the right channel (word
     *   select set to 1 => right channel data).
     * This reveals a major flaw: when the word select changes, the bit clock
     * line goes high (instead of low).
     */
  }
};
