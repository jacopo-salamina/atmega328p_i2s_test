#include <avr/io.h>
#include "delay_in_cycles.hpp"
#include "i2s_driver.hpp"


//    10 |
//              20 |
//                                  40 |
//                                                                          80 |


const uint8_t halfSquareWaveSamples = F_CPU / (16L * 8 * 2 * 440);


int main() {
  I2SDriver driver;
  // Both timer 0 and 2's counters should read 0.
  // DO NOT REPLACE 7 WITH MULTIPLES OF 8 (OR 0)
  delayInCycles<16 * 8>();
  // Timer 2's counter should read 0 (prescaler 0).
  for (;;) {
    /*
     * Before entering the loop, the compiler had to initialize 4 registers with
     * constants.
     * Because of this, timer 2 's counter should read 7 (prescaler 4).
     */
    for (uint8_t i = 0; i < halfSquareWaveSamples; i++) {
      // Timer 2's counter should read 7 (prescaler 5).
      bitSet(PORTB, PORTB4);
      // Timer 2's counter should read 7 (prescaler 7).
      delayInCycles<14>();
      /*
       * Timer 2's counter should read 1 (one overflow, prescaler 5 with two
       * overflows).
       */
      bitClear(PORTB, PORTB4);
      /*
       * Timer 2's counter should read 1 (prescaler 7).
       * Also, it takes 3 cycles to start a new loop run.
       */
      delayInCycles<107>();
    }
    /*
     * Timer 2's counter should read 7 (prescaler 4).
     * Also, it takes 2 cycles to go back to the start of the outer loop.
     */
    delayInCycles<halfSquareWaveSamples * 128 - 2>();
  }
  return 0;
}
