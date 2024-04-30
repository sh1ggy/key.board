#pragma once

static const char *TAG = "keydot";

#define MAX_PASS_SIZE 50
#define MAX_DESC_SIZE 50
#define NVS_STORAGE_NAMESPACE "kb"
#define TRIGGER_BUTTON_PIN GPIO_NUM_12
#define TRIGGER_BUTTON_BIT_MASK (1ULL << TRIGGER_BUTTON_PIN)
#define STATE_PRINT_INTERVAL 5 * 1000 * 1000

#define LOG_DETAILS
#define LOG_HEAP
#define LOG_STATE