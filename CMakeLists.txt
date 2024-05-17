cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
#include(pico_extras_import.cmake)

project(dan_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(Config)
add_subdirectory(LCD)
add_subdirectory(Infrared)

include_directories(Config)
include_directories(LCD)
include_directories(Infrared)

add_executable(main main.c)

pico_add_extra_outputs(main)

target_link_libraries(main 
    pico_stdlib 
    pico_multicore 
    hardware_spi 
    LCD 
    Config 
    Infrared)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)