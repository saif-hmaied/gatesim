# Runs the gatesim program once and checks its exit code and its output.
#
# Variables (given with -D by CMakeLists.txt):
#   GATESIM         path to the gatesim program
#   ARGS            its arguments, separated by '|'
#   EXPECTED_CODE   the exit code it must return
#   EXPECTED_REGEX  a regular expression its output (stdout + stderr) must match

string(REPLACE "|" ";" args "${ARGS}")
execute_process(
    COMMAND "${GATESIM}" ${args}
    RESULT_VARIABLE code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
set(output "${out}${err}")

if(NOT code EQUAL EXPECTED_CODE)
    message(FATAL_ERROR "expected exit code ${EXPECTED_CODE}, got ${code}\n--- output ---\n${output}")
endif()
if(NOT output MATCHES "${EXPECTED_REGEX}")
    message(FATAL_ERROR "output does not match '${EXPECTED_REGEX}'\n--- output ---\n${output}")
endif()
