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
  using MyDriver = I2SDriver<SAMPLE_WIDTH>;
  
  MyDriver driver;
  /*
   * generator.PERIOD_IN_TICKS is 0x00F42400 (16e6), half
   * generator.PERIOD_IN_TICKS is 0x007A1200, generator.ticksIncrement is
   * 0x00029400 (168960) and generator.PERIOD_IN_TICKS minus
   * generator.ticksIncrement is 0x00F19000 (it will be used later).
   * Also, the compiler decided to initialize generator later, right before the
   * main loop.
   */
  SquareWaveGenerator<MyDriver::FRAME_PERIOD> generator(440, 16);
  driver.start();
  // Both timer 0 and 2's counters should read 0.
  delayInCyclesWithLoop<driver.BIT_PERIOD * 5 + 8 - 13>();
  /*
   * Timer 2's counter should read 8 (prescaler 4). Also sample is initialized
   * slightly later.
   */
  uint8_t sample = generator.getFirstSample();
  /*
   * Right before starting the infinite loop, the compiler initializes:
   * - generator.elapsedTicks, (3 cycles);
   * - sample (1 cycle);
   * - generator.amplitude (1 cycle);
   * - three constants for delayInCycles() (3 cycles).
   * After that, the CPU uses rjmp to skip a part of generator.getNextSample()
   * (2 cycles).
   * At this point, timer 2's counter should read 9 (prescaler 5).
   */
  for (;;) {
    driver.sendSample(sample);
    /*
     * As soon as the first bit is sent, timer 2's counter should read 10
     * (prescaler 0).
     * By the time driver.sendSample(sample) completes, timer 2's counter should
     * read 2 (overflowed once, prescaler 0).
     */
    delayInCyclesWithLoop<driver.BIT_PERIOD * 4 - 3>();
    // Timer 2's counter should read 9 (prescaler 5).
    driver.sendSample(sample);
    /*
     * Again, as soon as the first bit is sent, timer 2's counter should read 10
     * (prescaler 0).
     * By the time driver.sendSample(sample) completes, timer 2's counter should
     * read 2 (overflowed once, prescaler 0).
     * The additional delay needed for the processor to jump back at the start
     * of the loop is already integrated in generator.getNextSample().
     */
    delayInCyclesWithLoop<driver.BIT_PERIOD * 4 - 26>();
    sample = generator.getNextSample();
  }
  return 0;
}
