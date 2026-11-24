# Builds one compiler-generated kr216 C or assembly program and runs it.
#
# Required: KRC, KRLD, EMU, SOURCE_ROOT, SOURCE, WORK, EXPECT_EXIT
# Optional: EXTRA_SOURCE, EXPECT_OUTPUT, NO_CRT0

if(NOT DEFINED KRC OR NOT DEFINED KRLD OR NOT DEFINED EMU OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED SOURCE OR NOT DEFINED WORK OR NOT DEFINED EXPECT_EXIT)
    message(FATAL_ERROR "KRC, KRLD, EMU, SOURCE_ROOT, SOURCE, WORK and EXPECT_EXIT are required")
endif()
foreach(tool KRC KRLD EMU)
    if(NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "${tool} not found: ${${tool}}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK}")

set(runtime "${SOURCE_ROOT}/../krcompiler/targets/kr216/runtime")
function(compile_object source output)
    execute_process(
        COMMAND "${KRC}" -c --target=kr216 "${source}" -o "${output}"
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error
        RESULT_VARIABLE compile_result)
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "krc failed for ${source} (${compile_result}):\n${compile_output}${compile_error}")
    endif()
endfunction()

set(objects)
if(NOT NO_CRT0)
    compile_object("${runtime}/crt0.s" "${WORK}/crt0.kro")
    list(APPEND objects "${WORK}/crt0.kro")
endif()
compile_object("${runtime}/syscalls.s" "${WORK}/syscalls.kro")
compile_object("${runtime}/eh.s" "${WORK}/eh.kro")
compile_object("${runtime}/libc.c" "${WORK}/libc.kro")
list(APPEND objects "${WORK}/syscalls.kro" "${WORK}/eh.kro" "${WORK}/libc.kro")

compile_object("${SOURCE}" "${WORK}/source.kro")
list(APPEND objects "${WORK}/source.kro")
if(DEFINED EXTRA_SOURCE AND NOT EXTRA_SOURCE STREQUAL "")
    compile_object("${EXTRA_SOURCE}" "${WORK}/extra.kro")
    list(APPEND objects "${WORK}/extra.kro")
endif()

set(image "${WORK}/program.kr216")
execute_process(
    COMMAND "${KRLD}" ${objects} -o "${image}"
    OUTPUT_VARIABLE link_output
    ERROR_VARIABLE link_error
    RESULT_VARIABLE link_result)
if(NOT link_result EQUAL 0)
    message(FATAL_ERROR "krld failed (${link_result}):\n${link_output}${link_error}")
endif()

execute_process(
    COMMAND "${EMU}" --max-instructions 1000000 "${image}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result)
string(REPLACE "\r\n" "\n" output "${output}")
if(NOT result EQUAL EXPECT_EXIT)
    message(FATAL_ERROR "expected exit ${EXPECT_EXIT}, got ${result}\n${output}${error}")
endif()
if(DEFINED EXPECT_OUTPUT AND NOT EXPECT_OUTPUT STREQUAL "")
    string(FIND "${output}" "${EXPECT_OUTPUT}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "expected output '${EXPECT_OUTPUT}', got:\n${output}")
    endif()
endif()
if(DEFINED CHECK_SYMBOLS AND CHECK_SYMBOLS)
    execute_process(
        COMMAND "${KRC}" --dump-symbols "${image}"
        OUTPUT_VARIABLE symbols
        ERROR_VARIABLE symbols_error
        RESULT_VARIABLE symbols_result)
    if(NOT symbols_result EQUAL 0)
        message(FATAL_ERROR "krc --dump-symbols failed (${symbols_result}):\n${symbols_error}")
    endif()
    if(NOT symbols MATCHES "KDBG target=4")
        message(FATAL_ERROR "KDBG target 4 missing:\n${symbols}")
    endif()
    if(NOT symbols MATCHES "function main")
        message(FATAL_ERROR "KDBG function main missing:\n${symbols}")
    endif()
    if(NOT symbols MATCHES "source 0")
        message(FATAL_ERROR "KDBG source record missing:\n${symbols}")
    endif()
endif()
