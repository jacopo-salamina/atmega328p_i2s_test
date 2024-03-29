#pragma once

#include "i2s_driver.hpp"
#include "type_traits_clone.hpp"


/**
 * Square wave generator with a duty cycle of 50%, which tries to be as precise
 * as possible while still using integer math.
 * 
 * This is a basic explanation of the way it works. Internally, the generator
 * actually simulates a sawtooth signal, which increments by a fixed amount
 * every time a new sample is needed, and is wrapped inside a predefined range.
 * Using a sawtooth signal makes it easy to generate a square wave.
 * 
 * An ideal algorithm to build a sawtooth generator could work like this:
 * - we assume to be capable of working with infinite precision floating point
 *   numbers (for the sake of simplicity and clarity);
 * - also, we assume the period between two audio samples (T_SAMPLE) is fixed
 *   (this is a real-world constraint we have to deal with);
 * - instead of storing directly the square wave's desired frequency (F_WAVE),
 *   we store the wave's period (T_WAVE = 1 / F_WAVE);
 * - an accumulator (T_ELAPSED) is first initialized with 0, and gets
 *   incremented by T_SAMPLE every time a new sample is requested; this way, we
 *   keep track of how many real-world seconds have passed;
 * - after T_ELAPSED is incremented, we check whether it's greater than, or
 *   equal to, T_WAVE (given that T_WAVE is usually not a perfect multiple of
 *   T_SAMPLE, we should expect T_ELAPSED to overshoot);
 * - if T_ELAPSED is greater than, or equal to, T_WAVE, we decrement the former
 *   by T_WAVE, in order to keep it within the range [0, T_WAVE) (we assume
 *   T_SAMPLE is much smaller than T_WAVE, so T_ELAPSED will never accidentally
 *   reach, or surpass, 2 * T_WAVE, and thus we don't need to perform the same
 *   check multiple times).
 * With this algorithm, the value of T_ELAPSED is a periodic snapshot of an
 * ideal sawtooth generator. Going from this to a square wave generator is easy:
 * we just need to compare T_ELAPSED against T_WAVE / 2, and if the former is
 * greater or equal we output a user-defined amplitude, otherwise we output its
 * opposite value. Of course, we could do this the other way around.
 * 
 * Despite being an "ideal" algorithm, it still doesn't produce a clean square
 * wave, and that's because of the sampling rate not being (usually) a multiple
 * of the desired frequency.
 * 
 * Another implementation would simply multiply the ideal square wave's period
 * by half the sampling rate and round the result (X), getting an approximation
 * of the square wave's period as a multiple of samples. Then, it would simply
 * loop X times and output 0, then loop X times and output the desired
 * amplitude, and so on. This has the advantage to produce a square wave with a
 * perfect 50% duty cycle, but at the cost of the frequency being slightly
 * different from what the end user expected. The "ideal" algorithm explained
 * above sacrifices the final signal quality in favor of an accurate frequency,
 * and the latter quality is desirable when using multiple instances of square
 * wave generators to produce chords.
 * 
 * Back to the algorithm, we now need to turn this into actual code. The biggest
 * challenge is turning the infinite range floating point values into limited
 * range (possibly integer) values. Every value was obtained by some sort of
 * fraction, so, ideally, there would be one constant by which we would multiply
 * every single floating point value and turn them into whole numbers. After
 * some mathematical considerations, it turns out such a constant does exist:
 * F_CPU * F_WAVE.
 * 
 * The entire algorithm can now be rewritten using integer math:
 * - instead of T_SAMPLE (which is FRAME_CYCLES / F_CPU), we're going to use
 *   FRAME_CYCLES / F_CPU * F_CPU * F_WAVE = FRAME_CYCLES * F_WAVE (and call it
 *   ticksIncrement);
 * - instead of T_WAVE (which is 1 / F_WAVE), we're going to store
 *   1 / F_WAVE * F_CPU * F_WAVE = F_CPU (and call it PERIOD_IN_TICKS);
 * - instead of T_WAVE / 2, we're going to use F_CPU / 2 (which will be a whole
 *   number, since F_CPU is usually a multiple of 1.000.000).
 * 
 * Note: the idea behind the word "ticks" is that we're creating a new unit for
 * measuring time. Rather than measuring it in multiples of a CPU clock cycle,
 * we're using a smaller unit, a "tick", such that a clock cycle contains
 * exactly F_WAVE ticks.
 */
template<
  uint16_t FRAME_PERIOD_IN_CYCLES,
  enable_if_t<FRAME_PERIOD_IN_CYCLES, int> = 0
>
class SquareWaveGenerator {
private:
  static const uint32_t PERIOD_IN_TICKS = F_CPU;
  uint32_t ticksIncrement;
  uint32_t elapsedTicks;
  int16_t amplitude;
  
public:
  /**
   * The number of CPU cycles required to run getNextSample()
   */
  static constexpr uint8_t GET_NEXT_SAMPLE_DURATION = 25;

  SquareWaveGenerator(uint32_t frequency, int16_t amplitude) :
    ticksIncrement(FRAME_PERIOD_IN_CYCLES * frequency),
    elapsedTicks(0),
    amplitude(amplitude)
  {}

  int16_t getFirstSample() {
    return 0;
  }

  /*
   * Determining the number of cycles needed by getNextSample() inside main() is
   * not trivial. There are multiple sections:
     * - 6 (1st common part)
     * - 7 (1st branch)
     * - 6 (2nd common part)
     * - 6 (2nd branch)
   * - a section which is always run (6 cycles);
   * - the section which makes sure elapsedTicks doesn't exceed PERIOD_IN_TICKS
   *   (7 cycles);
   *   - this part includes an if-else, and during the first tests each branch
   *     took a different amount of cycles, which made it impossible to reliably
   *     count cycles; however, thanks to a forced delay, both branches now take
   *     the same number of cycles;
   * - a second section for comparing elapsedTicks to PERIOD_IN_TICKS / 2, which
   *   is always run (6 cycles);
   * - a section for returning either amplitude or -amplitude (6 cycles);
   *   - like the first if-else, a forced delay made sure both branches took the
   *     same number of cycles;
   *   - the machine code includes the jumps required to go back to the start of
   *     the infinite loop inside main().
   * The entire method takes 25 cycles.
   * 
   * The section which makes sure elapsedTicks doesn't exceed PERIOD_IN_TICKS
   * deserves an in-depth discussion on some technical aspects.
   * 
   * During previous revisions, there was no tmp variable, and the if was shaped
   * slightly different (delays omitted for simplicity):
   *   if (elapsedTicks >= PERIOD_IN_TICKS) {
   *     elapsedTicks -= PERIOD_IN_TICKS;
   *   }
   * Evaluating this condition required the compiler to perform a series of cp
   * and cpc instructions, and these essentially simulate a subtraction (in our
   * case elapsedTicks - PERIOD_IN_TICKS); the branching instruction only needs
   * to check against carry. Then, the code inside the if actually performs the
   * same subtraction again, except this time it's not simulated. However,
   * during some tests, I realized the compiler didn't figure this out and
   * performed both the comparison and the subtraction, which was a considerable
   * waste of CPU cycles. The tmp variable helps solving this problem. It stores
   * the result of the actual subtraction, but cast as a signed integer; this
   * way, the compiler suddenly realizes the branching instruction only needs to
   * check against carry (or just test tmp's most significant bit, which is what
   * it ended up doing).
   */
  int16_t getNextSample() {
    elapsedTicks += ticksIncrement;
    int32_t tmp = elapsedTicks - PERIOD_IN_TICKS;
    if (tmp >= 0) {
      delayInCyclesWithNOP<4>();
      elapsedTicks = tmp;
    }
    if (elapsedTicks >= PERIOD_IN_TICKS / 2) {
      return amplitude;
    } else {
      delayInCyclesWithNOP<1>();
      return -amplitude;
    }
  }
};
