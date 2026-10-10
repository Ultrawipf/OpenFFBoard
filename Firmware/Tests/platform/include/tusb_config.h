/*
 * TinyUSB configuration for the host unit tests.
 *
 * The real TinyUSB headers from FFBoard/USB are used for all types and declarations
 * but the stack itself is not compiled. The device API functions used by the firmware
 * are replaced by recording fakes in platform/src/host_usb.cpp (see support/HostUsb.h).
 */
#ifndef HOSTTEST_TUSB_CONFIG_H_
#define HOSTTEST_TUSB_CONFIG_H_

#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 1
#define CFG_TUD_HID 1
#define CFG_TUD_VENDOR 0
#define CFG_TUD_AUDIO 0
#define CFG_TUD_DFU_RT 0

#define CFG_TUD_CDC_RX_BUFSIZE 512
#define CFG_TUD_CDC_TX_BUFSIZE 1024
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64
#define CFG_TUD_HID_EP_BUFSIZE 64

#endif
