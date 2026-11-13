# --- Requirement section ---
find_package(Python3 REQUIRED) # Used for the special dependencie

# --- Special Targets section ---
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

# --- Dependencies section (for every target using libutils: main target, plugins, unit_tests) ---
target_include_directories(${TARGET} PRIVATE include include/exception)
target_link_libraries(${TARGET} PRIVATE utils::utils)
add_dependencies(${TARGET} generated_external_exception_header)
