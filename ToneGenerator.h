#ifndef TONEGENERATOR_H_
#define TONEGENERATOR_H_

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include "MozziGuts.h"
#include "mozzi_fixmath.h"
#include <util/atomic.h>

#include <mozzi_midi.h>

#include "mozzi_rand.h"

// fractional bits for oscillator index precision
#define OSCIL_F_BITS 16

template <uint16_t NUM_TABLE_CELLS, uint16_t UPDATE_RATE>
class ToneGenerator
{

public:

  ToneGenerator()
  {
    for (int i=0; i<7; i++) phase[i] = xorshift96();
  }

  inline
  int next()
  {
    incrementPhase();
    return peek(0);
  }

  inline
  int peek()
  {
    return peek(0);
  }
  
  inline
  int phMod(Q15n16 phmod_proportion)
  {
    incrementPhase();
    return peek(phmod_proportion * NUM_TABLE_CELLS);
  }

  inline void setWaveTables(const int8_t * w1, const int8_t * b1, const int8_t * w2, const int8_t * b2)
  {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      wave1 = w1;
      bass_wave1 = b1;
      wave2 = w2;
      bass_wave2 = b2;
    }
  }

  inline
  void setMasterNote(int note)
  {
    setMasterFrequency_Q16n16(Q16n16_mtof(Q16n0_to_Q16n16(96+note)));
  }

  inline
  void setMasterNoteShifted(int note, int shift)
  {
    Q16n16 frequency = Q16n16_mtof(Q16n0_to_Q16n16(96+note));
    frequency += frequency/1023*shift;
    setMasterFrequency_Q16n16(frequency);
  }

  inline
  void setMasterFrequency_Q16n16(Q16n16 frequency)
  {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      //phase_increment_fractional = ((frequency * (NUM_TABLE_CELLS>>7))/(UPDATE_RATE>>6)) << (F_BITS-16+1);
      // TB2014-8-20 change this following Austin Grossman's suggestion on user list
      // https://groups.google.com/forum/?utm_medium=email&utm_source=footer#!msg/mozzi-users/u4D5NMzVnQs/pCmiWInFvrkJ
      //phase_increment_fractional = (((((uint32_t)NUM_TABLE_CELLS<<ADJUST_FOR_NUM_TABLE_CELLS)>>7)*frequency)/(UPDATE_RATE>>6))
      //                             << (OSCIL_F_BITS - ADJUST_FOR_NUM_TABLE_CELLS - 16 + 1);                      
      if (NUM_TABLE_CELLS >= UPDATE_RATE) {
        phase_increment[0] = ((unsigned long)frequency) * (NUM_TABLE_CELLS/UPDATE_RATE);
      } else {
        phase_increment[0] = ((unsigned long)frequency) / (UPDATE_RATE/NUM_TABLE_CELLS);
      }
//      phase_increment[6] = phase_increment[0] >> 6; // make them all exact ratios
//      phase_increment[0] = phase_increment[6] << 6;

      phase_increment[1] = phase_increment[0] >> 1;
      phase_increment[2] = phase_increment[0] >> 2;
      phase_increment[3] = phase_increment[0] >> 3;
      phase_increment[4] = phase_increment[0] >> 4;
      phase_increment[5] = phase_increment[0] >> 5;
      phase_increment[6] = phase_increment[0] >> 6;
    }
  }

  /*
   * expects 0-1023 gain values, 0-64 mix
   */
  inline
  void setGains(unsigned int g0, unsigned int g1, unsigned int g2, unsigned int g3, unsigned int g4, unsigned int g5, unsigned int g6, unsigned int mix)
  {
    unsigned int scale1 = 36 * (64 - mix) >> 6, scale2 = 36 * mix >> 6;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      // we multiply by 36.6 ~ 64*4*1024/(1023*7), then we divide by 1024, the sum will not exceed 256
      gain1[0] = g6*scale1>>10;
      gain2[0] = g6*scale2>>10;
      gain1[1] = g5*scale1>>10;
      gain2[1] = g5*scale2>>10;
      gain1[2] = g4*scale1>>10;
      gain2[2] = g4*scale2>>10;
      gain1[3] = g3*scale1>>10;
      gain2[3] = g3*scale2>>10;
      gain1[4] = g2*scale1>>10;
      gain2[4] = g2*scale2>>10;
      gain1[5] = g1*scale1>>10;
      gain2[5] = g1*scale2>>10;
      gain1[6] = g0*scale1>>10;
      gain2[6] = g0*scale2>>10;
    }
  }


private:

  /** Used for shift arithmetic in setFreq() and its variations.
  */
static const uint8_t ADJUST_FOR_NUM_TABLE_CELLS = (NUM_TABLE_CELLS<2048) ? 8 : 0;

  inline
  void incrementPhase()
  {
    phase[0] += phase_increment[0];
    phase[1] += phase_increment[1];
    phase[2] += phase_increment[2];
    phase[3] += phase_increment[3];
    phase[4] += phase_increment[4];
    phase[5] += phase_increment[5];
    phase[6] += phase_increment[6];
  }

  inline
  int read_interpolate(unsigned long p, int8_t gain1, int8_t gain2)
  {
    unsigned long index = (p >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1);
    return ((int8_t)pgm_read_byte_near(wave1 + index)) * gain1 + ((int8_t)pgm_read_byte_near(wave2 + index)) * gain2;
//    return (int8_t)pgm_read_byte_near(wave1 + index);
  }

  inline
  int read_interpolate_bass(unsigned long p, int8_t gain1, int8_t gain2)
  {
    unsigned long index = (p >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1);
    return ((int8_t)pgm_read_byte_near(bass_wave1 + index)) * gain1 + ((int8_t)pgm_read_byte_near(bass_wave2 + index)) * gain2;
  }

  inline
  int read_wave1(unsigned long p, int8_t gain)
  {
    unsigned long index = (p >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1);
    return (int8_t)pgm_read_byte_near(wave1 + index) * gain;
  }

  inline
  int read_wave2(unsigned long p, int8_t gain)
  {
    unsigned long index = (p >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1);
    return (int8_t)pgm_read_byte_near(wave2 + index) * gain;
  }

  inline
  int peek(Q15n16 phmod)
  {
    return
     (read_interpolate(phase[0]+phmod, gain1[0], gain2[0]) +
      read_interpolate(phase[1]+phmod, gain1[1], gain2[1]) +
      read_interpolate(phase[2]+phmod, gain1[2], gain2[2]) +
      read_interpolate(phase[3]+phmod, gain1[3], gain2[3]) +
      read_interpolate(phase[4]+phmod, gain1[4], gain2[4]) +
      read_interpolate(phase[5]+phmod, gain1[5], gain2[5]) +
      read_interpolate_bass(phase[6]+phmod, gain1[6], gain2[6])) >> 2;
  }

  
  unsigned long phase[7];
  volatile unsigned long phase_increment[7];
  volatile int8_t gain1[7], gain2[7];
  volatile const int8_t *wave1, *wave2;
  volatile const int8_t *bass_wave1, *bass_wave2;
};


#endif /* TONEGENERATOR_H_ */

