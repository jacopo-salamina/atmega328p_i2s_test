#pragma once

/*
 * When writing programs which require perfectly timed operations, we usually
 * need to generate clock-accurate delays. This header provides a template
 * function which does just that, delayInCycles().
 */

#include <util/delay_basic.h>
#include "type_traits_clone.hpp"


/**
 * Template function which does nothing for exactly T CPU cycles.
 * 
 * Only template parameter: T, the number of cycles needed for the generated
 * function to compete.
 * 
 * Generated functions have no arguments.
 * 
 * The delay is achieved by placing exactly T nop instructions. There are no
 * loops or computations involved, so the compiler doesn't need to store any
 * constant for instances of this template function. This is useful when we
 * don't want the program to waste more instructions, registers or the RAM
 * itself.
 * 
 * However, instances which generate considerable delays will make the final
 * executable larger, so usage of this template function should be limited to
 * short delays.
 * 
 * Some technical considerations.
 * 
 * Usually there's no need to generate the same exact delay in multiple pieces
 * of code, which means there will hardly be a generated function "shared"
 * between multiple sites. This helps with the inlining requirement.
 * 
 * _NOP() macros cannot be optimized out, as their only assembly instruction
 * has been annotated as volatile.
 * 
 * If T == 0, an empty function will be generated, which simply produces no
 * delay. I felt this might be useful in contexts where the parameter value is
 * the result of an expression which involves other constants. If one of these
 * constants (which could represent some external delay) is changed every now
 * and then, this could potentially cause T to be set to 0, without the
 * developer even realizing it. A delay of 0 cycles is perfectly acceptable.
 * 
 * Of course, a negative delay doesn't make sense, which is why T was declared
 * as unsigned.
 */
template<uint16_t T>
inline void delayInCyclesWithNOP() {
  if (T) {
    delayInCyclesWithNOP<T / 2>();
    delayInCyclesWithNOP<T / 2>();
    if (T % 2) {
      _NOP();
    }
  }
}


/**
 * Template function which does nothing for exactly T CPU cycles.
 * 
 * Only template parameter: T, the number of cycles needed for the generated
 * function to compete.
 * 
 * Generated functions have no arguments.
 * 
 * This template function works in a different way than delayInCyclesWithNOP().
 * It makes use of two other functions, _delay_loop_1(n) and _delay_loop_2(n).
 * These are forced assembly loops which take an argument n and (usually)
 * generate a delay of X * n cycles, where X is an integer which depends on the
 * value of N, as will be explained later. An additional number of nop
 * instructions may be added right after these loops, in order to cover the
 * remaining T - X * n delay cycles needed.
 * 
 * Since loops are being used, generated functions can be rather short in terms
 * of CPU instructions, while still being able to generate as many delay cycles
 * as needed.
 * 
 * However, since _delay_loop_1(n) and _delay_loop_2(n) need to be invoked with
 * a constant value, the compiler needs to store that constant somewhere, and
 * usually this is accomplished by storing it in a register (especially when
 * delayInCycles<T>() is used inside a loop). Storing that constant in a
 * register requires an additional instruction, whose placement is not easily
 * predictable and makes cycle counting harder. Because of this,
 * delayInCyclesWithNOP<T>() may be desired sometimes, especially for short
 * delays.
 * 
 * Some technical considerations.
 * 
 * Both forced delay loops (usually) consist of 3 assembly instructions: the
 * first one for copying the argument n inside a variable, the second one for
 * decrementing said variable, and the last one for jumping back to the second
 * instruction, unless the variable is 0. The only difference between the two
 * functions is the range of n: _delay_loop_1(n) needs a byte, while
 * _delay_loop_2(n) needs a short. Thus, the former will be used for shorter
 * delays, while the latter will be used in every other case.
 * 
 * _delay_loop_1(n) is usually compiled using the instructions mov, subi and
 * brne, and will spend CPU cycles like this:
 * - 1 cycle for initializing the looping variable (mov);
 * - 3 * (n - 1) cycles spent on the first n - 1 loop runs;
 *   - n - 1 cycles on subi;
 *   - 2 * (n - 1) cycles on brne (including the branch misprediction penalty);
 * - 2 cycles spent on the last loop run;
 *   - 1 cycle on subi;
 *   - 1 cycle on brne (no branch misprediction penalty).
 * The total number of cycles spent is (usually) 3 * n.
 * 
 * Similarly, _delay_loop_2(n) is usually compiled using movw, sbiw, brne, and
 * will spend CPU cycles like this:
 * - 1 cycle for initializing the looping variable (movw);
 * - 4 * (n - 1) cycles spent on the first n - 1 loop runs;
 *   - 2 * (n - 1) cycles on sbiw;
 *   - 2 * (n - 1) cycles on brne (including the branch misprediction penalty);
 * - 3 cycles spent on the last loop run;
 *   - 2 cycles on sbiw;
 *   - 1 cycle on brne (no branch misprediction penalty).
 * The total number of cycles spent is (usually) 4 * n.
 * 
 * Whenever a particular function is chosen, this template will invoke it
 * using T / X as an argument (e.g. _delay_loop_1(T / 3)). Since T is usually
 * not a multiple of X, the remaining delay cycles are generated by invoking
 * delayInCyclesWithNOP<T % X>() (which only uses nop instructions).
 * 
 * The choice of using a particular function only depends on the value of T, and
 * this can be easily seen within the body of the template.
 * 
 * It should be noted how many times the word "usually" has been used. In fact,
 * generated functions work as intended when the following requirements are met:
 * - the compiler always inlines the functions;
 * - for n strictly positive, running _delay_loop_1(n) takes exactly 3 * n
 *   cycles;
 * - for n strictly positive, running _delay_loop_2(n) takes exactly 4 * n
 *   cycles.
 * So far, when enabling optimization for space (-Os) and speed (-O3), all these
 * requirements have been met.
 * 
 * Usually there's no need to generate the same exact delay in multiple pieces
 * of code, which means there will hardly be a generated function "shared"
 * between multiple sites. This helps with the inlining requirement.
 * 
 * _NOP() macros cannot be optimized out, as their only assembly instruction
 * has been annotated as volatile.
 * 
 * Some additional notes about particular values of T.
 * 
 * 0 would produce an empty function, which simply produces no delay. I felt
 * this might be useful in contexts where the parameter value is the result of
 * an expression which involves other constants. If one of these constants
 * (which could represent some external delay) is changed every now and then,
 * this could potentially cause T to be set to 0, without the developer even
 * realizing it. A delay of 0 cycles is perfectly acceptable.
 * 
 * Of course, a negative delay doesn't make sense, which is why T was declared
 * as unsigned.
 * 
 * As soon as T reaches 3, generated functions start using _delay_loop_1(T / 3)
 * in combination of delayInCyclesWithNOP<T % 3>(). The reason _delay_loop_1(n)
 * is not used for lower values of T is because _delay_loop_1(0) doesn't run in
 * 0 cycles, but usually 3 * 256 cycles (not always, read below for more). In
 * fact, since the assembly instructions work like a do-while loop, at least one
 * iteration is executed, which causes an underflow and brings the variable from
 * 0 to 255.
 * 
 * As soon as T reaches 3 * 256, generated functions ditch _delay_loop_1(n) in
 * favor of _delay_loop_2(n); the reason for this is interesting. If we did use
 * _delay_loop_1(n) with the value 3 * 256, the argument n would be 0 (the
 * result of casting 256 to an unsigned byte). Unfortunately, based on some
 * experiments, the compiler's output is less consistent in this scenario.
 * Sometimes it still generates the instructions mov, subi and brne, and it
 * takes exactly 3 * 256 cycles (because of the underflow explained above).
 * Other times, it omits the first mov instruction and recycles a register which
 * has just been zeroed, making the delay one CPU cycle shorter.
 */
template<uint16_t T>
inline void delayInCyclesWithLoop() {
  if (T < 3) {
    delayInCyclesWithNOP<T>();
  } else if (T < 3 * 256) {
    _delay_loop_1(T / 3);
    delayInCyclesWithNOP<T % 3>();
  } else {
    _delay_loop_2(T / 4);
    delayInCyclesWithNOP<T % 4>();
  }
}
