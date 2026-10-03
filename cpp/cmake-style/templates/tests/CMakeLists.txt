set(TARGET unit_tests)

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination

# =========================
# Requirement
# =========================
find_package(GTest REQUIRED)

# =========================
# Sources
# =========================
# Project sources (executable: reuse the parent list without the entry point)
# Library: remove these 3 lines and link the library target instead
set(SRC_PROJECT ${SRC})
list(REMOVE_ITEM SRC_PROJECT src/<name>/main.cpp)
list(TRANSFORM SRC_PROJECT PREPEND ${CMAKE_SOURCE_DIR}/)

set(SRC
    # Core
    core/Core.cpp

    # Core (sub-part: x)
    core/x/X.cpp
)
add_executable(${TARGET} ${SRC} ${SRC_PROJECT})

# =========================
# Dependencies
# =========================
target_include_directories(${TARGET} PRIVATE ../include/ .)
target_link_libraries(${TARGET} PRIVATE GTest::gtest GTest::gtest_main)
#add_dependencies(${TARGET} generated_external_exception_header) # if the project generates its exception header

# =========================
# Modes
# =========================
target_compile_options(${TARGET} PRIVATE -g -ggdb3)

include(GoogleTest)
gtest_discover_tests(${TARGET})
