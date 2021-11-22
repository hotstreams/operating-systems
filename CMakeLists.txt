cmake_minimum_required(VERSION 3.10)
project(lab1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g")

link_libraries(pthread)
add_executable(lab1 main.c)