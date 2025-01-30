// ===================================================================================
// NeoPixel (Addressable LED) Functions for CH551, CH552 and CH554            * v1.0 *
// ===================================================================================
//
// Basic control functions for 800kHz addressable LEDs (NeoPixel). A simplified 
// protocol is used which should work with most LEDs.
//
// The following must be defined in config.h:
// PIN_NEO - pin connected to DATA-IN of the pixel strip (via a ~330 ohms resistor).
// NEO_GRB - type of pixel: NEO_GRB or NEO_RGB
// System clock frequency must be at least 6 MHz.
//
// Further information:     https://github.com/wagiminator/ATtiny13-NeoController
// 2023 by Stefan Wagner:   https://github.com/wagiminator

// ===================================================================================
// Libraries, Variables and Constants
// ===================================================================================
#include "neo.h"


// Gamma correction is neede to display correct colors.
// https://learn.adafruit.com/led-tricks-gamma-correction/the-quick-fix
__code uint8_t gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

#define NEOPIN PIN_asm(PIN_NEO)     // convert PIN_NEO for inline assembly

// ===================================================================================
// Protocol Delays
// ===================================================================================
// There are three essential conditions:
// - T0H (HIGH-time for "0"-bit) must be max.  500ns
// - T1H (HIGH-time for "1"-bit) must be min.  625ns
// - TCT (total clock time) must be      min. 1150ns
// The bit transmission loop takes 11 clock cycles.
#if FREQ_SYS == 24000000    // 24 MHz system clock
  #define T1H_DELAY \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop                     // 15 - 4 = 11 clock cycles for min 625ns
  #define TCT_DELAY \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop                     // 28 - 11 - 11 = 6 clock cycles for min 1150ns
#elif FREQ_SYS == 16000000  // 16 MHz system clock
  #define T1H_DELAY \
    nop             \
    nop             \
    nop             \
    nop             \
    nop             \
    nop                     // 10 - 4 = 6 clock cycles for min 625ns
  #define TCT_DELAY \
    nop             \
    nop                     // 19 - 6 - 11 = 2 clock cycles for min 1150ns
#elif FREQ_SYS == 12000000  // 12 MHz system clock
  #define T1H_DELAY \
    nop             \
    nop             \
    nop             \
    nop                     // 8 - 4 = 4 clock cycles for min 625ns
  #define TCT_DELAY         // 14 - 4 - 11 < 0 clock cycles for min 1150ns
#elif FREQ_SYS == 6000000   // 13 MHz system clock
  #define T1H_DELAY         // 4 - 4 = 0 clock cycles for min 625ns
  #define TCT_DELAY         // 7 - 0 - 11 < 0 clock cycles for min 1150ns
#else
  #error Unsupported system clock frequency for NeoPixels!
#endif

// ===================================================================================
// Send a Data Byte to the Pixels String
// ===================================================================================
// This is the most time sensitive part. Outside of the function, it must be 
// ensured that interrupts are disabled and that the time between the 
// transmission of the individual bytes is less than the pixel's latch time.
void NEO_sendByte(uint8_t data) { 
  data;                 // stop unreferenced argument warning
  __asm
    .even
    mov  r7, #8         ; 2 CLK - 8 bits to transfer
    xch  a, dpl         ; 2 CLK - data byte -> accu
    01$:
    rlc  a              ; 1 CLK - data bit -> carry (MSB first)
    setb NEOPIN         ; 2 CLK - NEO pin HIGH
    mov  NEOPIN, c      ; 2 CLK - "0"-bit? -> NEO pin LOW now
    T1H_DELAY           ; x CLK - TH1 delay
    clr  NEOPIN         ; 2 CLK - "1"-bit? -> NEO pin LOW a little later
    TCT_DELAY           ; y CLK - TCT delay
    djnz r7, 01$        ; 2/4|5|6 CLK - repeat for all bits
  __endasm;
}

// ===================================================================================
// Write Color to a Single Pixel
// ===================================================================================
void NEO_writeColor(uint8_t r, uint8_t g, uint8_t b) {
  
  #if defined (NEO_GRB)
    NEO_sendByte(gamma8[g]); NEO_sendByte(gamma8[r]); NEO_sendByte(gamma8[b]);
  #elif defined (NEO_RGB)
    NEO_sendByte(gamma8[r]); NEO_sendByte(gamma8[g]); NEO_sendByte(gamma8[b]);
  #else
    #error Wrong or missing NeoPixel type definition!
  #endif
}

// ===================================================================================
// Write Hue Value (0..191) and Brightness (0..2) to a Single Pixel
// ===================================================================================
void NEO_writeHue(uint8_t hue, uint8_t bright) {
  uint8_t phase = hue >> 6;
  uint8_t step  = (hue & 63) << bright;
  uint8_t nstep = (63 << bright) - step;
  switch(phase) {
    case 0:   NEO_writeColor(nstep,  step,     0); break;
    case 1:   NEO_writeColor(    0, nstep,  step); break;
    case 2:   NEO_writeColor( step,     0, nstep); break;
    default:  break;
  }
}
