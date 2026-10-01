cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(greenhouse_monitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(greenhouse_monitor
    greenhouse_monitor.c
)

target_link_libraries(greenhouse_monitor
    pico_stdlib
    hardware_adc
    hardware_pwm
)

pico_add_extra_outputs(greenhouse_monitor)

pico_enable_stdio_usb(greenhouse_monitor 1)
pico_enable_stdio_uart(greenhouse_monitor 1)
