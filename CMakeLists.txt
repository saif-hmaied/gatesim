cmake_minimum_required(VERSION 3.20)
project(gatesim VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gatesim apps/main.cpp)
target_compile_options(gatesim PRIVATE -Wall -Wextra -Wpedantic -Werror)

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

add_executable(gatesim_tests tests/test_sanity.cpp)
target_compile_options(gatesim_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(gatesim_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(gatesim_tests)
