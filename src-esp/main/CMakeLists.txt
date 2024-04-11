idf_component_register(
    SRCS "tusb_hid_example_main.c" "keyboard_ops.c"
    INCLUDE_DIRS "."
    # This is very important to build with nvs support
    # as shown here you dont need to add this to `components` in root cmake if this is here
    # https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s2/api-guides/build-system.html#optional-project-variables
    PRIV_REQUIRES driver nvs_flash esp-idf-rc522
    )

    # Create a NVS image from the contents of the `nvs_data` CSV file
# that fits the partition named 'nvs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
nvs_create_partition_image(nvs ../data.csv FLASH_IN_PROJECT)