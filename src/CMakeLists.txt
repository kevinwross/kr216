add_library(kr216_core STATIC
    encoding.cpp
)
target_include_directories(kr216_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
