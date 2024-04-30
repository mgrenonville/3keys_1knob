// ===================================================================================
// USB Handler for CH551, CH552 and CH554
// ===================================================================================

#pragma once
#include "usb_descr.h"
#include <stdint.h>

// ===================================================================================
// Endpoint Buffer
// ===================================================================================
__xdata __at(EP0_ADDR)
uint8_t EP0_buffer[EP0_BUF_SIZE];
__xdata __at(EP1_ADDR)
uint8_t EP1_buffer[EP1_BUF_SIZE];
// Setting up the DMA buffer pointers requires special attention when multiple
// IN and OUT endpoints are used.  Even though five endpoints are supported,
// there are only four DMA buffer pointer registers: UEP[0-3]_DMA.  When the
// bits bUEP4_RX_EN and bUEP4_TX_EN are set in the UEP4_1_MOD SFR, the EP4 OUT
// buffer is UEP0_DMA + 64, and the EP4 IN buffer is UEP0_DMA + 128.  Endpoints
// 1-3 have even more complex buffer configurations, with optional
// double-buffering for IN and OUT using 256 bytes for four buffers starting
// from the UEPn_DMA pointer.
__xdata __at(EP1_ADDR + 64)
uint8_t EP1_OUT_buffer[EP1_BUF_SIZE];
__xdata __at(EP2_ADDR)
uint8_t EP2_buffer[EP2_BUF_SIZE];

#define USB_setupBuf ((PUSB_SETUP_REQ)EP0_buffer)
extern uint8_t SetupReq;

// ===================================================================================
// Custom External USB Handler Functions
// ===================================================================================
void HID_setup(void);
void HID_reset(void);
void HID_EP1_IN(void);
void HID_EP1_OUT(void);
void HID_EP2_OUT(void);

// ===================================================================================
// USB Handler Defines
// ===================================================================================
// Custom USB handler functions
#define USB_INIT_handler HID_setup  // init custom endpoints
#define USB_RESET_handler HID_reset // custom USB reset handler

// Endpoint callback functions
#define EP0_SETUP_callback USB_EP0_SETUP
#define EP0_IN_callback USB_EP0_IN
#define EP0_OUT_callback USB_EP0_OUT
#define EP1_IN_callback HID_EP1_IN
#define EP1_OUT_callback HID_EP1_OUT
#define EP2_OUT_callback HID_EP2_OUT

// ===================================================================================
// Functions
// ===================================================================================
void USB_interrupt(void);
void USB_init(void);
