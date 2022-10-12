#include <avr/io.h>
#include "delay_in_cycles.hpp"
#include "i2s_driver.hpp"
#include "wave_generators.hpp"


//    10 |
//              20 |
//                                  40 |
//                                                                          80 |


const uint8_t SAMPLE_WIDTH = 12;


int main() {
  I2SDriver<SAMPLE_WIDTH> driver;
  /*
   * generator.PERIOD_IN_TICKS is 0x00F42400 (16e6), half
   * generator.PERIOD_IN_TICKS is 0x007A1200, generator.ticksIncrement is
   * 0x00029400 (168960) and the 2's complement of generator.ticksIncrement is
   * 0xFFFD6C00 (it will be used later).
   * Also, the compiler decided to initialize generator later, right before the
   * main loop.
   */
  SquareWaveGenerator generator(driver, 16, 440);
  driver.start();
  // Both timer 0 and 2's counters should read 0.
  delayInCycles<driver.BIT_PERIOD * 5 + 8 - 41>();
  /*
   * Timer 2's counter should read 4 (prescaler 7).
   * Right before starting the infinite loop, the compiler initializes:
   * - generator.elapsedTicks, I think... (3 cycles);
   * - sample, because the compiler knows its first value is 0 (1 cycle);
   * - three constants for delayInCycles() (3 cycles).
   * After that, timer 2's counter should read 5 (prescaler 6).
   */
  for (;;) {
    uint8_t sample = generator.getNextSample();
    /*
     * After the first call to generator.getNextSample() (which takes 31
     * cycles), timer 2's counter should read 9 (prescaler 5). Also, subsequent
     * invocations take 30 cycles each.
     */
    driver.sendSample(sample);
    /*
     * As soon as the first bit is sent, timer 2's counter should read 10
     * (prescaler 0).
     * By the time driver.sendSample(sample) completes, timer 2's counter should
     * read 2 (overflowed once, prescaler 0).
     */
    delayInCycles<driver.BIT_PERIOD * 4 - 3>();
    // Timer 2's counter should read 9 (prescaler 5).
    driver.sendSample(sample);
    /*
     * Again, as soon as the first bit is sent, timer 2's counter should read 10
     * (prescaler 0).
     * By the time driver.sendSample(sample) completes, timer 2's counter should
     * read 2 (overflowed once, prescaler 0).
     * The additional delay needed for the processor to jump back at the start
     * of the loop is already integrated in generator.getNextLoop().
     */
    delayInCycles<driver.BIT_PERIOD * 4 - 33>();
  }
  return 0;
}
