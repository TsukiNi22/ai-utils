# libutils skills

My C++ library [libutils](https://github.com/TsukiNi22/libutils): its API reference (generated from a recorded
commit, with what changed since), its exceptions and codes, its install on the computer and its setup in a
project.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`libutils`](libutils/SKILL.md) | Reference of my library [libutils](https://github.com/TsukiNi22/libutils): every section, header and public API generated from a recorded version/commit (`reference/VERSION.md`), integration and conventions, plus `scripts/update.sh` to see what changed since that commit and regenerate. | — |
| [`libutils-exception`](libutils-exception/SKILL.md) | libutils exceptions: classes, codes, throw/catch patterns, and how to add new exception codes to a project (JSON, generator script, CMake block). | [`libutils`](../libutils/libutils/SKILL.md) |
| [`libutils-install`](libutils-install/SKILL.md) | Installs / updates / removes libutils on the computer by itself from the OS (dnf, apt, or build from the sources), the newest version (stable or `libutils-pre`, whichever is newer), debug/asan variants, mirror setup, `/usr/local` shadowing check, libutils for Windows (v4, MinGW-w64 cross compilation, no root). | [`libutils`](../libutils/libutils/SKILL.md) |
| [`libutils-setup`](libutils-setup/SKILL.md) | Adds libutils to the current project: `find_package`/`utils::utils` in the CMake and optionally the custom exceptions (JSON, scripts, generation block), installing libutils first if needed. | [`libutils-install`](../libutils/libutils-install/SKILL.md), [`libutils-exception`](../libutils/libutils-exception/SKILL.md), [`cmake-style`](../cpp/cmake-style/SKILL.md), [`cpp-class`](../cpp/cpp-class/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *which libutils class reads a file line by line?*
- *add a custom exception code for a bad configuration to this project*
- *install the newest libutils (stable or pre) with the debug variant*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
