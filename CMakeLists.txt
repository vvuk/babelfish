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

  new_stdio_usb/stdio_usb.c
  new_stdio_usb/stdio_usb_descriptors.c
)

target_include_directories(babelfish PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/new_stdio_usb/include)

target_link_libraries(babelfish PUBLIC
  pico_stdlib
  pico_sync
  pico_multicore
  pico_unique_id
  tinyusb_host
  tinyusb_device_unmarked  # special _unmarked because we've got our usb hack
  tinyusb_pico_pio_usb)

# enable usb output, disable uart output
pico_enable_stdio_usb(babelfish 0)
pico_enable_stdio_uart(babelfish 1)

target_compile_definitions(babelfish PUBLIC
  DEBUG
  PICO_DEFAULT_UART_TX_PIN=0
  PICO_DEFAULT_UART_RX_PIN=1
)

pico_add_extra_outputs(babelfish)

if (FALSE)
add_executable(babelfish_test
  src/babelfish_test.c
)

target_include_directories(babelfish_test PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(babelfish_test PUBLIC
  pico_stdlib
  pico_multicore
  pico_stdio
  tinyusb_host
  tinyusb_board
  tinyusb_pico_pio_usb)
target_compile_definitions(babelfish_test PRIVATE
  DEBUG
)

pico_enable_stdio_usb(babelfish_test 0)
pico_enable_stdio_uart(babelfish_test 0)

pico_add_extra_outputs(babelfish_test)
endif()
