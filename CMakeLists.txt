cmake_minimum_required(VERSION 3.16)
project(digital_instruments C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(SDL3 REQUIRED)

add_executable(digital_instruments demo.c instrument.c effects.c)
target_link_libraries(digital_instruments SDL3::SDL3-static)
install(TARGETS digital_instruments)