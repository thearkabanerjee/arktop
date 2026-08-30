cmake_minimum_required(VERSION 3.16)

project(arktop)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic"
)

add_executable(
    arktop

    src/main.cpp
    # src/Process.cpp
    src/MacProcessProvider.cpp
    src/Terminal.cpp
)
