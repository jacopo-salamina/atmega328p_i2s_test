#include <avr/io.h>
#include "delay_in_cycles.hpp"
#include "i2s_driver.hpp"
#include "wave_generators.hpp"


//    10 |
//              20 |
//                                  40 |
//                                                                          80 |


int main() {
  using MyDriver = I2SDriver<5, 7>;

  MyDriver driver;
  SquareWaveGenerator<MyDriver::FRAME_PERIOD> generator(440, 16);
  int16_t sample = generator.getFirstSample();
  delayInCyclesWithNOP<driver.getCyclesForNextTransmissionStart(7 + 3 + 2)>();
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
    delayInCyclesWithLoop<driver.SAMPLE_PERIOD - 5>();
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
    delayInCyclesWithLoop<driver.SAMPLE_PERIOD - 5 - 25>();
    /*
     * - 6 (1st common part)
     * - 7 (1st branch)
     * - 6 (2nd common part)
     * - 6 (2nd branch)
     */
    sample = generator.getNextSample();
  }
  return 0;
}
