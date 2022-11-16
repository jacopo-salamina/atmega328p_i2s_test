#include <avr/io.h>
#include "delay_in_cycles.hpp"
#include "i2s_driver.hpp"
#include "wave_generators.hpp"


//    10 |
//              20 |
//                                  40 |
//                                                                          80 |


int main() {
  /*
   * Setting HALF_BIT_PERIOD to 5 results in a sampling rate of 50 kHz (good
   * enough for frequencies up to 25 kHz, well beyond the typical human ear
   * range).
   * 
   * BUSY_EXTERNAL_CYCLES_BEFORE_FIRST_BUFFER_WRITE takes into account:
   * - the initialization of generator (3 cycles);
   * - generator.getFirstSample() (2 cycles);
   * - two constants for generating delays (2 cycles).
   */
  using MyDriver = I2SDriver<5, 7>;

  /*
   * The various constructors and function calls have been reordered to match the
   * actual machine code. Moving the initialization of generator and the call to
   * generator.getFirstSample() had no effect (probably because there are no side
   * effects involved, and the compiler chose to recycle some registers).
   */
  MyDriver driver;
  delayInCyclesWithNOP<
    driver.OTHER_EXTERNAL_DELAY_CYCLES_BEFORE_FIRST_BUFFER_WRITE
  >();
  SquareWaveGenerator<MyDriver::FRAME_PERIOD> generator(440, 16);
  int16_t sample = generator.getFirstSample();
  for (;;) {
    driver.sendSample(sample);
    delayInCyclesWithLoop<driver.SAMPLE_PERIOD - driver.SEND_SAMPLE_DURATION>();
    driver.sendSample(sample);
    delayInCyclesWithLoop<
      driver.SAMPLE_PERIOD
      - driver.SEND_SAMPLE_DURATION
      - generator.GET_NEXT_SAMPLE_DURATION
    >();
    sample = generator.getNextSample();
  }
  return 0;
}
