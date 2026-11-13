cmake_minimum_required(VERSION 3.20)
set(TARGET <name>)
set(CMAKE_CXX_COMPILER clang++)
project(${TARGET} VERSION <x.y.z> LANGUAGES CXX)

# =========================
# Options
# =========================
option(<NAME>_STABLE_RELEASE "Channel as stable release" OFF)
option(BUILD_TESTS "Build unit tests" OFF)

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Moderne compilation -fPIE
add_compile_definitions(BACKWARD_COMPATIBILITY_WARNING) # To check internal usage for potential depreacted function

# =========================
# Requirement
# =========================
find_package(utils <utils-version> CONFIG REQUIRED) # Check utils dependencies existance
find_package(Python3 REQUIRED) # Used for the special dependencie
find_package(PkgConfig REQUIRED)
#pkg_check_modules(<LIB> REQUIRED IMPORTED_TARGET <lib>) # Used for ...
find_program(CCACHE_PROGRAM ccache) # Used for cache during compilation
if (CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# =========================
# Warnings
# =========================
add_compile_options(
    -W
    -Wall
    -Wextra
    -Wpedantic
    -Wunused-parameter
    -Wshadow
    -Wuninitialized
)

# =========================
# Sources
# =========================
set(SRC
    # Entry
    src/<name>/main.cpp

    # Core
    src/<name>/Core.cpp

    # Core (sub-part: x)
    src/<name>/x/X.cpp
)
add_executable(${TARGET} ${SRC})

# =========================
# Special Targets
# =========================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

file(GLOB_RECURSE EXCEPTION_CONFIG_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cmake/config/exceptions/*.json"
)
set(GENERATED_EXCEPTION_HEADER
    "${CMAKE_SOURCE_DIR}/include/exception/generated_external_exception_header.hpp"
)
add_custom_command(
    OUTPUT ${GENERATED_EXCEPTION_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXCEPTION_CONFIG_FILES} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    COMMENT "Generating exception header (config changed)"
    VERBATIM
)
add_custom_target(generated_external_exception_header
    DEPENDS ${GENERATED_EXCEPTION_HEADER}
)

# =========================
# Release (build Optimized)
# =========================
add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel
    COMMENT "Release version: Optimized"
    VERBATIM
)
add_custom_target(package_release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel
    COMMENT "Building package release version"
    VERBATIM
)

# =========================
# Dependencies
# =========================
target_include_directories(${TARGET} PRIVATE include include/exception)
target_link_libraries(${TARGET} PRIVATE utils::utils)
add_dependencies(${TARGET} generated_external_exception_header)

# =========================
# Modes
# =========================
target_compile_options(${TARGET} PRIVATE
    $<$<CONFIG:Debug>:-g -ggdb3>
    $<$<CONFIG:Asan>:-fsanitize=address -fno-omit-frame-pointer -g -ggdb3>
    $<$<CONFIG:Optimized>:-O3 -ffast-math -march=native -funroll-loops -pipe -DNDEBUG>
)
target_link_options(${TARGET} PRIVATE
    $<$<CONFIG:Asan>:-fsanitize=address>
)

# =========================
# Installation
# =========================
include(GNUInstallDirs) # load variable used for installation

# Binary
install(TARGETS ${TARGET}
    EXPORT ${TARGET}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
)

# =========================
# Utils
# =========================
add_custom_target(get_unregistered_files
    COMMAND bash ${CMAKE_SOURCE_DIR}/cmake/scripts/check_files.sh unregistered ${SRC}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# =========================
# Package Building
# =========================
set(CPACK_PACKAGE_VENDOR "TsukiNi22")
set(CPACK_PACKAGE_CONTACT "xartania.contact@gmail.com")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_GENERATOR "RPM;DEB")

# Setup the channel name
if(<NAME>_STABLE_RELEASE)
    set(CHANNEL "")
    set(MIRROR_CHANNEL "-pre")
else()
    set(CHANNEL "-pre")
    set(MIRROR_CHANNEL "")
endif()

# Setup the package name
set(CPACK_PACKAGE_NAME "<name>${CHANNEL}")
set(MIRROR_PACKAGE_NAME "<name>${MIRROR_CHANNEL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "<summary>")

# Setup the conflict between stable & unstable packages
if(<NAME>_STABLE_RELEASE)
    # If a recent stable version exist the <name>-pre installation will result in the stable version
    set(CPACK_RPM_PACKAGE_OBSOLETES "${MIRROR_PACKAGE_NAME} <= ${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_REPLACES "${MIRROR_PACKAGE_NAME} (<= ${PROJECT_VERSION})")
endif()

set(CPACK_RPM_PACKAGE_CONFLICTS "${MIRROR_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_BREAKS "${MIRROR_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "${MIRROR_PACKAGE_NAME}")

# Separate headers & lib
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL "")
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF) # In case~

# Edit release value (run count)
if(<NAME>_STABLE_RELEASE)
    # A stable version should never have to be rebuild and should always be removed or marked as depreacted
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
elseif(DEFINED PACKAGE_RELEASE)
    set(CPACK_RPM_PACKAGE_RELEASE "${PACKAGE_RELEASE}")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "${PACKAGE_RELEASE}")
else() # 0 is impossible for github.run_number
    set(CPACK_RPM_PACKAGE_RELEASE "0")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
endif()

# Dependencies
set(CPACK_RPM_PACKAGE_REQUIRES "")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_LIB_PACKAGE_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_RPM_HEADERS_PACKAGE_NAME ${CPACK_PACKAGE_NAME})

# Debian
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TsukiNi22")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/TsukiNi22/<name>")
set(CPACK_DEBIAN_LIB_PACKAGE_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_DEBIAN_HEADERS_PACKAGE_NAME ${CPACK_PACKAGE_NAME})

include(CPack)
