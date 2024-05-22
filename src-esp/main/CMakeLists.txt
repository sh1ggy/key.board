set(USE_PN532 1)
# set(USE_MRFC522 1)

set(RFID_SRC "rfid/mock_rfid.cpp" )

if(USE_PN532)
    message("Using PN532")
    set(RFID_SRC "rfid/pn532.c")
elseif(USE_MRFC522)
    message("Using MRFC522")
    set(RFID_SRC "rfid/mfrc522.c")
endif()

message("Using rfid impl: " ${RFID_SRC})

idf_component_register(
    SRCS "comms.c" "keyboard.c" "cards.c" "util/profile.c" ${RFID_SRC} "tusb_hid_example_main.c"
    INCLUDE_DIRS "."
    # This is very important to build with nvs support
    # as shown here you dont need to add this to `components` in root cmake if this is here
    # https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s2/api-guides/build-system.html#optional-project-variables
    PRIV_REQUIRES driver nvs_flash esp-idf-rc522 json esp_timer esp32-pn532
    )

    # Create a NVS image from the contents of the `nvs_data` CSV file
# that fits the partition named 'nvs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
# nvs_create_partition_image(nvs ../data.csv FLASH_IN_PROJECT)


# https://cmake.org/cmake/help/v2.8.11/cmake.html#variable:CMAKE_HOST_SYSTEM
# https://stackoverflow.com/questions/68139352/how-to-print-variables-in-cmake
# (Not so useful) https://stackoverflow.com/questions/9742003/platform-detection-in-cmake
include(CMakePrintHelpers)
cmake_print_variables(CMAKE_HOST_SYSTEM)
cmake_print_variables(CMAKE_HOST_UNIX)
cmake_print_variables(CMAKE_HOST_WIN32)


if(CMAKE_HOST_WIN32)
    message("Can't nvs_create_part cuz we on windows")
else()
    message("Dumping sample nvs")
    nvs_create_partition_image(nvs ../data.csv FLASH_IN_PROJECT)
endif()

message("Done running custom Cmake config")