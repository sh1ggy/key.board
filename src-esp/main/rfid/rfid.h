#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// all of your legacy C code here
void setup_rfid_reader();

//Declared in main
extern int currently_scanned_tag_index;
extern uint64_t currently_scanned_tag;

#ifdef __cplusplus
}
#endif