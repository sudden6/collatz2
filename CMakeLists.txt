cmake_minimum_required(VERSION 2.8)

project(collatz2)
set (CMAKE_CXX_STANDARD 11)

set(${PROJECT_NAME}_SOURCES
    "src/main.cpp"
    "src/math256.cpp"
    "src/math256.h"
    )

add_executable(${PROJECT_NAME}  ${${PROJECT_NAME}_SOURCES})
