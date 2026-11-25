add_executable(kr216_emu main.cpp)
target_link_libraries(kr216_emu PRIVATE kr216_core)
set_target_properties(kr216_emu PROPERTIES OUTPUT_NAME kr216)
