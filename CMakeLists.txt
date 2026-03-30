cmake_minimum_required(VERSION 3.10)

project(linux-wire
    VERSION 0.1.1
    LANGUAGES C CXX
)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Options
option(LINUX_WIRE_BUILD_EXAMPLES "Build example programs" ON)

# Use modern standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library sources
add_library(linux_wire
    src/linux_wire.c
    src/Wire.cpp
)

target_include_directories(linux_wire
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# On Linux we need these headers; no extra link libraries usually required.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "linux-wire is intended for Linux systems with /dev/i2c-* and linux/i2c-dev.h")
endif()

# Examples
if(LINUX_WIRE_BUILD_EXAMPLES)
    # C++ example binaries (original)
    add_executable(i2c_scanner_cpp examples/cpp/i2c_scanner/main.cpp)
    target_link_libraries(i2c_scanner_cpp PRIVATE linux_wire)

    add_executable(i2c_scanner_strict_cpp examples/cpp/i2c_scanner_strict/main.cpp)
    target_link_libraries(i2c_scanner_strict_cpp PRIVATE linux_wire)

    add_executable(master_multiplier_cpp examples/cpp/master_multiplier/main.cpp)
    target_link_libraries(master_multiplier_cpp PRIVATE linux_wire)

    add_executable(master_writer_cpp examples/cpp/master_writer/main.cpp)
    target_link_libraries(master_writer_cpp PRIVATE linux_wire)

    add_executable(master_reader_cpp examples/cpp/master_reader/main.cpp)
    target_link_libraries(master_reader_cpp PRIVATE linux_wire)

    # C example binaries (new) — mirrors the C++ examples but use the C API
    add_executable(i2c_scanner_c examples/c/i2c_scanner/main.c)
    target_link_libraries(i2c_scanner_c PRIVATE linux_wire)

    add_executable(i2c_scanner_strict_c examples/c/i2c_scanner_strict/main.c)
    target_link_libraries(i2c_scanner_strict_c PRIVATE linux_wire)

    add_executable(master_multiplier_c examples/c/master_multiplier/main.c)
    target_link_libraries(master_multiplier_c PRIVATE linux_wire)

    add_executable(master_writer_c examples/c/master_writer/main.c)
    target_link_libraries(master_writer_c PRIVATE linux_wire)

    add_executable(master_reader_c examples/c/master_reader/main.c)
    target_link_libraries(master_reader_c PRIVATE linux_wire)
endif()

install(TARGETS linux_wire
    EXPORT linux_wireTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT linux_wireTargets
    FILE linux_wireTargets.cmake
    NAMESPACE linux_wire::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux_wire
)

configure_package_config_file(
    cmake/linux_wireConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/linux_wireConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux_wire
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/linux_wireConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/linux_wireConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/linux_wireConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux_wire
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
