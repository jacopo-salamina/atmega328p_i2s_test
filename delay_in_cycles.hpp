#pragma once

/*
 * When writing programs which require perfectly timed operations, we usually
 * need to generate clock-accurate delays. This header provides a template
 * function which does just that, 'delayInCycles()'.
 */

#include <util/delay_basic.h>
#include "type_traits_clone.hpp"

/**
 * Template function which does nothing for exactly 'T' CPU cycles.
 * 
 * Only template parameter: 'T', the number of cycles needed for the generated
 * function to compete. 'T' must be strictly positive and less than 3 * 256.
 * 
 * Generated functions have no arguments.
 * 
 * Generated functions work as intended when the following requirements are met:
 * - the compiler always inlines the functions;
 * - for 'n' strictly positive, running '_delay_loop_1(n)' takes exactly 3 * n
 *   cycles;
 * - '_NOP()' macros are never optimized out.
 * 
 * So far, when enabling optimization for space, all these requirements have been
 * met.
 * Usually there's no need to generate the same exact delay in multiple pieces of
 * code, which means there will never be a generated function "shared" between
 * multiple sites. This helps with the inlining requirement.
 * '_delay_loop_1(n)' is usually compiled using three assembly instructions:
 * 'mov', 'subi', 'brne'. '_delay_loop_1(n)' will then spend CPU cycles like
 * this:
 * - 1 cycle for initializing the looping variable ('mov');
 * - 3 * (n - 1) cycles spent on the first n - 1 loop runs;
 *   - n - 1 cycles on 'subi';
 *   - 2 * (n - 1) cycles on 'brne' (including the branch misprediction penalty);
 * - 2 cycles spent on the last loop run;
 *   - 1 cycle on 'subi';
 *   - 1 cycle on 'brne' (no branch misprediction penalty).
 * '_NOP()' macros have never been optimized out so far.
 * 
 * One last note about the allowed range of 'T'.
 * 0 would produce an empty function, which simply produces no delay. I felt this
 * might be useful in contexts where the parameter value is the result of an
 * expression which involves other constants.
 * If one of these constants (which could represent some external delay) is
 * changed every now and then, this could potentially cause 'T' to be set to 0,
 * without the developer even realizing it. A delay of 0 cycles is perfectly
 * acceptable.
 * Of course, a negative delay doesn't make sense, which is why 'T' was declared
 * as unsigned.
 * The upper bound of 'T' is interesting. If we did allow the value 3 * 256, the
 * argument of '_delay_loop_1(n)' would be 0 (the result of casting 256 to an
 * unsigned byte). Unfortunately, based on some experiments, the compiler's
 * output is less consistent in this scenario. Sometimes it still generates the
 * instructions 'mov', 'subi' and 'brne', and it takes exactly 3 * 256 cycles
 * (the first loop run causes an underflow which sets the counter to 255). Other
 * times, it omits the first 'mov' instruction and recycles a register which has
 * just been zeroed, making the delay one CPU cycle shorter.
 */
template<
  uint16_t T//,
  // enable_if_t<(T < 3 * 256), int> = 0
>
inline void delayInCycles() {
  if (T < 3 * 256) {
    if (T >= 3) {
      _delay_loop_1(T / 3);
    }
    if (T % 3 > 1) {
      _NOP();
    }
    if (T % 3 > 0) {
      _NOP();
    }
  } else {
    _delay_loop_2(T / 4);
    if (T % 4 > 2) {
      _NOP();
    }
    if (T % 4 > 1) {
      _NOP();
    }
    if (T % 4 > 0) {
      _NOP();
    }
  }
}
