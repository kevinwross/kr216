# Boundary check for the two source trees.
#
# src/ is host code built by the host toolchain; src216/ is guest code compiled by
# `krc --target=kr216`. Neither may include the other's headers (AGENTS.local.md,
# architecture boundaries). Run from CMakeLists.txt at configure time and again as the
# `boundary` CTest entry, so a build directory configured before a violation existed still
# fails the suite.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "CheckBoundary.cmake requires SOURCE_ROOT")
endif()

set(host_dir "${SOURCE_ROOT}/src")
set(guest_dir "${SOURCE_ROOT}/src216")
set(violations "")

file(GLOB_RECURSE host_files
     "${host_dir}/*.h" "${host_dir}/*.hpp" "${host_dir}/*.c" "${host_dir}/*.cc" "${host_dir}/*.cpp")
foreach(file IN LISTS host_files)
    file(READ "${file}" contents)
    if(contents MATCHES "#[ \t]*include[ \t]*[\"<][^\">]*src216")
        list(APPEND violations "${file}: host code includes a src216 header")
    endif()
endforeach()

file(GLOB_RECURSE guest_files
     "${guest_dir}/*.h" "${guest_dir}/*.hpp" "${guest_dir}/*.c" "${guest_dir}/*.cc"
     "${guest_dir}/*.cpp" "${guest_dir}/*.s" "${guest_dir}/*.S")
foreach(file IN LISTS guest_files)
    file(READ "${file}" contents)
    if(contents MATCHES "#[ \t]*include[ \t]*[\"<][^\">]*kr216/")
        list(APPEND violations "${file}: guest code includes a host header")
    endif()
endforeach()

foreach(file IN LISTS violations)
    message(SEND_ERROR "boundary violation: ${file}")
endforeach()
