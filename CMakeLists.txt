cmake_minimum_required(VERSION 3.13)
set(PICO_SDK_PATH "/opt/UnitySrc/proj/pico-sdk")
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

include(pico_sdk_import.cmake)
project(usb2multi C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# set global define USBH_DEBUG=2
add_compile_definitions(USBH_DEBUG=2)

pico_sdk_init()

# include(${CMAKE_CURRENT_LIST_DIR}/external/pico-pio-usb/CMakeLists.txt)

add_executable(usb2multi
  main.c
  hid_app.c
  sun.c
  sun_mouse.c
  sun_keyboard.c
  adb.c
  apollo.c
  debug.c
)
target_include_directories(usb2multi PUBLIC
  ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(usb2multi PUBLIC
  pico_stdlib
  pico_multicore
  tinyusb_host
  tinyusb_board
  tinyusb_pico_pio_usb)

# enable usb output, disable uart output
pico_enable_stdio_usb(usb2multi 1)
pico_enable_stdio_uart(usb2multi 0)

target_compile_definitions(usb2multi PRIVATE
  DEBUG
)

pico_enable_stdio_usb(usb2multi 0)
pico_enable_stdio_uart(usb2multi 0)

pico_add_extra_outputs(usb2multi)

add_executable(babelfish_test
  babelfish_test.c
  ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

target_include_directories(babelfish_test PUBLIC
  ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(babelfish_test PUBLIC
  pico_stdlib
  pico_multicore
  pico_stdio
  pico_stdio_semihosting
  tinyusb_board
  tinyusb_host_base
  tinyusb_host
  tinyusb_pico_pio_usb)
target_compile_definitions(babelfish_test PRIVATE
  DEBUG
)

pico_enable_stdio_usb(babelfish_test 0)
pico_enable_stdio_uart(babelfish_test 0)

pico_add_extra_outputs(babelfish_test)
