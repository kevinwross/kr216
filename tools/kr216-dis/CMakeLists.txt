add_executable(kr216_dis main.cpp)
target_link_libraries(kr216_dis PRIVATE kr216_core)
set_target_properties(kr216_dis PROPERTIES OUTPUT_NAME kr216-dis)
