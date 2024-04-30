// ===================================================================================
// Project:   MacroPad Mini for CH551, CH552 and CH554
// Version:   v1.1
// Year:      2023
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// Firmware example implementation for the MacroPad Mini.
//
// References:
// -----------
// - Blinkinlabs: https://github.com/Blinkinlabs/ch554_sdcc
// - Deqing Sun: https://github.com/DeqingSun/ch55xduino
// - Ralph Doncaster: https://github.com/nerdralph/ch554_sdcc
// - WCH Nanjing Qinheng Microelectronics: http://wch.cn
//
// Compilation Instructions:
// -------------------------
// - Chip:  CH551, CH552 or CH554
// - Clock: min. 12 MHz internal
// - Adjust the firmware parameters in include/config.h if necessary.
// - Make sure SDCC toolchain and Python3 with PyUSB is installed.
// - Press BOOT button on the board and keep it pressed while connecting it via
// USB
//   with your PC.
// - Run 'make flash'.
//
// Operating Instructions:
// -----------------------
// - Connect the board via USB to your PC. It should be detected as a HID
// keyboard.
// - Press a macro key and see what happens.
// - To enter bootloader hold down key 1 while connecting the MacroPad to USB.
// All
//   NeoPixels will light up white as long as the device is in bootloader mode
//   (about 10 seconds).

// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================

// Libraries
#include <config.h>     // user configurations
#include <delay.h>      // delay functions
#include <neo.h>        // NeoPixel functions
#include <system.h>     // system functions
#include <usb_conkbd.h> // USB HID consumer keyboard functions
#include <usb_descr.h>  // system functions

// Prototypes for used interrupts
void USB_interrupt(void);
void USB_ISR(void) __interrupt(INT_NO_USB) { USB_interrupt(); }


enum KeyType {
  KEYBOARD = 0,
  CONSUMER = 1,
};

#define KEY_EEPROM_FIELDS 3
#define RGB_EEPROM_FIELDS 3

#define RGB_EEPROM_OFFSET 6 * KEY_EEPROM_FIELDS

// structur with key details
struct key {
  uint8_t mod;
  enum KeyType type;
  char code;
  uint8_t last;
};

struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// ===================================================================================
// NeoPixel Functions
// ===================================================================================

// Update NeoPixels
void NEO_update(struct RGBColor *neo, float *percent) {

  // if (!KBD_CAPS_LOCK_state) {
  //   EA = 0;                  // disable interrupts
  //   NEO_writeColor(0, 0, 0); // NeoPixel 1 OFF
  //   NEO_writeColor(0, 0, 0); // NeoPixel 2 OFF
  //   NEO_writeColor(0, 0, 0); // NeoPixel 3 OFF
  //   EA = 1;                  // enable interrupts
  // } else {
  // percent glowing is not working :(
  EA = 0;                                       // disable interrupts
  NEO_writeColor(neo[0].r, neo[0].g, neo[0].b); // NeoPixel 1 lights up red
  NEO_writeColor(neo[1].r, neo[1].g, neo[1].b); // NeoPixel 2 lights up green
  NEO_writeColor(neo[2].r, neo[2].g, neo[2].b); // NeoPixel 3 lights up blue
  EA = 1;                                       // enable interrupts
  // }
}

// Read EEPROM (stolen from
// https://github.com/DeqingSun/ch55xduino/blob/ch55xduino/ch55xduino/ch55x/cores/ch55xduino/eeprom.c)
uint8_t eeprom_read_byte(uint8_t addr) {
  ROM_ADDR_H = DATA_FLASH_ADDR >> 8;
  ROM_ADDR_L = addr << 1; // Addr must be even
  ROM_CTRL = ROM_CMD_READ;
  return ROM_DATA_L;
}

void eeprom_write_byte(__data uint8_t addr, __xdata uint8_t val) {
  if (addr >= 128) {
    return;
  }
  SAFE_MOD = 0x55;
  SAFE_MOD = 0xAA;        // Enter Safe mode
  GLOBAL_CFG |= bDATA_WE; // Enable DataFlash write
  SAFE_MOD = 0;           // Exit Safe mode
  ROM_ADDR_H = DATA_FLASH_ADDR >> 8;
  ROM_ADDR_L = addr << 1;
  ROM_DATA_L = val;
  if (ROM_STATUS & bROM_ADDR_OK) { // Valid access Address
    ROM_CTRL = ROM_CMD_WRITE;      // Write
  }
  SAFE_MOD = 0x55;
  SAFE_MOD = 0xAA;         // Enter Safe mode
  GLOBAL_CFG &= ~bDATA_WE; // Disable DataFlash write
  SAFE_MOD = 0;            // Exit Safe mode
}

// handle key press
void handle_key(uint8_t current, struct key *key, float *neo) {
  if (current != key->last) { // state changed?
    key->last = current;      // update last state flag
    if (current) {            // key was pressed?
      if (key->type == KEYBOARD) {
        KBD_code_press(key->mod, key->code); // press keyboard/keypad key
      } else {
        CON_press(key->code); // press consumer key
      }
      if (neo)
        *neo = NEO_MAX;
    } else { // key was released?
      if (key->type == KEYBOARD) {
        KBD_code_release(key->mod,
                         key->code); // release
      } else {
        CON_release(key->code); // release
      }
    }
  } else if (key->last) { // key still being pressed?
                          // if(neo) *neo = NEO_MAX;                 // keep
                          // NeoPixel on
  }
}

// ===================================================================================
// Main Function
// ===================================================================================
void main(void) {
  // Variables
  struct key keys[6];         // array of struct for keys
  struct key *currentKnobKey; // current key to be sent by knob
  __idata uint8_t i;          // temp variable
  struct RGBColor buttonColors[3];
  float percent[3] = {0.0, 0.0, 0.0};

  // Enter bootloader if key 1 is pressed
  NEO_init();                // init NeoPixels
  if (!PIN_read(PIN_KEY1)) { // key 1 pressed?
    NEO_latch();             // make sure pixels are ready
    for (i = 9; i; i--)
      NEO_sendByte(255 * NEO_MAX); // light up all pixels
    BOOT_now();                    // enter bootloader
  }

  // Setup
  CLK_config(); // configure system clock
  DLY_ms(5);    // wait for clock to settle
  KBD_init();   // init USB HID keyboard
  WDT_start();  // start watchdog timer

  // TODO: Read eeprom for key characters
  for (i = 0; i < 6; i++) {
    keys[i].mod = (char)eeprom_read_byte(i * KEY_EEPROM_FIELDS);
    keys[i].type = eeprom_read_byte(i * KEY_EEPROM_FIELDS + 1);
    keys[i].code = (char)eeprom_read_byte(i * KEY_EEPROM_FIELDS + 2);
    keys[i].last = 0;
  }

  for (i = 0; i < 3; i++) {
    buttonColors[i].r =
        (char)eeprom_read_byte(i * RGB_EEPROM_FIELDS + (RGB_EEPROM_OFFSET));
    buttonColors[i].g =
        (char)eeprom_read_byte(i * RGB_EEPROM_FIELDS + 1 + (RGB_EEPROM_OFFSET));
    buttonColors[i].b =
        (char)eeprom_read_byte(i * RGB_EEPROM_FIELDS + 2 + (RGB_EEPROM_OFFSET));
  }

  // Loop
  while (1) {
    handle_key(!PIN_read(PIN_KEY1), &keys[0], &percent[0]);
    handle_key(!PIN_read(PIN_KEY2), &keys[1], &percent[1]);
    handle_key(!PIN_read(PIN_KEY3), &keys[2], &percent[2]);
    handle_key(!PIN_read(PIN_ENC_SW), &keys[3], (void *)0);

    // Handle knob
    currentKnobKey = 0;         // clear key variable
    if (!PIN_read(PIN_ENC_A)) { // encoder turned ?
      if (PIN_read(PIN_ENC_B)) {
        currentKnobKey = &keys[4]; // clockwise?
      } else {
        currentKnobKey = &keys[5]; // counter-clockwise?
      }
      DLY_ms(10); // debounce
      while (!PIN_read(PIN_ENC_A))
        ; // wait until next detent
    }

    if (currentKnobKey) {
      if (currentKnobKey->type == KEYBOARD) {
        KBD_code_type(
            currentKnobKey->mod,
            currentKnobKey->code); // press and release corresponding key ...
      } else {
        CON_type(
            currentKnobKey->code); // press and release corresponding key ...
      }
    }

    // Update NeoPixels
    NEO_update(buttonColors, percent);
    for (i = 0; i < 3; i++) {
      if (percent[i] > NEO_GLOW)
        percent[i] = -0.01; // fade down NeoPixel
    }
    DLY_ms(5); // latch and debounce

    // https://github.com/DeqingSun/ch55xduino/blob/ch55xduino/ch55xduino/ch55x/libraries/Generic_Examples/examples/05.USB/qmkCompatibleKeyboard/src/userQmkCompatibleKeyboard/via.c
    //  See this project for how to handle HID packets and declare a new HID raw
    //  interface.
    if (HID_available()) { // received data packet?
      i = HID_available(); // get number of bytes in packet

      // while (i--)
      //   HID_read(); // read and discard all bytes

      HID_ack(); // acknowledge packet
    }

    if (!((HID_statusLed() >> 1) & 1)) { // LED Capslock = 1
      buttonColors[0].r = 255;
      buttonColors[0].b = 0;
      buttonColors[0].g = 0;
    } else {
      buttonColors[0].r = 0; // pass all bytes in packet to I2C
      buttonColors[0].b = 0;
      buttonColors[0].g = 0;
    }

    // if (HID_available()) {
    //   uint8_t c = HID_read(); // read incoming character
    //   // eeprom_write_byte(RGB_EEPROM_FIELDS + i, c);   // write to eeprom
    //   // if (c == 0x01) {
    //     buttonColors[0].r = 255;
    //     buttonColors[0].g = 0;
    //   //   buttonColors[i].g = HID_read();
    //   //   buttonColors[i].b = HID_read();
    //   //   i++;
    //   // }

    // } else {
    //     buttonColors[0].g = 255;
    //     buttonColors[0].r = 255;
    // }
    WDT_reset(); // reset watchdog
  }
}
