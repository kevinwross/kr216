# Builds one compiler-generated kr216 C, C++ or assembly program and runs it.
#
# Required: KRC, KRLD, EMU, SOURCE_ROOT, SOURCE, WORK, EXPECT_EXIT
# Optional: EXTRA_SOURCE, EXPECT_DISPLAY, EXPECT_CONSOLE_ABSENT, DISPLAY_INPUT, NO_CRT0,
#           MAX_INSTRUCTIONS, EXPECT_OUTPUT, CHECK_SYMBOLS
#
# The program's stdin/stdout is the text display (the runtime's putchar/puts/printf and
# getchar), so EXPECT_DISPLAY asserts on the screen text; EXPECT_OUTPUT stays the debug serial
# console (SYS 1 / __krc_debug_write). DISPLAY_INPUT is the scripted line-editor input, one line
# per `|` separator; the display input source closes when it is exhausted, so a getchar loop
# ending at EOF terminates the guest.

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
compile_object("${runtime}/display.c" "${WORK}/display.kro")
list(APPEND objects "${WORK}/syscalls.kro" "${WORK}/eh.kro" "${WORK}/libc.kro"
                   "${WORK}/display.kro")

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

if(NOT DEFINED MAX_INSTRUCTIONS OR MAX_INSTRUCTIONS STREQUAL "")
    set(MAX_INSTRUCTIONS 1000000)
endif()

# The `|` separator keeps newlines out of the test command line: a scripted line becomes a
# newline in a file the emulator reads, and a trailing `|` supplies the final newline.
set(run_arguments --max-instructions ${MAX_INSTRUCTIONS})
if(DEFINED DISPLAY_INPUT AND NOT DISPLAY_INPUT STREQUAL "")
    string(REPLACE "|" "\n" display_input "${DISPLAY_INPUT}")
    file(WRITE "${WORK}/display-input.txt" "${display_input}")
    list(APPEND run_arguments --display-inputfile "${WORK}/display-input.txt")
endif()
if(DEFINED EXPECT_DISPLAY AND NOT EXPECT_DISPLAY STREQUAL "")
    list(APPEND run_arguments --display-dump)
endif()
list(APPEND run_arguments "${image}")

execute_process(
    COMMAND "${EMU}" ${run_arguments}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result)
string(REPLACE "\r\n" "\n" output "${output}")
if(NOT result EQUAL EXPECT_EXIT)
    message(FATAL_ERROR "expected exit ${EXPECT_EXIT}, got ${result}\n${output}${error}")
endif()

# With a screen assertion the emulator prints `console:` and `display:` sections; the console
# assertion then applies to the console bytes only, so a string on the screen cannot satisfy it.
set(console_section "${output}")
set(display_section "")
if(DEFINED EXPECT_DISPLAY AND NOT EXPECT_DISPLAY STREQUAL "")
    string(FIND "${output}" "console:\n" console_marker)
    string(FIND "${output}" "\ndisplay:\n" display_marker)
    if(console_marker EQUAL -1 OR display_marker EQUAL -1)
        message(FATAL_ERROR "expected console:/display: sections, got:\n${output}")
    endif()
    string(LENGTH "console:\n" console_prefix)
    math(EXPR console_start "${console_marker} + ${console_prefix}")
    math(EXPR console_length "${display_marker} - ${console_start}")
    string(SUBSTRING "${output}" ${console_start} ${console_length} console_section)
    string(LENGTH "\ndisplay:\n" display_prefix)
    math(EXPR display_start "${display_marker} + ${display_prefix}")
    string(SUBSTRING "${output}" ${display_start} -1 display_section)
endif()

if(DEFINED EXPECT_OUTPUT AND NOT EXPECT_OUTPUT STREQUAL "")
    string(FIND "${console_section}" "${EXPECT_OUTPUT}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "expected console output '${EXPECT_OUTPUT}', got:\n${console_section}")
    endif()
endif()
if(DEFINED EXPECT_CONSOLE_ABSENT AND NOT EXPECT_CONSOLE_ABSENT STREQUAL "")
    string(FIND "${console_section}" "${EXPECT_CONSOLE_ABSENT}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "console unexpectedly carries '${EXPECT_CONSOLE_ABSENT}':\n${console_section}")
    endif()
endif()
if(DEFINED EXPECT_DISPLAY AND NOT EXPECT_DISPLAY STREQUAL "")
    foreach(expected IN LISTS EXPECT_DISPLAY)
        string(FIND "${display_section}" "${expected}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "expected display text '${expected}', got:\n${display_section}")
        endif()
    endforeach()
endif()
if(DEFINED CHECK_SYMBOLS AND CHECK_SYMBOLS)
    execute_process(
        COMMAND "${KRC}" --dump-symbols "${image}"
        OUTPUT_VARIABLE symbols
        ERROR_VARIABLE error_text
        RESULT_VARIABLE symbols_result)
    if(NOT symbols_result EQUAL 0)
        message(FATAL_ERROR "krc --dump-symbols failed (${symbols_result}):\n${error_text}")
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
