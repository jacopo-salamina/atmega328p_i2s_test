#pragma once

#include "i2s_driver.hpp"


template<uint8_t FREQUENCY_DIVIDER = 1>
class SquareWaveGenerator {
private:
  static const uint32_t PERIOD = F_CPU * FREQUENCY_DIVIDER;
  uint8_t amplitude;
  uint32_t ticksIncrement;
  uint8_t currentAmplitude;
  uint32_t elapsedTicks;
public:
  template<auto... Args>
  SquareWaveGenerator(
    const I2SDriver<Args...>& driver,
    uint8_t amplitude,
    uint32_t frequency
  ) :
    amplitude(amplitude),
    ticksIncrement(driver.FRAME_PERIOD * frequency),
    currentAmplitude(0),
    elapsedTicks(0)
  {}

  /*
   * Determining the number of cycles needed by 'getNextSample()' inside
   * 'main()' is not trivial. There are multiple sections:
   * - an 'rjmp' instruction which is only run upon the first invocation (2
   *   cycles);
   * - a 'mov' instruction which the aforementioned 'rjmp' skips on purpose,
   *   which means 'mov' is run only on subsequent calls (1 cycle);
   * - a section which is always run (13 cycles);
   * - the section which makes sure 'elapsedTicks' doesn't exceed 'PERIOD' (7
   *   cycles);
   *   - this part includes an if-else, and during the first tests each branch
   *     took a different amount of cycles, which made it impossible to reliably
   *     count cycles; however, thanks to a couple of forced delays and some
   *     help from the compiler, both branches now take the same number of
   *     cycles;
   * - another section which is always run (9 cycles).
   * This means that, when 'getNextSample()' is called the first time, it takes
   * 31 cycles, and the other times it takes 30 cycles.
   * TODO Get rid of 'currentAmplitude'
   */
  uint8_t getNextSample() {
    uint8_t sample = currentAmplitude;
    elapsedTicks += ticksIncrement;
    if (elapsedTicks >= PERIOD) {
      elapsedTicks -= PERIOD;
      delayInCycles<1>();
    } else {
      delayInCycles<1>();
    }
    currentAmplitude = elapsedTicks >= PERIOD / 2 ? amplitude : 0;
    return sample;
  }
};
