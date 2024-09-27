#pragma once

#include "constants.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "cJSON.h"

#include <inttypes.h>

typedef struct
{
    uint64_t *serial_number_buffer;
    uint32_t total_rfid_tags;
} RFID_DB_t;

typedef struct
{
    char pass[MAX_PASS_SIZE];
    char desc[MAX_DESC_SIZE];
} NEW_CARD_t;

/****** Defined in main *******/
extern RFID_DB_t rfid_db;
extern nvs_handle_t storage_handle;

void init_rfid_tags();

/// @brief Creates a json response for the get_db command
/// @param root cjson root, caller is responsible for mem
/// @param json_str json string, caller is responsible for mem clean
void create_get_db_response(cJSON *root, char **json_str);
// To be honest I think the above API isnt that great, the caller should just call the print function
// This function is best reserved for just constructing the object

void save_new_card(NEW_CARD_t *new_card, uint64_t *currently_scanned_tag);

void clear_card(int index);

/// @brief Clears the entire nvs
void clear_db();