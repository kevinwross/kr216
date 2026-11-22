add_executable(length_map_test length_map_test.cpp)
target_link_libraries(length_map_test PRIVATE kr216_core)
add_test(NAME length_map COMMAND length_map_test)

add_executable(vector_test vector_test.cpp)
target_link_libraries(vector_test PRIVATE kr216_core)
add_test(NAME vectors COMMAND vector_test)

add_executable(machine_test machine_test.cpp)
target_link_libraries(machine_test PRIVATE kr216_core)
add_test(NAME machine COMMAND machine_test)

add_executable(image_test image_test.cpp)
target_link_libraries(image_test PRIVATE kr216_core)
add_test(NAME image COMMAND image_test)

add_executable(canonical_test canonical_test.cpp)
target_link_libraries(canonical_test PRIVATE kr216_core)
add_test(NAME canonical COMMAND canonical_test)

add_executable(fuzz_test fuzz_test.cpp)
target_link_libraries(fuzz_test PRIVATE kr216_core)
add_test(NAME fuzz COMMAND fuzz_test)
