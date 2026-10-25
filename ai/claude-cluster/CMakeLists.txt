cmake_minimum_required(VERSION 3.20)
set(TARGET claude-cluster)
set(CMAKE_CXX_COMPILER clang++)
project(${TARGET} VERSION 1.4.121 LANGUAGES CXX)

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Moderne compilation -fPIE
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Optimized) # Installed by setup.sh: optimized by default
endif()

# =========================
# Requirement
# =========================
find_package(utils 4.0.0 CONFIG REQUIRED) # Check utils dependencies existance
find_package(nlohmann_json 3.10 CONFIG REQUIRED) # stream-json of claude, MCP, state files
find_package(tomlplusplus 3.0 CONFIG REQUIRED) # config.toml
find_package(Threads REQUIRED)
find_package(OpenSSL QUIET) # https of the web access (the microphone of the page needs it), optional
option(CLUSTER_GUI "Build the Qt6 window front-end (when Qt6 Widgets is found)" ON)
if(CLUSTER_GUI)
    find_package(Qt6 6.2 COMPONENTS Widgets QUIET) # window front-end, optional
endif()
find_program(CCACHE_PROGRAM ccache) # Used for cache during compilation
if (CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Memory of the build: at most BUILD_JOBS compilations at once (a clang of these sources: up to ~0.8 GB), from BUILD_MEMORY_MB
# (default 4 GB, at most half the memory of the computer); any -j goes through the gate (a make -j alone starts them all)
set(BUILD_MEMORY_MB 4096 CACHE STRING "Memory the compilations may take at once (MB)")
set(BUILD_JOBS "" CACHE STRING "Compilations at once (empty: from BUILD_MEMORY_MB)")
cmake_host_system_information(RESULT TOTAL_MEMORY_MB QUERY TOTAL_PHYSICAL_MEMORY)
math(EXPR HALF_MEMORY_MB "${TOTAL_MEMORY_MB} / 2")
set(GATE_MEMORY_MB ${BUILD_MEMORY_MB})
if (GATE_MEMORY_MB GREATER HALF_MEMORY_MB)
    set(GATE_MEMORY_MB ${HALF_MEMORY_MB})
endif()
math(EXPR GATE_JOBS "${GATE_MEMORY_MB} / 800")
if (BUILD_JOBS)
    set(GATE_JOBS ${BUILD_JOBS})
elseif (GATE_JOBS LESS 1)
    set(GATE_JOBS 1)
endif()
find_program(FLOCK_PROGRAM flock)
if (FLOCK_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_CURRENT_SOURCE_DIR}/cmake/jobs-gate.sh ${GATE_JOBS} ${CMAKE_BINARY_DIR}/jobs-gate ${CCACHE_PROGRAM})
    message(STATUS "claude-cluster: ${GATE_JOBS} compilations at once at most (BUILD_MEMORY_MB=${BUILD_MEMORY_MB}, BUILD_JOBS to force)")
endif()

# FTXUI (terminal front-end): not packaged on Fedora, fetched at a fixed version
include(FetchContent)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(ftxui)

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
    src/cluster/main.cpp

    # Core
    src/cluster/Core.cpp
    src/cluster/Core-Completion.cpp
    src/cluster/Config.cpp
    src/cluster/Auth.cpp
    src/cluster/Tools.cpp
    src/cluster/Docs.cpp

    # Core (sub-part: sessions)
    src/cluster/Session.cpp
    src/cluster/Manager.cpp
    src/cluster/Manager-Tasks.cpp
    src/cluster/Manager-Schedules.cpp
    src/cluster/Cron.cpp
    src/cluster/Manager-Push.cpp
    src/cluster/Manager-Jobs.cpp
    src/cluster/Orchestrator.cpp
    src/cluster/Orchestrator-Control.cpp
    src/cluster/Orchestrator-Settings.cpp
    src/cluster/Hardware.cpp
    src/cluster/RunProfile.cpp
    src/cluster/Manager-Usage.cpp
    src/cluster/Manager-Describe.cpp
    src/cluster/Taxonomy.cpp
    src/cluster/Manager-Taxonomy.cpp
    src/cluster/Manager-Method.cpp
    src/cluster/Manager-Groups.cpp
    src/cluster/Manager-Replica.cpp
    src/cluster/Prompt.cpp
    src/cluster/PromptNav.cpp
    src/cluster/PromptStash.cpp
    src/cluster/Rewind.cpp
    src/cluster/Match.cpp
    src/cluster/Origin.cpp
    src/cluster/Requirements.cpp
    src/cluster/Progress.cpp
    src/cluster/Services.cpp
    src/cluster/Showcase.cpp
    src/cluster/Demo.cpp
    src/cluster/Simple.cpp
    src/cluster/StartMenu.cpp
    src/cluster/Stats.cpp
    src/cluster/Plugins.cpp
    src/cluster/Policy.cpp
    src/cluster/Method.cpp
    src/cluster/Manager-Policy.cpp
    src/cluster/Manager-Upgrade.cpp
    src/cluster/Usage.cpp
    src/cluster/WebTty.cpp
    src/cluster/Vt.cpp
    src/cluster/Manager-External.cpp
    src/cluster/Asking.cpp
    src/cluster/Display.cpp
    src/cluster/Attachments.cpp
    src/cluster/Icons.cpp
    src/cluster/External.cpp
    src/cluster/Store.cpp
    src/cluster/Git.cpp
    src/cluster/Models.cpp
    src/cluster/Voice.cpp
    src/cluster/Vocabulary.cpp
    src/cluster/VoiceEngines.cpp
    src/cluster/Web.cpp
    src/cluster/WebVoice.cpp

    # Core (sub-part: control)
    src/cluster/Control.cpp
    src/cluster/Mcp.cpp
    src/cluster/Actions.cpp
    src/cluster/Settings.cpp
    src/cluster/Editor.cpp
    src/cluster/Reveal.cpp
    src/cluster/Service.cpp
    src/cluster/Machine.cpp
    src/cluster/Worker.cpp

    # Front-ends
    src/cluster/ui/Tty.cpp
    src/cluster/ui/LineEditor.cpp
    src/cluster/ui/TtyGraph.cpp
    src/cluster/ui/Popup.cpp
    src/cluster/ui/RewindChooser.cpp
    src/cluster/ui/StashPopup.cpp
)
set(CLUSTER_WITH_GUI OFF)
if(CLUSTER_GUI AND Qt6Widgets_FOUND)
    set(CLUSTER_WITH_GUI ON)
    list(APPEND SRC src/cluster/ui/Gui.cpp)
endif()
add_executable(${TARGET} ${SRC})

# cluster-run: the wrapper of a heavy command (resource orchestrator), a small binary of its own installed next to claude-cluster
add_executable(cluster-run src/run/cluster-run.cpp src/cluster/RunProfile.cpp)
target_include_directories(cluster-run PRIVATE include)
target_link_libraries(cluster-run PRIVATE nlohmann_json::nlohmann_json)

# =========================
# Special Targets
# =========================
# Default system prompts (global session, sub-sessions) and default config, embedded (written once in the config folder)
set(TEMPLATES_HEADER "// Generated by the CMakeLists.txt of claude-cluster from templates/, do not edit\n#pragma once\n#include <string_view>\n#include <string>\nnamespace cluster::templates {\n")
foreach(NAME_FILE "GLOBAL_PROMPT=global.md" "SESSIONS_PROMPT=sessions.md" "RENDERING=rendering.md" "ROUTING=routing.md" "VOCABULARY=vocabulary.txt" "VOICE_WORDS=voice-words.txt" "CONFIG=config.toml" "WAKE_SCRIPT=wake.py" "WEB_PAGE=web.html" "TTY_PAGE=tty.html")
    string(REPLACE "=" ";" PARTS ${NAME_FILE})
    list(GET PARTS 0 VAR_NAME)
    list(GET PARTS 1 FILE_NAME)
    file(READ "${CMAKE_SOURCE_DIR}/templates/${FILE_NAME}" CONTENT)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/templates/${FILE_NAME}")
    string(LENGTH "${CONTENT}" CONTENT_LENGTH)
    if(CONTENT_LENGTH LESS 60000)
        string(APPEND TEMPLATES_HEADER "inline constexpr std::string_view ${VAR_NAME} = R\"XTPL(${CONTENT})XTPL\";\n")
    else()
        # longer than a literal may be (64 KB): its parts joined once at the start
        string(APPEND TEMPLATES_HEADER "inline const std::string ${VAR_NAME} = std::string()")
        foreach(START RANGE 0 ${CONTENT_LENGTH} 30000)
            string(SUBSTRING "${CONTENT}" ${START} 30000 PART)
            string(APPEND TEMPLATES_HEADER "\n    + R\"XTPL(${PART})XTPL\"")
        endforeach()
        string(APPEND TEMPLATES_HEADER ";\n")
    endif()
endforeach()
string(APPEND TEMPLATES_HEADER "}\n")
file(WRITE ${CMAKE_BINARY_DIR}/generated/cluster/Templates.hpp "${TEMPLATES_HEADER}")

# The documentation site (docs/site: Markdown EN + FR, the html-style scripts of the repository): generated by python3 into DocsData.cpp (the files as byte
# arrays), embedded, served by the core at /docs and opened by the interfaces. Optional: without python3 3.11+, docs/site or the html-style scripts, an
# empty table is built (the program says so where the docs would open). The commit shown is the one of this configure.
set(DOCS_CPP ${CMAKE_BINARY_DIR}/generated/cluster/DocsData.cpp)
set(DOCS_SITE ${CMAKE_SOURCE_DIR}/docs/site)
set(DOCS_HTML_STYLE ${CMAKE_SOURCE_DIR}/../../docs/html-style/scripts/new_page.py)
find_package(Python3 3.11 QUIET COMPONENTS Interpreter)
execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD OUTPUT_VARIABLE CLUSTER_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT CLUSTER_COMMIT)
    set(CLUSTER_COMMIT "unknown")
endif()
# The release notes: the subjects of the last commits of claude-cluster ("type(claude-cluster): version - what"), embedded; an older core reads them from
# this binary (--notes) to tell what it brings in its update popup
execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} log -n 120 --format=%s -- . OUTPUT_VARIABLE CLUSTER_NOTES OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(REPLACE ")XNOTES" ") XNOTES" CLUSTER_NOTES "${CLUSTER_NOTES}")
set(NOTES_HEADER "// Generated by CMake: the subjects of the commits of claude-cluster (its release notes)\ninline const char* const CLUSTER_NOTES = \"\"")
string(LENGTH "${CLUSTER_NOTES}" NOTES_LENGTH)
foreach(START RANGE 0 ${NOTES_LENGTH} 30000)
    string(SUBSTRING "${CLUSTER_NOTES}" ${START} 30000 PART)
    string(APPEND NOTES_HEADER "\n    R\"XNOTES(${PART})XNOTES\"")
endforeach()
string(APPEND NOTES_HEADER ";\n")
file(WRITE ${CMAKE_BINARY_DIR}/generated/cluster/Notes.hpp "${NOTES_HEADER}")
if(Python3_FOUND AND EXISTS ${DOCS_SITE}/build.py AND EXISTS ${DOCS_HTML_STYLE})
    file(GLOB_RECURSE DOCS_INPUTS CONFIGURE_DEPENDS ${DOCS_SITE}/*.md ${DOCS_SITE}/*.py ${DOCS_SITE}/*.toml ${DOCS_SITE}/src/assets/* ${DOCS_SITE}/downloads/*)
    add_custom_command(
        OUTPUT ${DOCS_CPP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated/cluster
        COMMAND ${Python3_EXECUTABLE} ${DOCS_SITE}/build.py --out ${CMAKE_BINARY_DIR}/docs-site --embed --cpp ${DOCS_CPP}
            --version ${PROJECT_VERSION} --commit ${CLUSTER_COMMIT} --code-root ${CMAKE_SOURCE_DIR}
        DEPENDS ${DOCS_INPUTS} ${DOCS_HTML_STYLE} ${CMAKE_SOURCE_DIR}/templates/config.toml ${CMAKE_SOURCE_DIR}/src/cluster/Mcp.cpp
            ${CMAKE_SOURCE_DIR}/src/cluster/Settings.cpp ${CMAKE_SOURCE_DIR}/src/cluster/Orchestrator-Control.cpp ${CMAKE_SOURCE_DIR}/src/cluster/Orchestrator-Settings.cpp
        COMMENT "claude-cluster: the documentation site (python3)"
        VERBATIM
    )
    message(STATUS "claude-cluster: documentation site embedded (python3 ${Python3_VERSION})")
else()
    file(WRITE ${DOCS_CPP} "#include \"cluster/Docs.hpp\"\nconst cluster::DocEntry cluster::docs_table[] = {{nullptr, nullptr, nullptr, 0}};\nconst std::size_t cluster::docs_count = 0;\n")
    message(STATUS "claude-cluster: documentation site not built (python3 3.11+, docs/site or docs/html-style missing)")
endif()
target_sources(${TARGET} PRIVATE ${DOCS_CPP})
target_compile_definitions(${TARGET} PRIVATE CLUSTER_COMMIT="${CLUSTER_COMMIT}")

# =========================
# Dependencies
# =========================
target_include_directories(${TARGET} PRIVATE include ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(${TARGET} PRIVATE CLUSTER_VERSION="${PROJECT_VERSION}")
if(OpenSSL_FOUND)
    target_link_libraries(${TARGET} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(${TARGET} PRIVATE CLUSTER_TLS)
endif()
target_link_libraries(${TARGET} PRIVATE
    utils::utils
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    Threads::Threads
    util
    ftxui::screen
    ftxui::dom
    ftxui::component
)
if(CLUSTER_WITH_GUI)
    target_compile_definitions(${TARGET} PRIVATE CLUSTER_GUI=1)
    target_link_libraries(${TARGET} PRIVATE Qt6::Widgets)
    message(STATUS "claude-cluster: window front-end (Qt6) enabled")
else()
    message(STATUS "claude-cluster: window front-end disabled (Qt6 Widgets not found or CLUSTER_GUI=OFF)")
endif()

# =========================
# Modes
# =========================
target_compile_options(${TARGET} PRIVATE
    $<$<CONFIG:Debug>:-g -ggdb3>
    $<$<CONFIG:Asan>:-fsanitize=address,undefined -fno-omit-frame-pointer -g -ggdb3>
    $<$<CONFIG:Optimized>:-O2 -pipe -DNDEBUG>
)
target_link_options(${TARGET} PRIVATE
    $<$<CONFIG:Asan>:-fsanitize=address,undefined>
)
target_compile_options(cluster-run PRIVATE
    $<$<CONFIG:Debug>:-g -ggdb3>
    $<$<CONFIG:Optimized>:-O2 -pipe -DNDEBUG>
)

# =========================
# Tests
# =========================
# The unit test of the line editor (no framework, exit code 0 = ok): cmake -DCLUSTER_TESTS=ON, then the target line_editor_test
option(CLUSTER_TESTS "Build the unit tests (tests/*.cpp)" OFF)
if(CLUSTER_TESTS)
    set(TEST_SRC ${SRC})
    list(FILTER TEST_SRC EXCLUDE REGEX "/(main|Tty|Gui|Docs)\\.cpp$")
    add_executable(line_editor_test tests/line_editor.cpp ${TEST_SRC})
    target_include_directories(line_editor_test PRIVATE include ${CMAKE_BINARY_DIR}/generated)
    target_compile_definitions(line_editor_test PRIVATE CLUSTER_VERSION="${PROJECT_VERSION}")
    target_link_libraries(line_editor_test PRIVATE utils::utils nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus Threads::Threads util ftxui::screen ftxui::dom ftxui::component)
    if(OpenSSL_FOUND)
        target_link_libraries(line_editor_test PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(line_editor_test PRIVATE CLUSTER_TLS)
    endif()
    # the files of the front-ends that main() starts are not linked: what only they define is never called by this test
    target_link_options(line_editor_test PRIVATE -Wl,--unresolved-symbols=ignore-all)
endif()


# =========================
# Installation
# =========================
include(GNUInstallDirs) # load variable used for installation

# Binary
install(TARGETS ${TARGET} cluster-run
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
)
