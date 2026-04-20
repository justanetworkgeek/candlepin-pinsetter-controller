cmake_minimum_required(VERSION 3.16)
project(candlepin_state_machine_viewer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)

add_executable(state_machine_viewer state_machine_viewer.c)
target_link_libraries(state_machine_viewer PRIVATE raylib)

if(WIN32)
    target_link_libraries(state_machine_viewer PRIVATE winmm)
endif()
