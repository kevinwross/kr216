add_executable(length_map_test length_map_test.cpp)
target_link_libraries(length_map_test PRIVATE kr216_core)
add_test(NAME length_map COMMAND length_map_test)

add_executable(vector_test vector_test.cpp)
target_link_libraries(vector_test PRIVATE kr216_core)
add_test(NAME vectors COMMAND vector_test)

add_executable(machine_test machine_test.cpp)
target_link_libraries(machine_test PRIVATE kr216_core)
add_test(NAME machine COMMAND machine_test)

add_executable(display_test display_test.cpp)
target_link_libraries(display_test PRIVATE kr216_core)
add_test(NAME display COMMAND display_test)

add_executable(image_test image_test.cpp)
target_link_libraries(image_test PRIVATE kr216_core)
add_test(NAME image COMMAND image_test)

add_executable(canonical_test canonical_test.cpp)
target_link_libraries(canonical_test PRIVATE kr216_core)
add_test(NAME canonical COMMAND canonical_test)

add_executable(fuzz_test fuzz_test.cpp)
target_link_libraries(fuzz_test PRIVATE kr216_core)
add_test(NAME fuzz COMMAND fuzz_test)
add_executable(modes_test modes_test.cpp)
target_link_libraries(modes_test PRIVATE kr216_core)
add_test(NAME modes COMMAND modes_test)

add_executable(fp_test fp_test.cpp)
target_link_libraries(fp_test PRIVATE kr216_core)
add_test(NAME fp COMMAND fp_test)
add_executable(frames_test frames_test.cpp)
target_link_libraries(frames_test PRIVATE kr216_core)
add_test(NAME frames COMMAND frames_test)
add_executable(traps_test traps_test.cpp)
target_link_libraries(traps_test PRIVATE kr216_core)
add_test(NAME traps COMMAND traps_test)





if(KR216_BUILD_GUEST_TESTS)
    function(add_guest_program_test name source expect_exit expect_display)
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                    "-DKRC=${KRG_KRC}"
                    "-DKRLD=${KRG_KRLD}"
                    "-DEMU=$<TARGET_FILE:kr216_emu>"
                    "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                    "-DSOURCE=${source}"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name}"
                    "-DEXPECT_EXIT=${expect_exit}"
                    "-DEXPECT_DISPLAY=${expect_display}"
                    -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    endfunction()

    add_test(NAME density_benchmark
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DBENCH=$<TARGET_FILE:kr216-bench>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/density_benchmark"
                -P ${CMAKE_SOURCE_DIR}/cmake/RunDensityBenchmark.cmake)

    add_guest_program_test(guest_hello
        "${CMAKE_SOURCE_DIR}/src216/hello.c" 0 "Hello, KR216!")
    add_test(NAME guest_hello_unlimited
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/src216/hello.c"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_hello_unlimited"
                "-DEXPECT_EXIT=0"
                "-DEXPECT_DISPLAY=Hello, KR216!"
                "-DMAX_INSTRUCTIONS=0"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    add_test(NAME guest_hello_max_steps_alias
        COMMAND $<TARGET_FILE:kr216_emu> --display-dump --max-steps 0
                "${CMAKE_CURRENT_BINARY_DIR}/guest_hello_unlimited/program.kr216")
    set_tests_properties(guest_hello_max_steps_alias PROPERTIES
        DEPENDS guest_hello_unlimited
        PASS_REGULAR_EXPRESSION "Hello, KR216!")
    add_guest_program_test(guest_abi
        "${CMAKE_SOURCE_DIR}/src216/abi.c" 42 "abi ok")
    add_guest_program_test(guest_long_literal
        "${CMAKE_SOURCE_DIR}/src216/long_literal.c" 42 "long ok")
    add_guest_program_test(guest_runtime
        "${CMAKE_SOURCE_DIR}/src216/runtime.c" 42 "aa")
    add_guest_program_test(guest_printf
        "${CMAKE_SOURCE_DIR}/src216/printf.c" 42 "42 ok A ff")
    add_guest_program_test(guest_long_args
        "${CMAKE_SOURCE_DIR}/src216/long_args.c" 42 "")
    add_guest_program_test(guest_aggregate_param
        "${CMAKE_SOURCE_DIR}/src216/aggregate_param.c" 42 "")
    add_guest_program_test(guest_shift32
        "${CMAKE_SOURCE_DIR}/src216/shift32.c" 42 "")
    add_guest_program_test(guest_shift_dynamic
        "${CMAKE_SOURCE_DIR}/src216/shift_dynamic.c" 42 "")
    add_guest_program_test(guest_mul32
        "${CMAKE_SOURCE_DIR}/src216/mul32.c" 42 "")
    add_guest_program_test(guest_div32
        "${CMAKE_SOURCE_DIR}/src216/div32.c" 42 "")
    add_guest_program_test(guest_float_math
        "${CMAKE_SOURCE_DIR}/src216/float_math.c" 42 "")
    add_guest_program_test(guest_double_math
        "${CMAKE_SOURCE_DIR}/src216/double_math.c" 42 "")
    add_guest_program_test(guest_float_args
        "${CMAKE_SOURCE_DIR}/src216/float_args.c" 42 "")
    add_guest_program_test(guest_double_args
        "${CMAKE_SOURCE_DIR}/src216/double_args.c" 42 "")
    add_guest_program_test(guest_long_long_add
        "${CMAKE_SOURCE_DIR}/src216/long_long_add.c" 42 "")
    add_guest_program_test(guest_float_compare
        "${CMAKE_SOURCE_DIR}/src216/float_compare.c" 42 "")
    add_guest_program_test(guest_double_compare
        "${CMAKE_SOURCE_DIR}/src216/double_compare.c" 42 "")
    add_guest_program_test(guest_float_convert
        "${CMAKE_SOURCE_DIR}/src216/float_convert.c" 42 "")
    add_guest_program_test(guest_signed_mul
        "${CMAKE_SOURCE_DIR}/src216/signed_mul.c" 42 "")
    add_guest_program_test(guest_long_long_compare
        "${CMAKE_SOURCE_DIR}/src216/long_long_compare.c" 42 "")
    add_guest_program_test(guest_long_long_shift
        "${CMAKE_SOURCE_DIR}/src216/long_long_shift.c" 42 "")
    add_guest_program_test(guest_long_long_shift_dynamic
        "${CMAKE_SOURCE_DIR}/src216/long_long_shift_dynamic.c" 42 "")
    add_guest_program_test(guest_long_long_mul
        "${CMAKE_SOURCE_DIR}/src216/long_long_mul.c" 42 "")
    add_guest_program_test(guest_long_long_div
        "${CMAKE_SOURCE_DIR}/src216/long_long_div.c" 42 "")
    add_guest_program_test(guest_long_long_args
        "${CMAKE_SOURCE_DIR}/src216/long_long_args.c" 42 "")
    add_guest_program_test(guest_float_literal
        "${CMAKE_SOURCE_DIR}/src216/float_literal.c" 42 "")
    add_guest_program_test(guest_float_convert_wide
        "${CMAKE_SOURCE_DIR}/src216/float_convert_wide.c" 42 "")
    add_guest_program_test(guest_printf_long
        "${CMAKE_SOURCE_DIR}/src216/printf_long.c" 42 "")
    add_guest_program_test(guest_float_third_arg
        "${CMAKE_SOURCE_DIR}/src216/float_third_arg.c" 42 "")
    add_guest_program_test(guest_text_sections
        "${CMAKE_SOURCE_DIR}/src216/text_sections.s" 42 "")
    add_guest_program_test(guest_sub_immediate
        "${CMAKE_SOURCE_DIR}/src216/sub_immediate.s" 42 "")
    add_guest_program_test(guest_cpp_m4
        "${CMAKE_SOURCE_DIR}/src216/cpp_m4.cpp" 42 "")
    add_guest_program_test(guest_cpp_rtti
        "${CMAKE_SOURCE_DIR}/src216/cpp_rtti.cpp" 42 "")
    add_guest_program_test(guest_cpp_containers
        "${CMAKE_SOURCE_DIR}/src216/cpp_containers.cpp" 42 "")
    add_guest_program_test(guest_cpp_string_ops
        "${CMAKE_SOURCE_DIR}/src216/cpp_string_ops.cpp" 42 "")
    add_guest_program_test(guest_cpp_vector_objects
        "${CMAKE_SOURCE_DIR}/src216/cpp_vector_objects.cpp" 42 "")
    add_guest_program_test(guest_cpp_exceptions
        "${CMAKE_SOURCE_DIR}/src216/cpp_exceptions.cpp" 42 "")
    add_guest_program_test(guest_cpp_exceptions_frames
        "${CMAKE_SOURCE_DIR}/src216/cpp_exceptions_frames.cpp" 42 "")
    add_guest_program_test(guest_cpp_exception_object
        "${CMAKE_SOURCE_DIR}/src216/cpp_exception_object.cpp" 42 "")
    add_guest_program_test(guest_signed_div
        "${CMAKE_SOURCE_DIR}/src216/signed_div.c" 42 "")
    add_test(NAME guest_display
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/src216/display.c"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_display"
                "-DEXPECT_EXIT=42"
                "-DEXPECT_DISPLAY=display ok"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    # stdio end to end: printf writes the screen, getchar/getc read the display line editor,
    # the guest observes EOF when the host closes the input script, and the console keeps only
    # the explicit debug-serial write.
    add_test(NAME guest_stdio
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/src216/stdio.c"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_stdio"
                "-DEXPECT_EXIT=42"
                "-DDISPLAY_INPUT=ada|b|z|"
                "-DEXPECT_DISPLAY=name? ada;hello ada;rand 16838;second? b;got b (1);extra z;tail: eof;bye;putchar ok"
                "-DEXPECT_OUTPUT=debug serial ok"
                "-DEXPECT_CONSOLE_ABSENT=hello"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    # The KRCompiler tictactoe example, linked with the kr216 runtime and no getchar/rand shim.
    # Four moves finish the round (the computer wins) and the closing 'Play again?' read gets
    # EOF from the exhausted display input, so the program exits 0.
    add_test(NAME guest_tictactoe
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/../krcompiler/examples/tictactoe.cpp"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_tictactoe"
                "-DEXPECT_EXIT=0"
                "-DDISPLAY_INPUT=1|9|7|2|"
                "-DEXPECT_DISPLAY=The computer wins.;Input ended.;Thanks for playing."
                "-DMAX_INSTRUCTIONS=5000000"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    add_test(NAME guest_init_array
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/src216/init_array.s"
                "-DEXTRA_SOURCE=${CMAKE_SOURCE_DIR}/src216/init_main.c"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_init_array"
                "-DEXPECT_EXIT=42"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    add_test(NAME guest_kdbg
        COMMAND ${CMAKE_COMMAND}
                "-DKRC=${KRG_KRC}"
                "-DKRLD=${KRG_KRLD}"
                "-DEMU=$<TARGET_FILE:kr216_emu>"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSOURCE=${CMAKE_SOURCE_DIR}/src216/hello.c"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_kdbg"
                "-DEXPECT_EXIT=0"
                "-DEXPECT_DISPLAY=Hello, KR216!"
                "-DCHECK_SYMBOLS=ON"
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
    add_test(NAME guest_abi_callee_saved
        COMMAND ${CMAKE_COMMAND}
                -DKRC=${KRG_KRC}
                -DKRLD=${KRG_KRLD}
                -DEMU=$<TARGET_FILE:kr216_emu>
                -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
                -DSOURCE=${CMAKE_SOURCE_DIR}/src216/abi_callee.s
                -DEXTRA_SOURCE=${CMAKE_SOURCE_DIR}/src216/abi_target.c
                -DNO_CRT0=ON
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/guest_abi_callee_saved
                -DEXPECT_EXIT=42
                -P ${CMAKE_SOURCE_DIR}/cmake/BuildGuestProgram.cmake)
else()
    message(STATUS "guest program tests skipped: KRC_EXECUTABLE/KRLD_EXECUTABLE not found")
endif()
