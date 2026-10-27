cmake_minimum_required(VERSION 3.20)
project(gatesim VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GATESIM_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# ---------- Library ----------
add_library(gatesim_core
    src/lexer.cpp
    src/parser.cpp
    src/check.cpp
)
target_include_directories(gatesim_core PUBLIC include)
target_compile_options(gatesim_core PRIVATE ${GATESIM_WARNINGS})

# ---------- Command-line tool ----------
add_executable(gatesim apps/main.cpp)
target_compile_options(gatesim PRIVATE ${GATESIM_WARNINGS})
target_link_libraries(gatesim PRIVATE gatesim_core)

# ---------- Tests ----------
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(gatesim_tests
    tests/test_sanity.cpp
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_check.cpp
)
target_compile_options(gatesim_tests PRIVATE ${GATESIM_WARNINGS})
target_link_libraries(gatesim_tests PRIVATE gatesim_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(gatesim_tests)
