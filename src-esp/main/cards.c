#include "cards.h"

#include "esp_log.h"

void init_rfid_tags()
{
    ESP_LOGI(TAG, "Initialising Tag DB");
    // Get the total number of tags from nvs
    esp_err_t err = nvs_get_u32(storage_handle, "num_cards", &rfid_db.total_rfid_tags);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No tags found in NVS");
        return;
    }
    // malloc the buffer to match the number of tags
    rfid_db.serial_number_buffer = (uint64_t *)malloc(rfid_db.total_rfid_tags * sizeof(uint64_t));

    // Get the serial numbers from nvs
    for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
    {
        char tag_key[16];
        sprintf(tag_key, "tag%zu", i);
        err = nvs_get_u64(storage_handle, tag_key, &rfid_db.serial_number_buffer[i]);
        ESP_ERROR_CHECK(err);

        ESP_LOGI(TAG, "Tag %s: (sn: %" PRIu64 "): ", tag_key, rfid_db.serial_number_buffer[i]);
    }
}

// TODO: Add error handling
void create_get_db_response(cJSON *root, char **json_str)
{
    cJSON *tags = cJSON_CreateArray();
    for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
    {
        cJSON *tag = cJSON_CreateObject();
        cJSON_AddNumberToObject(tag, "rfid", rfid_db.serial_number_buffer[i]);

        // Get the description from nvs, its ok if this takes long, we don't need to cache descs in mem
        char desc_key[16];
        sprintf(desc_key, "name%zu", i);
        printf("Desckey: %s\n", desc_key);

        char out_desc[MAX_DESC_SIZE];
        size_t str_len = MAX_DESC_SIZE;
        esp_err_t err = nvs_get_str(storage_handle, desc_key, out_desc, &str_len);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Desc for key '%s' not found in NVS\n", desc_key);
            continue;
        }

        ESP_ERROR_CHECK_WITHOUT_ABORT(err);

        cJSON_AddStringToObject(tag, "name", out_desc);
        cJSON_AddItemToArray(tags, tag);
    }
    cJSON_AddItemToObject(root, "descriptions", tags);
    // *json_str = cJSON_Print(root);
    *json_str = cJSON_PrintUnformatted(root);
}

void save_card(NEW_CARD_t *new_card, uint64_t currently_scanned_tag, int index)
{
    ESP_LOGI(TAG, "Saving card at index %d", index);

    char tag_key[16];
    sprintf(tag_key, "tag%zu", index);
    esp_err_t err = nvs_set_u64(storage_handle, tag_key, currently_scanned_tag);

    sprintf(tag_key, "pass%zu", index);
    err = nvs_set_str(storage_handle, tag_key, new_card->pass);

    sprintf(tag_key, "name%zu", index);
    err = nvs_set_str(storage_handle, tag_key, new_card->desc);

    ESP_ERROR_CHECK(err);
}

void save_new_card(NEW_CARD_t *new_card, uint64_t currently_scanned_tag)
{
    ESP_LOGI(TAG, "Saving new card with desc: %s", new_card->desc);

    save_card(new_card, currently_scanned_tag, rfid_db.total_rfid_tags);

    rfid_db.total_rfid_tags++;
    esp_err_t err = nvs_set_u32(storage_handle, "num_cards", rfid_db.total_rfid_tags);

    ESP_ERROR_CHECK(err);
}

void clear_card(int index)
{
    ESP_LOGI(TAG, "Clearing card at index %d", index);
    esp_err_t err;

    for (size_t i = index; i < rfid_db.total_rfid_tags; i++)
    {
        // Get the next tag and move it to the current index
        int next_tag = i + 1;
        if (next_tag >= rfid_db.total_rfid_tags)
        {
            break;
        }
        ESP_LOGI(TAG, "Moving tag %d to %d", next_tag, i);
        NEW_CARD_t card;
        uint64_t next_tag_sn;

        size_t str_len = MAX_DESC_SIZE;
        char tag_key[16];
        sprintf(tag_key, "tag%zu", next_tag);
        err = nvs_get_u64(storage_handle, tag_key, &next_tag_sn);
        sprintf(tag_key, "name%zu", next_tag);
        err = nvs_get_str(storage_handle, tag_key, card.pass, &str_len);
        sprintf(tag_key, "pass%zu", next_tag);
        err = nvs_get_str(storage_handle, tag_key, card.desc, &str_len);

        ESP_ERROR_CHECK(err);

        save_card(&card, next_tag_sn, i);
    }

    // Clear the last tag
    char tag_key[16];
    sprintf(tag_key, "tag%zu", (size_t)rfid_db.total_rfid_tags);
    err = nvs_erase_key(storage_handle, tag_key);
    sprintf(tag_key, "name%zu", (size_t)rfid_db.total_rfid_tags);
    err = nvs_erase_key(storage_handle, tag_key);
    sprintf(tag_key, "pass%zu", (size_t)rfid_db.total_rfid_tags);
    err = nvs_erase_key(storage_handle, tag_key);

    rfid_db.total_rfid_tags--;
    err = nvs_set_u32(storage_handle, "num_cards", rfid_db.total_rfid_tags);

    ESP_ERROR_CHECK(err);
}