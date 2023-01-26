cmake_minimum_required(VERSION 3.13)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
set(PICO_SDK_PATH "/home/vladimir/proj/pico-sdk")

include(pico_sdk_import.cmake)
project(usb2multi C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

include(${CMAKE_CURRENT_LIST_DIR}/external/pico-pio-usb/CMakeLists.txt)

add_executable(usb2multi
  main.c
  hid_app.c
  sun.c
  sun_mouse.c
  sun_keyboard.c
  adb.c
  apollo.c
)
target_include_directories(usb2multi PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/external/pico-pio-usb/src)
target_link_libraries(usb2multi PUBLIC pico_stdlib tinyusb_host tinyusb_board pico_pio_usb)

pico_enable_stdio_usb(usb2multi 0)
pico_enable_stdio_uart(usb2multi 0)

pico_add_extra_outputs(usb2multi)

