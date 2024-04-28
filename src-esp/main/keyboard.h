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

void initialise_keyboard();

Keyboard_payload_t ascii_2_keyboard_payload(char chr);