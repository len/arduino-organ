/*
 * Tone Generator module for organ.
 * 
 * TODO:
 *    - use Hammond tuning http://www.goodeveca.net/RotorOrgan/ToneWheelSpec.html
 *    - maybe add 8th octave (wheels 85 to 91)
 *    - some ppl say lowest octave in hammond uses an almost square wave, other are sines
 *    - ideal vibrato 6hz, depth 1.5%
 *    - original hammond vibrato is almost triangular, 6.87 hz, with scans of: V1 45%, V3 66% V3 100%
 *    - can we put 2 drawbars per analog input?
 *    - drawbar foldback
 *    - add tonewheel leakage
 *    - atenuate high frequencies, not all drawbars apply the same gain, lower notes should be louder
 *    - add some tremolo to vibrato effect, 90 degrees out of phase with the vibrato
 *    - add digital noise keyclick (temporarily until we have real keyclick)
 *    - maybe remove tunings and use the pin for percussion (short 200 ms decay, long 1 second decay)
 *    - the 5 drawbar analog inputs can be used for 5 or 10 extra digital inputs reducing drawbar resolution to 1/2 or 1/4
 *    - experiment with a control to change the phase between the 7 oscillators
 *    - maybe rocker buttons to assing other functions to the tuning pots
 *    - on the back pedal inputs for different functions too (volume, tuning, etc)
 *    
 *    more technical information: 
 *    https://electricdruid.net/technical-aspects-of-the-hammond-organ/
 *    https://teichman.org/blog/2011/05/roto.html
 *    https://www.soundonsound.com/techniques/synthesizing-rest-hammond-organ-part-2
 *    https://www.soundonsound.com/techniques/synthesizing-tonewheel-organs
 *    https://www.soundonsound.com/techniques/synthesizing-hammond-organ-effects-part-1
 *    
 */

#include <MozziGuts.h>
#include <mozzi_midi.h>
#include <mozzi_fixmath.h> // for Q16n16 fixed-point fractional number type
#include <mozzi_rand.h>

#define CONTROL_RATE 64 // powers of 2 please

#define DRAWBAR1_PIN A0 // 16' sub-fundamental, f/2
#define DRAWBAR2_PIN A1 // 8' fundamental, f
#define DRAWBAR3_PIN A2 // 4' 2nd harmonic, f*2
#define DRAWBAR4_PIN A3 // (II) 5 1/3' + 1 3/5', sub-3rd harmonic + 6th harmonic, f*3/2 + f*5
#define DRAWBAR5_PIN A4 // (III) 2 2/3' + 2' + 1', 3rd harminic + 4th harmonic + 8th harmonic, f*3 + f*4 + f*8

#define TUNING_PIN A5
#define WAVE_PIN A6
#define VIBRATO_PIN A7

#define KEY_A1_PIN 0 // PD0
#define KEY_A2_PIN 1 // PD1
#define KEY_A3_PIN 2 // PD2
#define KEY_A4_PIN 3 // PD3
#define KEY_D1_PIN 4 // PD4
#define KEY_D2_PIN 5 // PD5
#define KEY_D3_PIN 6 // PD6
#define KEY_D4_PIN 7 // PD7
#define KEY_F1_PIN 8 // PB0
#define KEY_F2_PIN 11 // PB3
#define KEY_F3_PIN 12 // PB4
#define KEY_F4_PIN 13 // PB5

//#define OSCIL_DITHER_PHASE 1
#include "ToneGenerator.h"

#define WAVE_SIZE 4096
#include "sine.h"
#include "vox_reed.h"
//#include "farfisa.h"
#include "farfisa2.h"
//#include "farfisa3.h"
#include "bass.h"

ToneGenerator<WAVE_SIZE, AUDIO_RATE> generator;

byte vibrato_depth = 0;
byte vibrato_speed = 0;
unsigned int vibrato_phase = 0;

unsigned int counter = 0;

int note = 0; // the note of this tone generator

#define CROSSTALK_BITS 6

#include <RollingAverage.h>

RollingAverage<int, 32> tuning_average;


void setup() {
  startMozzi(CONTROL_RATE);
//  Serial.begin(9600);

//  analogReference(EXTERNAL); // use AREF for reference voltage

  pinMode(KEY_A1_PIN, INPUT_PULLUP);
  pinMode(KEY_A2_PIN, INPUT_PULLUP);
  pinMode(KEY_A3_PIN, INPUT_PULLUP);
  pinMode(KEY_A4_PIN, INPUT_PULLUP);
  pinMode(KEY_D1_PIN, INPUT_PULLUP);
  pinMode(KEY_D2_PIN, INPUT_PULLUP);
  pinMode(KEY_D3_PIN, INPUT_PULLUP);
  pinMode(KEY_D4_PIN, INPUT_PULLUP);
  pinMode(KEY_F1_PIN, INPUT_PULLUP);
  pinMode(KEY_F2_PIN, INPUT_PULLUP);
  pinMode(KEY_F3_PIN, INPUT_PULLUP);
  pinMode(KEY_F4_PIN, INPUT); // this is pin 13, uses an external pull-up resistor

  generator.setWaveTables(SINE_WAVE, BASS_WAVE, SINE_WAVE, BASS_WAVE);
  generator.setMasterNote(0);
}


void updateControl() {
  unsigned int param;
  byte a1, a2, a3, a4, f1, f2, f3, f4, d1, d2, d3, d4;
  unsigned int sub2, h, h2, h_ii, h_iii;
  unsigned int x0, x1, x2, x3, x4, x5, x6;
  unsigned int mix;
  
//  generator.setMasterNoteShifted(note, mozziAnalogRead(TUNING_PIN));
  generator.setMasterNoteShifted(note, tuning_average.next(mozziAnalogRead(TUNING_PIN)));

  // set vibrato
  param = mozziAnalogRead(VIBRATO_PIN);
  if (param < 512) {
    vibrato_speed = ((687<<4)/100)/(AUDIO_RATE/WAVE_SIZE); // 6.87 hz like Hammond B3
    vibrato_depth = (511 - param) >> 2; // depth 0 to 128, with the ideal vibrato in the middle (1.5% is the ideal vibrato, and since the wave cycle size is 4096 this is around 64)
  } else {
    vibrato_speed = ((400<<4)/100)/(AUDIO_RATE/WAVE_SIZE); // 4 hz (slow)
    vibrato_depth = (param - 512) >> 2;
  }
  
  // set wave shape
  param = mozziAnalogRead(WAVE_PIN);
  if (param < 512) {
    generator.setWaveTables(VOX_REED_WAVE, VOX_REED_WAVE, SINE_WAVE, BASS_WAVE);
  } else {
    generator.setWaveTables(SINE_WAVE, BASS_WAVE, FARFISA2_WAVE, FARFISA2_WAVE);
    param -= 512;
  }
  mix = (param<<6)/511; // value between 0 and 64
  
  // read drawbars
  sub2 = mozziAnalogRead(DRAWBAR1_PIN);
  h = mozziAnalogRead(DRAWBAR2_PIN);
  h2 = mozziAnalogRead(DRAWBAR3_PIN);
  h_ii = mozziAnalogRead(DRAWBAR4_PIN);
  h_iii = mozziAnalogRead(DRAWBAR5_PIN);
  sub2 = 1023;
  h = 1023;
  h2 = 1023;
  h_ii = 1023;
  h_iii = 1023;

  a1 = a2 = a3 = a4 = f1 = f2 = f3 = f4 = d1 = d2 = d3 = d4 = 0;
  
  // read keyboard keys
  a1 = !digitalRead(KEY_A1_PIN);
  a2 = !digitalRead(KEY_A2_PIN);
  a3 = !digitalRead(KEY_A3_PIN);
  a4 = !digitalRead(KEY_A4_PIN);
  d1 = !digitalRead(KEY_D1_PIN);
  d2 = !digitalRead(KEY_D2_PIN);
  d3 = !digitalRead(KEY_D3_PIN);
  d4 = !digitalRead(KEY_D4_PIN);
  f1 = !digitalRead(KEY_F1_PIN);
  f2 = !digitalRead(KEY_F2_PIN);
  f3 = !digitalRead(KEY_F3_PIN);
  f4 = !digitalRead(KEY_F4_PIN);
  
/*
//x7 =                                   + a4*h_iii                      + f4*h_ii;
  x6 =                          a4*h_iii + a3*h_iii                      + f3*h_ii;
  x5 =                  a4*h2 + a3*h_iii + a2*h_iii           + d4*h_iii + f2*h_ii;
  x4 =           a4*h + a3*h2 + a2*h_iii + a1*h_iii + d4*h_ii + d3*h_iii + f1*h_ii;
  x3 = a4*sub2 + a3*h + a2*h2 + a1*h_iii            + d3*h_ii + d2*h_iii;
  x2 = a3*sub2 + a2*h + a1*h2                       + d2*h_ii + d1*h_iii;
  x1 = a2*sub2 + a1*h                               + d1*h_ii;
  x0 = a1*sub2;
*/



  // some random notes for testing
/*  switch (counter >> 13 & 3) {
    case 0: note = (counter >> 12) & 0xf; a4 = 1; f1 = 1; break;
    case 1: a3 = 1; f2 = 1; break;
    case 2: a2 = 1; d3 = 1; break;
    case 3: a1 = 1; f4 = 1; break;
  }
*/
  // compute amplitudes from keys pressed and drawbars
  x0 = x1 = x2 = x3 = x4 = x5 = x6 = 0;
  if (a1) {
    x0 = sub2; x1 = h; x2 = h2; x3 = h_iii; x4 = h_iii;
  }
  if (a2) {
    x1 = max(x1,sub2); x2 = max(x2,h); x3 = max(x3,h2); x4 = max(x4,h_iii); x4 = max(x4,h_iii);
  }
  if (a3) {
    x2 = max(x2,sub2); x3 = max(x3,h); x4 = max(x4,h2); x5 = max(x5,h_iii); x6 = max(x6,h_iii);
  }
  if (a4) {
    x3 = max(x3,sub2); x4 = max(x4,h); x5 = max(x5,h2); x6 = max(x6,h_iii); // it's unnecesary to foldback x7 = max(x7,h_iii);
  }
  if (d1) {
    x1 = max(x1,h_ii); x2 = max(x2,h_iii);
  }
  if (d2) {
    x2 = max(x2,h_ii); x3 = max(x3,h_iii);
  }
  if (d3) {
    x3 = max(x3,h_ii); x4 = max(x4,h_iii);
  }
  if (d4) {
    x4 = max(x4,h_ii); x5 = max(x5,h_iii);
  }
  if (f1) {
    x4 = max(x4,h_ii);
  }
  if (f2) {
    x5 = max(x5,h_ii);
  }
  if (f3) {
    x6 = max(x6,h_ii);
  }
  if (f4) {
    x6 = max(x6,h_ii); // foldback of x7 = max(x7,h_ii);
  }

  // add crosstalk x0+x3+x6, x1+x4, x2+x5
  x0 = min(1023,x0+((x3+x6)>>(10-CROSSTALK_BITS)));
  x3 = min(1023,x3+((x0+x6)>>(10-CROSSTALK_BITS)));
  x6 = min(1023,x6+((x0+x3)>>(10-CROSSTALK_BITS)));
  x1 = min(1023,x1+(x4>>(10-CROSSTALK_BITS)));
  x4 = min(1023,x4+(x1>>(10-CROSSTALK_BITS)));
  x2 = min(1023,x2+(x5>>(10-CROSSTALK_BITS)));
  x5 = min(1023,x5+(x2>>(10-CROSSTALK_BITS)));
  
  generator.setGains(x0, x1, x2, x3, x4, x5, x6, mix);
//  generator.setGains(0, 0, 0, 1023, 0, 0, 0, mix);
}


int updateAudio() {
  Q15n16 vibrato = ((int8_t)pgm_read_byte_near(SINE_WAVE + (vibrato_phase>>4)))*vibrato_depth; // bitshift by 4 gives 12 bits, i.e. 0 to 4095 whch is the table size
  vibrato_phase += vibrato_speed;
  counter++;
  return generator.phMod(vibrato);
}


void loop() {
  audioHook(); // required here
}

