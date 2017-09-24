cmake_minimum_required(VERSION 2.8)

project(collatz2)
set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-msse -msse2 -msse3")

set(${PROJECT_NAME}_SOURCES
    "src/candidates_file.cpp"
    "src/candidates_file.h"
    "src/cleared_file.cpp"
    "src/cleared_file.h"
    "src/csv_file.cpp"
    "src/csv_file.h"
    "src/worktodo_file.cpp"
    "src/worktodo_file.h"
    "src/main.cpp"
    "src/math256.cpp"
    "src/math256.h"
    )

add_executable(${PROJECT_NAME}  ${${PROJECT_NAME}_SOURCES})
