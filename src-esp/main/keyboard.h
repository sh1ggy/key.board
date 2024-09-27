#pragma once
#define TEST_VALUE 3

#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include <inttypes.h>


typedef struct
{
    uint8_t keycode[6];
    uint8_t modifier;
} Keyboard_payload_t;

typedef enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
} USB_INTERFACES;


void initialise_keyboard();

Keyboard_payload_t ascii_2_keyboard_payload(char chr);

void wait_for_hid_ready();