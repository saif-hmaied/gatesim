cmake_minimum_required(VERSION 3.20)
project(gatesim VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gatesim apps/main.cpp)
target_compile_options(gatesim PRIVATE -Wall -Wextra -Wpedantic -Werror)
