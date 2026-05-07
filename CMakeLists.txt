cmake_minimum_required(VERSION 3.30)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(pico-sdk/pico_sdk_init.cmake)

project(pico-usb-ethernet)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    ${SRC}/main.c
    ${SRC}/usb_descriptors.c
  pico-sdk/lib/tinyusb/lib/networking/dhserver.c
  pico-sdk/lib/tinyusb/lib/networking/dnserver.c
pico-sdk/lib/lwip/src/apps/http/httpd.c
pico-sdk/lib/lwip/src/apps/http/fs.c
pico-sdk/lib/lwip/src/apps/lwiperf/lwiperf.c
pico-sdk/lib/tinyusb/hw/bsp/rp2040/family.c
)

# disable usb and uart because we're reusing it
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# add pico_stdlib library which aggregates commonly used features
target_include_directories(${PROJECT_NAME} PUBLIC
  ${SRC}
  pico-sdk/lib/tinyusb/hw
  pico-sdk/lib/tinyusb/lib/networking
  pico-sdk/lib/lwip/src/include/lwip/apps
)

target_link_libraries(${PROJECT_NAME} pico_stdlib pico_unique_id pico_lwip pico_lwip_mdns tinyusb_device)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT_NAME})
