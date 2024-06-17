#pragma once

#include "esp_chip_info.h"

void print_memory_sizes(void);

void print_system_info_task(void *pvParameters);

extern esp_chip_info_t chip_info;