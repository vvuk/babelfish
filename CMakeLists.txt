cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

include(pico_sdk_import.cmake)
project(babelfish C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(PICO_NO_FLASH 1)

# set global define USBH_DEBUG=2
add_compile_definitions(USBH_DEBUG=2)

pico_sdk_init()

# include(${CMAKE_CURRENT_LIST_DIR}/external/pico-pio-usb/CMakeLists.txt)

add_executable(babelfish
  src/main.c
  src/bootmode.c
  src/hid_app.c
  src/host_sun.c
  src/host_sun_mouse.c
  src/host_sun_keyboard.c
  src/host_adb.c
  src/host_apollo.c
  src/output.c
  src/debug.c
  src/usb_descriptors.c
  src/usb_reset_interface.c
  src/hw_aux.c
  src/cmd.c
)

target_include_directories(babelfish PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(babelfish PUBLIC
  pico_stdlib
  pico_sync
  pico_multicore
  pico_unique_id
  pico_usb_reset_interface
  tinyusb_host
  tinyusb_device
  tinyusb_pico_pio_usb
)

# enable usb output, disable uart output
#pico_enable_stdio_usb(babelfish 1)
pico_enable_stdio_uart(babelfish 0)

target_compile_definitions(babelfish PUBLIC
  DEBUG
)

pico_add_extra_outputs(babelfish)

add_executable(babelfish_test
  src/babelfish_test.c
  src/debug.c
  src/usb_descriptors.c
  src/usb_reset_interface.c
  src/hw_aux.c
  src/output.c
)

target_include_directories(babelfish_test PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(babelfish_test PUBLIC
  pico_stdlib
  pico_sync
  pico_multicore
  pico_unique_id
  pico_usb_reset_interface
  tinyusb_host
  tinyusb_device
  tinyusb_pico_pio_usb
)
target_compile_definitions(babelfish_test PRIVATE
  DEBUG
  BABELFISH_TEST
)

pico_enable_stdio_uart(babelfish_test 0)

pico_add_extra_outputs(babelfish_test)

add_executable(hwtest
        src/hwtest.c
)

target_link_libraries(hwtest PUBLIC
        pico_stdlib
        pico_sync
        pico_multicore
        pico_unique_id
        pico_usb_reset_interface
)
target_compile_definitions(hwtest PRIVATE
        DEBUG
)
pico_enable_stdio_uart(hwtest 0)
pico_enable_stdio_usb(hwtest 1)

pico_add_extra_outputs(hwtest)
