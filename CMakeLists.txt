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
    src/simulator.cpp
    src/timing.cpp
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
    tests/test_simulator.cpp
    tests/test_timing.cpp
)
target_compile_options(gatesim_tests PRIVATE ${GATESIM_WARNINGS})
target_link_libraries(gatesim_tests PRIVATE gatesim_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(gatesim_tests)

# ---------- Command-line tests ----------
# Runs the real gatesim program and checks its exit code and output.
# Usage: gatesim_cli_test(<name> <expected exit code> <regex> <arguments...>)
function(gatesim_cli_test name expected_code expected_regex)
    string(REPLACE ";" "|" args "${ARGN}")
    add_test(NAME cli.${name}
        COMMAND ${CMAKE_COMMAND}
            -DGATESIM=$<TARGET_FILE:gatesim>
            -DARGS=${args}
            -DEXPECTED_CODE=${expected_code}
            -DEXPECTED_REGEX=${expected_regex}
            -P ${CMAKE_SOURCE_DIR}/tests/cli/run_cli_test.cmake
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

gatesim_cli_test(check_full_adder      0 "0 errors, 0 warnings"            check examples/full_adder.v)
gatesim_cli_test(check_multidriver     1 "net 'y' has 2 drivers: g1 g2"    check examples/bad_multidriver.v)
gatesim_cli_test(check_undriven        1 "net 'w' has no driver"           check examples/bad_undriven.v)
gatesim_cli_test(check_loop            1 "combinational loop: g[12]"       check examples/bad_loop.v)
gatesim_cli_test(check_syntax_error    1 "bad_syntax.v:5:14: error"        check examples/bad_syntax.v)
gatesim_cli_test(check_missing_file    1 "cannot open"                     check examples/no_such_file.v)

gatesim_cli_test(sim_input             0 "cout = 1[\r\n]+sum = 0"          sim examples/full_adder.v --input a=1 b=0 cin=1)
gatesim_cli_test(sim_all_header        0 "a b cin . cout sum"              sim examples/full_adder.v --all)
gatesim_cli_test(sim_all_last_row      0 "1 1 1   . 1    1"                sim examples/full_adder.v --all)
gatesim_cli_test(sim_missing_input     1 "missing value for input 'cin'"   sim examples/full_adder.v --input a=1 b=0)
gatesim_cli_test(sim_bad_value         1 "must be 0 or 1"                  sim examples/full_adder.v --input a=1 b=2 cin=1)
gatesim_cli_test(sim_unknown_input     1 "'d' is not an input"             sim examples/full_adder.v --input a=1 d=0 cin=1)
gatesim_cli_test(sim_twice             1 "given twice"                     sim examples/full_adder.v --input a=1 a=0 b=0 cin=1)
gatesim_cli_test(sim_bad_circuit       1 "cannot simulate"                 sim examples/bad_loop.v --all)

gatesim_cli_test(timing_arrivals       0 "cout  7 ns[\r\n]+  sum   6 ns"   timing examples/full_adder.v)
gatesim_cli_test(timing_path           0 "critical path \\(7 ns\\):[\r\n]+  a -> g1 -> w1 -> g4 -> w3 -> g5 -> cout"
                                                                          timing examples/full_adder.v)
gatesim_cli_test(timing_slack_ok       0 "clock 8 ns, slack 1 ns: OK"      timing examples/full_adder.v --clock 8)
gatesim_cli_test(timing_slack_zero     0 "slack 0 ns: OK"                  timing examples/full_adder.v --clock 7)
gatesim_cli_test(timing_slack_fails    1 "clock 5 ns, slack -2 ns: FAILS"  timing examples/full_adder.v --clock 5)
gatesim_cli_test(timing_fractional     0 "slack 0.5 ns: OK"                timing examples/full_adder.v --clock 7.5)
gatesim_cli_test(timing_clock_zero     1 "must be a positive number"       timing examples/full_adder.v --clock 0)
gatesim_cli_test(timing_clock_negative 1 "must be a positive number"       timing examples/full_adder.v --clock -3)
gatesim_cli_test(timing_clock_text     1 "must be a positive number"       timing examples/full_adder.v --clock 5x)
gatesim_cli_test(timing_bad_circuit    1 "cannot analyze the timing"       timing examples/bad_loop.v)
gatesim_cli_test(timing_wrong_flag     1 "usage:"                          timing examples/full_adder.v --clk 5)

gatesim_cli_test(no_arguments         1 "usage:")
gatesim_cli_test(unknown_command       1 "usage:"                          run examples/full_adder.v)
gatesim_cli_test(sim_without_mode      1 "usage:"                          sim examples/full_adder.v)
