# C++ skills

C++20 the way I write it (from [libutils](https://github.com/TsukiNi22/libutils)): a project from
[cpp_project_template](https://github.com/TsukiNi22/cpp_project_template), classes with empty bodies in my file
layout, the code style and comments, the `CMakeLists.txt`. Router: [`/cpp`](../routers/cpp/SKILL.md).

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`cmake-style`](cmake-style/SKILL.md) | `CMakeLists.txt` in my style: section order, explicit sources, Debug/Asan/Optimized modes, release targets, GTest tests, install + `find_package` config, CPack RPM/DEB stable/pre channels, with app/lib/tests templates. | [`cpp-class`](../cpp/cpp-class/SKILL.md), [`cpp-project`](../cpp/cpp-project/SKILL.md) |
| [`cpp-class`](cpp-class/SKILL.md) | Sets up a C++20 architecture / new `.hpp` & `.cpp` files (interface `I*`, abstract `A*`, class, template, `Type`/`Define`, family headers, `main.cpp` entry point) with the namespaces, the Xartania header and **empty bodies only** (never the logic), picks libutils attributes or standard `[[...]]` ones and registers the sources in the `CMakeLists.txt`. | [`cpp-style`](../cpp/cpp-style/SKILL.md), [`cpp-comments`](../cpp/cpp-comments/SKILL.md), [`libutils`](../libutils/libutils/SKILL.md) |
| [`cpp-comments`](cpp-comments/SKILL.md) | How I comment C++ code: section separators, `/* group */` labels, aligned trailing comments, no Doxygen. | [`cpp-class`](../cpp/cpp-class/SKILL.md) |
| [`cpp-project`](cpp-project/SKILL.md) | Sets up a new C++ project with the `setup.sh` of [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template) (never committing): renaming, binary / `.a` / `.so` / header-only, RPM/DEB packages of every variant, Linux + Windows (MinGW-w64) builds and packages, libutils (v4.1) or not, exceptions, GTest, the template's workflows (dispatch, unit tests, build, packages, GitHub release from the CHANGELOG, docs), optional gh-pages branch (signed packages repository + docs), README/CHANGELOG/docs, using the other skills. | [`cmake-style`](../cpp/cmake-style/SKILL.md), [`cpp-class`](../cpp/cpp-class/SKILL.md), [`libutils-exception`](../libutils/libutils-exception/SKILL.md), [`readme-style`](../docs/readme-style/SKILL.md), [`git-conventions`](../git/git-conventions/SKILL.md), [`html-doc`](../docs/html-doc/SKILL.md), [`ci-cd`](../git/ci-cd/SKILL.md) |
| [`cpp-style`](cpp-style/SKILL.md) | My C++ coding style: naming, indentation, braces, spacing, loops, switch, lambdas, const correctness, attributes. | [`cpp-class`](../cpp/cpp-class/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *create a `Timer` class in `utils::system` (interface + implementation)*
- *set up a new C++ project `netio`, shared library, with GTest and RPM / DEB packages*
- *rewrite `src/Parser.cpp` in my style*
- *add the install and `find_package` config to the CMakeLists.txt*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
