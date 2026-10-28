cmake_minimum_required(VERSION 3.20)
project(gatesim VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GATESIM_WARNINGS -Wall -Wextra -Wpedantic -Werror)

add_library(gatesim_core
    src/lexer.cpp
    src/parser.cpp
    src/check.cpp
    src/simulator.cpp
    src/timing.cpp
)
target_include_directories(gatesim_core PUBLIC include)
target_compile_options(gatesim_core PRIVATE ${GATESIM_WARNINGS})

add_executable(gatesim apps/main.cpp)
target_compile_options(gatesim PRIVATE ${GATESIM_WARNINGS})
target_link_libraries(gatesim PRIVATE gatesim_core)
