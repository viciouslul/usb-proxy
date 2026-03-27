# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD adafruit_feather_rp2040 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(usb-proxy C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(pico_pio_usb)

# Add executable. Default name is the project name, version 0.1

add_executable(usb-proxy
        src/usb-proxy.c
        src/usb_descriptors.c
        src/proxy_host.c
        src/proxy_device.c
        src/proxy_data.c
        src/proxy_detection.c
        src/proxy_ui.c
        src/proxy_metrics.c
        src/proxy_ssd1306.c
)
target_sources(usb-proxy PRIVATE
    src/usb-proxy.c
    src/usb_descriptors.c
    src/proxy_host.c
    src/proxy_device.c
    src/proxy_data.c
    src/proxy_detection.c
    src/proxy_ui.c
    src/proxy_metrics.c
    src/proxy_ssd1306.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

pico_set_program_name(usb-proxy "usb-proxy")
pico_set_program_version(usb-proxy "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(usb-proxy 0)
pico_enable_stdio_usb(usb-proxy 0)

set(PROXY_METRICS 1)
set(PROXY_FILTERING 1)

# Add the standard library to the build
target_link_libraries(usb-proxy
        pico_stdlib
        tinyusb_device
        tinyusb_host
        tinyusb_board
        pico_pio_usb
        pico_multicore
        hardware_i2c
)
# use tinyusb implementation
target_compile_definitions(usb-proxy PRIVATE
    USE_ADAFRUIT_FEATHER_RP2040_USBHOST
)

if(PROXY_METRICS)
    target_compile_definitions(usb-proxy PRIVATE
        PROXY_METRICS
    )
endif()

target_compile_definitions(usb-proxy PRIVATE
    PROXY_FILTERING=${PROXY_FILTERING}
)

# Add the standard include files to the build
target_include_directories(usb-proxy PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
)

pico_add_extra_outputs(usb-proxy)

