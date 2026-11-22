# Compiles the fixed density corpus, links it with section GC and checks the
# average static bytes per instruction against the 2.5-byte target.

foreach(variable KRC KRLD BENCH SOURCE_ROOT WORK)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK}")
set(runtime "${SOURCE_ROOT}/../krcompiler/targets/kr216/runtime")

function(compile_object source output)
    execute_process(
        COMMAND "${KRC}" -c --target=kr216 "${source}" -o "${output}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output_text
        ERROR_VARIABLE error_text)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "krc failed for ${source}:\n${output_text}${error_text}")
    endif()
endfunction()

compile_object("${runtime}/crt0.s" "${WORK}/crt0.kro")
compile_object("${runtime}/syscalls.s" "${WORK}/syscalls.kro")
compile_object("${runtime}/eh.s" "${WORK}/eh.kro")
compile_object("${runtime}/libc.c" "${WORK}/libc.kro")

set(sources
    string_copy.c
    printf.c
    container.cpp
    recursive.c
    fp_loop.c)
set(images)
foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    compile_object("${SOURCE_ROOT}/benchmarks/${source}" "${WORK}/${name}.kro")
    execute_process(
        COMMAND "${KRLD}" --gc-sections
                "${WORK}/crt0.kro" "${WORK}/syscalls.kro" "${WORK}/eh.kro"
                "${WORK}/libc.kro" "${WORK}/${name}.kro"
                -o "${WORK}/${name}.k216"
        RESULT_VARIABLE link_result
        OUTPUT_VARIABLE link_output
        ERROR_VARIABLE link_error)
    if(NOT link_result EQUAL 0)
        message(FATAL_ERROR "krld failed for ${name}:\n${link_output}${link_error}")
    endif()
    list(APPEND images "${WORK}/${name}.k216")
endforeach()

execute_process(
    COMMAND "${BENCH}" --max-density 2.5 ${images}
    RESULT_VARIABLE bench_result
    OUTPUT_VARIABLE bench_output
    ERROR_VARIABLE bench_error)
message(STATUS "${bench_output}")
if(NOT bench_result EQUAL 0)
    message(FATAL_ERROR "density benchmark failed:\n${bench_output}${bench_error}")
endif()
