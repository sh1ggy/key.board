#include "keyboard.h"

#include "tinyusb.h"
#include "constants.h"

#include "esp_log.h"

uint8_t const conv_table[128][2] = {HID_ASCII_TO_KEYCODE};

Keyboard_payload_t ascii_2_keyboard_payload(char chr)
{
    Keyboard_payload_t payload = {
        .keycode = {0},
        .modifier = 0};
    size_t ascii = (size_t)chr;

    if (conv_table[ascii][0])
        payload.modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
    payload.keycode[0] = conv_table[ascii][1];

    return payload;
}

/************* TinyUSB descriptors ****************/

// //------------- CLASS -------------// (Just confirming that these are activated)
// #define CFG_TUD_CDC              1
// #define CFG_TUD_HID               1

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE)),
    // https://github.com/chegewara/EspTinyUSB/blob/master/src/device/hid/hidgeneric.cpp
    //  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(HID_ITF_PROTOCOL_NONE)),
    //  https://github.com/hathach/tinyusb/blob/e54023d7657c9c090cdb068ce9352c1bba936f2e/examples/device/hid_generic_inout/src/usb_descriptors.c
    //  TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE)
};

/**
 * @brief String descriptor
 */
const char *hid_string_descriptor[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},          // 0: is supported language is English (0x0409)
    "Kongi Industries",            // 1: Manufacturer
    "Keydotboard",                 // 2: Product
    "123456",                      // 3: Serials, should use chip ID
    "KeyDOTboard dongle",          // 4: HID
    "KeyDOTboard debug interface", // 5: CDC
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82
#define EPNUM_HID 0x83

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    // The count here has to be correct, cdc counts for 2
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), EPNUM_HID, 16, 10),

    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(1, 5, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

};

/*** Keyboard Initialisation ****/
void initialise_keyboard()
{

    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        // .device_descriptor = &desc_device,
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");
}

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}
