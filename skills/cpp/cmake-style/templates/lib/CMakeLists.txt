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
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Moderne compilation -fPIE

# =========================
# Requirement
# =========================
find_package(PkgConfig REQUIRED)
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
    ## Core
    src/<name>/Core.cpp

    ## Core (sub-part: x)
    src/<name>/x/X.cpp
)
add_library(${TARGET} STATIC ${SRC})
#add_library(${TARGET} SHARED ${SRC})

# =========================
# Special Targets
# =========================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =========================
# Release (build Debug & Optimized)
# =========================
add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Asan
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel
    COMMENT "Building both debug versions Debug & Asan (lib<name>_debug.a, lib<name>_asan.a) and Optimized (lib<name>.a) one"
    VERBATIM
)
add_custom_target(install_release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Asan
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel
    COMMENT "Installing both debug versions Debug & Asan and Optimized one"
    VERBATIM
)
add_custom_target(package_release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel
    COMMENT "Building package release version"
    VERBATIM
)

# =========================
# Includes
# =========================
target_include_directories(${TARGET} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# =========================
# Modes
# =========================
set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "<name>$<$<CONFIG:Debug>:_debug>$<$<CONFIG:Asan>:_asan>"
)

target_compile_options(${TARGET} PRIVATE
    $<$<CONFIG:Debug>:-g -ggdb3>
    $<$<CONFIG:Asan>:-fsanitize=address -fno-omit-frame-pointer -g -ggdb3>
    $<$<CONFIG:Optimized>:-O3 -ffast-math -funroll-loops -fno-plt -pipe -DNDEBUG>
)
target_link_options(${TARGET} PRIVATE
    $<$<CONFIG:Asan>:-fsanitize=address>
)

# =========================
# Installation
# =========================
include(GNUInstallDirs) # load variable used for installation
include(CMakePackageConfigHelpers)

# licenses
install(
    FILES ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${TARGET}
    COMPONENT lib
)

# library
install(TARGETS ${TARGET}
    EXPORT ${TARGET}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT lib
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# header
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT lib
    PATTERN "*.in" EXCLUDE
)

# CMake setup for find_package(<name> CONFIG REQUIRED)
install(EXPORT ${TARGET}Targets
    FILE ${TARGET}Targets.cmake
    NAMESPACE ${TARGET}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
    COMPONENT lib
)

set(CONFIG_INSTALL_DIR
    ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/package/${TARGET}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}Config.cmake
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}ConfigVersion.cmake
    DESTINATION ${CONFIG_INSTALL_DIR}
    COMPONENT lib
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
set(CPACK_PACKAGE_NAME "lib<name>${CHANNEL}")
set(MIRROR_PACKAGE_NAME "lib<name>${MIRROR_CHANNEL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "<summary>")

# Setup the conflict between stable & unstable packages
if(<NAME>_STABLE_RELEASE)
    # If a recent stable version exist the lib<name>-pre installation will result in the stable version
    set(CPACK_RPM_PACKAGE_OBSOLETES "${MIRROR_PACKAGE_NAME} <= ${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_REPLACES "${MIRROR_PACKAGE_NAME} (<= ${PROJECT_VERSION})")
endif()

set(CPACK_RPM_PACKAGE_CONFLICTS "${MIRROR_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_BREAKS "${MIRROR_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "${MIRROR_PACKAGE_NAME}")

# One package (library + headers + cmake config)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL lib)

# Edit release value (run count)
if(<NAME>_STABLE_RELEASE)
    # A stable version should never have to be rebuild and should always be removed or marked as deprecated
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

# Debian
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TsukiNi22")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/TsukiNi22/<name>")
set(CPACK_DEBIAN_LIB_PACKAGE_NAME ${CPACK_PACKAGE_NAME})

include(CPack)
