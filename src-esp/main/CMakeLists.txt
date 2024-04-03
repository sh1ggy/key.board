idf_component_register(
    SRCS "tusb_hid_example_main.c" "keyboard_ops.c"
    INCLUDE_DIRS "."
    # This is very important to build with nvs support
    # as shown here you dont need to add this to `components` in root cmake if this is here
    # https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s2/api-guides/build-system.html#optional-project-variables
    PRIV_REQUIRES driver nvs_flash esp-idf-rc522
    )
