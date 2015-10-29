cmake_minimum_required(VERSION 3.3)
project(OS_HW1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES mysh.c)
add_executable(OS_HW1 ${SOURCE_FILES})