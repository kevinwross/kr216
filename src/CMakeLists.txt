add_library(kr216_core STATIC
    decode.cpp
    disasm.cpp
    encoding.cpp
    image.cpp
    machine.cpp
)
include(GNUInstallDirs)
target_include_directories(kr216_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# The core is the machine without the tools around it. Exporting it as
# `kr216::core` is what lets krdbg link one machine instead of compiling these
# sources into its adapter (D4 of the build cleanup plan).
add_library(kr216::core ALIAS kr216_core)
set_target_properties(kr216_core PROPERTIES EXPORT_NAME core)
target_compile_features(kr216_core PUBLIC cxx_std_20)
option(KR216_INSTALL "Install kr216::core and its headers for other trees" OFF)
if(KR216_INSTALL)
    install(TARGETS kr216_core EXPORT kr216Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT kr216Targets
        FILE kr216Targets.cmake
        NAMESPACE kr216::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kr216-core")
    include(CMakePackageConfigHelpers)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/kr216-core-config.cmake.in"
        "@PACKAGE_INIT@\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/kr216Targets.cmake\")\n")
    configure_package_config_file(
        "${CMAKE_CURRENT_BINARY_DIR}/kr216-core-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kr216-core-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kr216-core")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kr216-core-config.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kr216-core")
endif()
