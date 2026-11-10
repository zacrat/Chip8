cmake_minimum_required(VERSION 4.4)
project(untitled1 C)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED CONFIG)

add_executable(untitled1 main.c)
target_link_libraries(untitled1 PRIVATE SDL3::SDL3)

