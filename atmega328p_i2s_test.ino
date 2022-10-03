#include <avr/io.h>
#include "delay_in_cycles.hpp"
#include "i2s_driver.hpp"


//    10 |
//              20 |
//                                  40 |
//                                                                          80 |


const uint8_t SAMPLE_WIDTH = 8;


int main() {
  I2SDriver<SAMPLE_WIDTH> driver;
  const uint8_t HALF_SQUARE_WAVE_SAMPLES =
    F_CPU / (driver.FRAME_PERIOD * 440L);
  // Both timer 0 and 2's counters should read 0.
  // DO NOT REPLACE 7 WITH MULTIPLES OF 8 (OR 0)
  delayInCycles<driver.SAMPLE_PERIOD>();
  // Timer 2's counter should read 0 (prescaler 0).
  for (;;) {
    /*
     * Before entering the loop, the compiler had to initialize 4 registers with
     * constants.
     * Because of this, timer 2 's counter should read 7 (prescaler 4).
     */
    for (uint8_t i = 0; i < HALF_SQUARE_WAVE_SAMPLES; i++) {
      // Timer 2's counter should read 7 (prescaler 5).
      bitSet(PORTB, PORTB4);
      // Timer 2's counter should read 7 (prescaler 7).
      delayInCycles<driver.BIT_PERIOD - 2>();
      /*
       * Timer 2's counter should read 1 (one overflow, prescaler 5 with two
       * overflows).
       */
      bitClear(PORTB, PORTB4);
      /*
       * Timer 2's counter should read 1 (prescaler 7).
       * Also, it takes 3 cycles to start a new loop run.
       */
      delayInCycles<driver.SAMPLE_PERIOD - driver.BIT_PERIOD - 5>();
    }
    /*
     * Timer 2's counter should read 7 (prescaler 4).
     * Also, it takes 2 cycles to go back to the start of the outer loop.
     */
    delayInCycles<HALF_SQUARE_WAVE_SAMPLES * driver.SAMPLE_PERIOD - 2>();
  }
  return 0;
}
