#pragma once

typedef enum
{
    APP_STATE_BOOT,

    APP_STATE_SCANNER_MODE,
    /// @brief Master mode is basically idle, the app is waiting for a scan
    APP_STATE_MASTER_MODE,
    /// @brief A card has been scanned and the app is waiting for the user to press the trigger button.
    /// Will leave back to master if not pressed within a certain time
    APP_STATE_SCANNED_CARD,
    APP_STATE_TRIGGER_BUTTON_PRESSED,
    APP_STATE_APPLY_KEYSTROKES,

    APP_STATE_SEND_PASSWORD_DB,
    APP_STATE_SEND_RFID,
    APP_STATE_SAVE_NEW_CARD,
    APP_STATE_CLEAR_CARD,
    APP_STATE_CLEAR_DB,

    APP_STATE_MAX
} APP_STATE;

extern APP_STATE state;