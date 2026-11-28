cmake_minimum_required(VERSION 3.25)
project(kr216 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()

# The host text display is the shared krc::display package, installed by KRCompiler; this
# repository does not carry a copy of it (docs/display-abi.md). A configure without the
# package fails here rather than at link time.
find_package(krc-display CONFIG REQUIRED)

# src/ is host code, built by the host toolchain. src216/ is guest code, compiled by
# `krc --target=kr216`, and is deliberately not part of this build: the boundary is which
# compiler builds the tree (AGENTS.local.md, architecture boundaries).
set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckBoundary.cmake")
unset(SOURCE_ROOT)

# The guest tests need krc and krld. Asking for them is explicit, because a
# configure that finds nothing used to register no rows and still report success:
# with the option ON the toolchain is required and its absence stops the
# configure, and with it OFF no row is registered and the configure says so.
option(KR216_BUILD_GUEST_TESTS "Build the guest program tests, which need krc and krld" OFF)
set(KRC_EXECUTABLE "" CACHE FILEPATH "krc executable used by guest program tests (legacy alias)")
set(KRLD_EXECUTABLE "" CACHE FILEPATH "krld executable used by guest program tests (legacy alias)")
set(KRG_KRC "" CACHE FILEPATH "krc executable used by guest program tests")
set(KRG_KRLD "" CACHE FILEPATH "krld executable used by guest program tests")
set(KRG_LEGACY_KRC "${KRC_EXECUTABLE}")
set(KRG_LEGACY_KRLD "${KRLD_EXECUTABLE}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/KrGuest.cmake")
if(KR216_BUILD_GUEST_TESTS)
    krguest_find_toolchain()
else()
    message(STATUS "guest program tests are off: pass -DKR216_BUILD_GUEST_TESTS=ON to run them")
endif()

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)

# Guest code (src216/) is compiled by `krc --target=kr216`, never by this build. The target
# exists so the gap is explicit rather than silent: it names what is missing and fails,
# until the compiler has a kr216 backend (kr216-plan.md M2).
add_custom_target(guest
    COMMAND ${CMAKE_COMMAND} -E echo "guest: src216/ needs krc with --target=kr216, which this toolchain does not have yet (kr216-plan.md M2)"
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)

# Re-run the boundary check as a test too, so a cross-tree include fails even when a
# build directory was configured before the include was added.
add_test(NAME boundary
         COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckBoundary.cmake)
