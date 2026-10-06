---
name: cmake-style
description: Write or edit CMakeLists.txt in Tsukini's style (from libutils, context-forge, c2dmp-hsm) - fixed section order with '# =====' banners, TARGET variable, clang++ C++20, ccache, warnings, explicit SRC list with group comments, Debug/Asan/Optimized modes, release targets, exception header generation, GTest tests/, install + find_package config, CPack RPM/DEB with stable/pre channels, Makefile wrapper. Use whenever creating, editing or reviewing a CMakeLists.txt, a tests/CMakeLists.txt, a *Config.cmake.in or the CMake packaging of one of the user's projects.
---

# Tsukini CMake style

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

Templates (copy, then replace `<name>` = target/package name, `<NAME>` = upper-case option prefix,
`<x.y.z>` = version, `<summary>` = package description, `<utils-version>` = minimal libutils version):

| Project kind | Files |
|---|---|
| executable (context-forge) | `templates/app/CMakeLists.txt` |
| installable library (libutils) | `templates/lib/CMakeLists.txt` + `templates/lib/NAMEConfig.cmake.in` -> `cmake/package/<name>Config.cmake.in` |
| header-only library (c2dmp-hsm) | `templates/lib/CMakeLists.txt` with the `src/nothing.cpp` trick (see below) |
| unit tests (GTest) | `templates/tests/CMakeLists.txt` -> `tests/CMakeLists.txt` |
| helpers | `templates/Makefile` (root), `templates/check_files.sh` -> `cmake/scripts/` |

Full real examples: `~/personal_delivery/cpp/libutils/CMakeLists.txt` (library + multi-component packages),
`~/personal_delivery/cpp/context-forge/CMakeLists.txt` (executable + plugins `.so`).

## File layout (always this order, omit the sections that don't apply)
```cmake
cmake_minimum_required(VERSION 3.20)
set(TARGET <name>)
set(CMAKE_CXX_COMPILER clang++)
project(${TARGET} VERSION <x.y.z> LANGUAGES CXX)

# =========================
# Options
# =========================
```
`set(CMAKE_CXX_COMPILER clang++)` goes **before `project()`**: the compiler is detected by `project()`, set after
it is ignored (or re-runs the configuration). Sections, each introduced by the 3-line banner (`# ` + 25 `=`), one empty line before the next banner:
1. `Options` - `option(<NAME>_STABLE_RELEASE "Channel as stable release" OFF)`, `option(BUILD_TESTS "Build unit tests" OFF)`, project specific `<NAME>_*`.
2. `Compilateur & Standard` (exact title) - C++20 required, no extensions, PIC, output dir for executables, global definitions.
3. `Requirement` - `find_package(...)` with a trailing comment saying why, `pkg_check_modules(... IMPORTED_TARGET ...)`, then the ccache block.
4. `Warnings` - `add_compile_options(-W -Wall -Wextra -Wpedantic -Wunused-parameter -Wshadow -Wuninitialized)` one flag per line.
5. `Sources` - explicit `set(SRC ...)` then `add_executable` / `add_library` right after the list.
6. Extra targets when needed (`Plugins`, `Compilation parameters` for `set_target_properties`).
7. `Special Targets` - `tests` subdirectory, generated exception header, aggregate custom targets.
8. `Release (build ...)` - `release`, `install_release`, `package_release` custom targets that re-configure and build every mode.
9. `Dependencies` - include dirs, `target_link_libraries`, `add_dependencies` (a `foreach(target ${PLUGINS})` for the extra targets).
10. `Includes` / `Headers configuration` (libraries) - `BUILD_INTERFACE`/`INSTALL_INTERFACE`, `configure_file` of `version.hpp.in`.
11. `Modes` - per configuration compile/link options (and the `OUTPUT_NAME` suffix of a library).
12. `Installation` - `include(GNUInstallDirs)`, `install(...)` with `COMPONENT`, export + package config for a library.
13. `Utils` - `get_unregistered_files` target.
14. `Package Building` - CPack RPM & DEB with the stable/pre channels, `include(CPack)` last line.

## Formatting
- Commands in lower case, 4 spaces, `if(...)` / `else()` / `endif()` / `foreach(...)` / `endforeach()` without
  space and with empty `()` on the closers (the ccache `if (CCACHE_PROGRAM)` is the only legacy exception).
- One argument per line when a call has several arguments, closing `)` alone at column 0 (or at the
  indentation of the call): `add_compile_options(`, `set(SRC`, `add_custom_command(`, `install(`.
  Short calls stay on one line: `target_link_libraries(${TARGET} PRIVATE utils::utils)`.
- Keywords grouped as in the examples: `OUTPUT`, `COMMAND`, `WORKING_DIRECTORY`, `DEPENDS`, `COMMENT`, `VERBATIM`;
  `set_target_properties(` then targets, `PROPERTIES` and the indented key/values.
- Variables `UPPER_SNAKE` (`SRC`, `SRC_<GROUP>`, `PLUGINS`, `CHANNEL`, `MIRROR_CHANNEL`), options prefixed by the
  project in upper case (`UTILS_`, `CONTEXT_FORGE_`). Always `${TARGET}`, never the raw name, for the main target.
- Paths from `${CMAKE_SOURCE_DIR}`; sources relative to the root (`src/<project>/X.cpp`).
- Comments: `# Comment` trailing or above, short English (typos aside), commented alternatives kept
  (`#add_library(${TARGET} SHARED ${SRC})`), `# =========================` banners only for sections.
- One empty line between blocks inside a section, none after the opening banner.

## Rules
- **Sources are listed explicitly** in `set(SRC ...)`, grouped by comments: `# Group` or `## Group`,
  `## Group (sub-part: x)` for a sub-folder, one empty line between groups. Never `file(GLOB ...)` for the
  sources (only for configs, with `CONFIGURE_DEPENDS`); the `get_unregistered_files` target lists the
  `.cpp` missing from `SRC`. To add a file: `python3 <cpp-class>/scripts/cmake_add.py CMakeLists.txt src/<path>/X.cpp`.
- Build types: `Debug` (`-g -ggdb3`), `Asan` (`-fsanitize=address -fno-omit-frame-pointer -g -ggdb3` + link option),
  `Optimized` (`-O3 -ffast-math -funroll-loops -pipe -DNDEBUG`, `-march=native` only for a binary not distributed,
  `-fno-plt` for a library), all through generator expressions `$<$<CONFIG:Debug>:...>`. No `Release` type:
  the `release` custom target builds the modes.
- Library output names per mode: `OUTPUT_NAME "<name>$<$<CONFIG:Debug>:_debug>$<$<CONFIG:Asan>:_asan>"`.
- Executables are written at the root: `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination`.
- Exception codes: JSON files in `cmake/config/exceptions/`, generated header with `add_custom_command` +
  `add_custom_target(generated_<internal|external>_exception_header)` + `add_dependencies(${TARGET} ...)`
  (internal = libutils itself, external = a project using libutils).
- libutils dependency: `find_package(utils <min version> CONFIG REQUIRED) # Check utils dependencies existance`
  + `utils::utils`.
- Tests: `if(BUILD_TESTS)` -> `enable_testing()` + `add_subdirectory(tests)`; `tests/CMakeLists.txt` builds
  `unit_tests` (GTest, `gtest_discover_tests`), reuses the parent `SRC` without `main.cpp`.
- Header-only library: `src/nothing.cpp` in `SRC` to keep a target, a `message(WARNING ...)` explaining that only
  `--target install` is meaningful, and `target_include_directories(${TARGET} PUBLIC include)`.
- Library install: `install(TARGETS ... EXPORT ${TARGET}Targets ...)`, headers `install(DIRECTORY include/ ... PATTERN "*.in" EXCLUDE)`,
  `install(EXPORT ... NAMESPACE ${TARGET}:: ...)`, `configure_package_config_file` + `write_basic_package_version_file`
  (`SameMajorVersion`) so that `find_package(<name> <version> CONFIG REQUIRED)` works.
- Packages: `CPACK_GENERATOR "RPM;DEB"`, vendor/maintainer `TsukiNi22`, contact `xartania.contact@gmail.com`, MIT,
  channel `-pre` unless `<NAME>_STABLE_RELEASE`, the stable package obsoletes/replaces the `-pre` one and both
  conflict, release number `1` for stable / `${PACKAGE_RELEASE}` (CI run number) / `0`.
- New target / output written in the sources (executable at the root, plugin dir, generated file): update the
  `.gitignore` with `python3 <cpp-project>/scripts/update_gitignore.py <root>`.
- Root `Makefile` wrapper (`all`, `clean`, `fclean`, `re`) calling CMake with `BUILD_DIR := build`.

## Checking (xstyle)
When `xstyle` is installed (`command -v xstyle`), check what was written or reviewed with it instead of re-reading the
rules by hand: `xstyle --rtk <files|dirs> [-r]` (**always `--rtk`**: compact output made for the assistant),
`xstyle --rtk --fix [CODES] <paths>` for the fixable ones (`-n` to preview), `-c CMAKE,G` for the rules of this
skill, `xstyle -x CODE` to explain one. Not installed: apply the rules by hand and say once that
`./setup.sh install xstyle` would check them. Its findings are heuristic: the local style of a file wins.
