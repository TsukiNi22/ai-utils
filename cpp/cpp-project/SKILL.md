---
name: cpp-project
description: Set up a new C++20 project in Tsukini's way with the setup.sh of the cpp_project_template repository (TsukiNi22/cpp_project_template) - name / core class renaming, kind of project (binary, static .a or shared .so library, header-only; RPM/DEB packages of every variant, Windows MinGW-w64 build and packages), libutils v4 or not, exception codes, GTest tests, the CI/CD workflows of the template (dispatch, unit tests, build, packages, GitHub release from the CHANGELOG, docs) and the optional gh-pages branch (signed packages repository + docs), install script, README/CHANGELOG/docs written by the template - using the other skills (cmake-style, cpp-class, libutils-exception, readme-style, git-conventions, ci-cd, html-doc). Use whenever the user wants to create, bootstrap, initialize or scaffold a new C++ project or repository.
---

# New C++ project

The base is always **`cpp_project_template`** and **its own `setup.sh`** (local clone
`~/personal_delivery/cpp/cpp_project_template`, otherwise `https://github.com/TsukiNi22/cpp_project_template.git`): it
renames, adapts the copy to the kind, writes the README, the CHANGELOG, the docs and the gh-pages branch. This skill
drives it without git actions, then completes what it does not do (libutils freshness, no-libutils, `.gitignore`, the
custom CI/CD, the checks). Skill synced with the template at `d65a146` (2026-10-17: Windows build, one CMake for
every kind, `SRC` / `SRC_SHARED`, workflows dispatch / unit-tests / build / build-packages / release / docs, gh-pages,
`install.sh`, CHANGELOG written by `setup.sh`, libutils `>= 4.1.0`): when its `HEAD` is newer
(`git -C <clone> log --oneline d65a146..HEAD`, and `origin/main` after a `git fetch`), read its README (*Quick Setup*,
*Generated project*, *Workflows/Release*) and its `setup.sh --help` before relying on the steps below.
The other skills, each loaded when its step comes: `cmake-style` (CMake), `cpp-class` (classes, header question, main),
`libutils-exception` (codes), `readme-style`, `git-conventions` (CHANGELOG, commits, branches), `ci-cd`, `html-doc`.
`SKILL_DIR` = the directory of this file.

## libutils freshness
First, once per conversation: the freshness check of the `libutils` skill (`bash <libutils skill>/scripts/check.sh`,
section "Freshness check" of `libutils/SKILL.md`). Outdated -> reuse the answer already given in this conversation,
else ask once (continue / update the installed libutils with `libutils-install` / the project's requirement with
`libutils-setup` / the skills reference) and remember it. The template needs libutils **`4.1.0`** at least
(`set(LIBUTILS_VERSION 4.1.0)`: the Windows packages `mingw64-libutils*`); older installed -> `libutils-install`.

## 1. Ask (one AskUserQuestion call, French)
Name and core class (PascalCase) are usually in the request: ask in plain text if missing. Both are **C++
identifiers** (`[A-Za-z_][A-Za-z0-9_]*`: `my_project`, never `my-project`), neither a C++ keyword nor `std` / `utils`,
nor `testing` / `Test` (GoogleTest): the script refuses them, ask another name.
1. **Type de projet**: `Binaire (Recommandé)` · `Bibliothèque (.a / .so + headers)` · `Header-only`. For a library,
   ask after: static `.a` (default) or shared `.so` (`<NAME>_SHARED_LIB=ON`). A binary that also ships its library and
   headers: `<NAME>_PACKAGE_LIB=ON` + `<NAME>_PACKAGE_DEV=ON` (step 3).
2. **Publication** (GitHub repository only): `Sans gh-pages (Recommandé)` (the packages stay artifacts of the runs) ·
   `Branche gh-pages` (signed RPM/DEB repository + the docs on `https://<owner>.github.io/<repo>/`, needs the GPG
   secrets and GitHub Pages).
3. **CI/CD** (GitHub only, see section 5): `Workflows du template (Recommandé)` · `Personnalisée (ci-cd)` · `Aucune`.
4. **libutils**: `Oui (Recommandé)` · `Non`.
5. **Extras** (multiSelect): `Dépôt GitHub (git init + gh)` · `Doc HTML (html-doc, au-delà du placeholder)` ·
   `Classes / modules en plus (cpp-class)`.
Plus the header banner question of `cpp-class` (once, reused for every generated file). The Windows build and the
unit tests come with the template (nothing to ask): only say it.

## 2. Generate
```bash
bash SKILL_DIR/scripts/new_project.sh <dir> <name> <Core> --kind binary|library|header-only \
    --gh-pages|--no-gh-pages [--repo <owner/repo>] [--no-libutils]
```
It copies the **committed** files of the local clone (`git archive HEAD`, never its work in progress) or clones the
repository, runs the template's `setup.sh` in `<dir>` with every choice given (no question, **never `--push`**), then
removes the exception generation for `--no-libutils` and fills the `.gitignore` block (section 6). `<dir>` is created;
an existing one may only hold a `.git`, a `LICENSE` or hidden files. `--repo`: the GitHub `owner/repo` (default: the
`origin` remote of `<dir>`, else `TsukiNi22/<name>`), written in the CMake, the workflows, the docs, the packages
repository. What `setup.sh` does (template README, *Quick Setup*):
- renames `template` -> `<name>`, `TEMPLATE_` -> `<NAME>_`, `Core` -> `<Core>`, `CORE_H` -> `<CORE>_H` (whole words, files,
  folders, sources, CMake, workflows, scripts, `Doxyfile`), the header dates set to today;
- the kind: `<NAME>_KIND` of the CMake, the `PACKAGE_BIN` / `PACKAGE_LIB` / `PACKAGE_DEV` of `build.yml` and
  `build-packages.yml`; `library`: no `main.cpp`; `header-only`: no `src/`, the class defined in its header, its test
  rewritten;
- writes `README.md` (dependencies, packages, setup, usage, workflows), `CHANGELOG.md` (first entry `## v1.0.0 - <date>`,
  the `git-conventions` format read by the release workflow) and `docs/` (technical page placeholder + the project graph);
- gh-pages: the packages setup script as root `setup.sh` (from `install.sh`: mirror + GPG key + install, `--pre`,
  `--windows`, `--no-install`, `--no-sudo`), keeps `docs.yml` and the publication job, `git init -b main` when needed and
  creates the `gh-pages` branch (seed `<name>.repo`, `RPM-GPG-KEY-tsukini`, `Sync Packages (CD)`, `docs/`) **without
  touching the working tree nor committing `main`**; without: `install.sh` and `docs.yml` removed.

## 3. Adapt (rules of `cmake-style`)
The generated `CMakeLists.txt` is final for the three kinds: options at the top (`<NAME>_KIND`, `<NAME>_PACKAGE_BIN`,
`<NAME>_PACKAGE_LIB`, `<NAME>_PACKAGE_DEV`, `<NAME>_SHARED_LIB`, `<NAME>_STABLE_RELEASE`, `BUILD_TESTS`), libutils,
exception header, `release` / `install_release` / `package_release` targets, CPack (RPM/DEB, `-op` / `-db` / `-as`
variants, `-dev`, global package, `mingw64-` packages for Windows). Never replace it by the `cmake-style` templates.
- **Sources**: `SRC` = the executable only (`main.cpp`), `SRC_SHARED` = the executable and the library (`<Core>-init.cpp`,
  `<Core>.cpp`, every new module). A new `.cpp` goes in one of them (`cpp-class`, `cmake_add.py`);
  `get_unregistered_files` lists the missing ones.
- **Package options changed** (a binary shipping its library: `<NAME>_PACKAGE_LIB=ON`, the executable linked on
  `<name>_lib`): update `PACKAGE_BIN` / `PACKAGE_LIB` / `PACKAGE_DEV` of `.github/workflows/build.yml` and
  `build-packages.yml` the same way.
- **Without libutils** (`--no-libutils`): `main.cpp` from `cpp-class/templates/main-std.cpp`, `<Core>-init.cpp` reduced
  to an empty `init`, remove `find_package(utils ...)`, `utils::utils`, the exception blocks and `include/exception` from
  the CMake and `tests/CMakeLists.txt`, the `libutils-dev` requirement of the packages (`cmake-style`); attributes in
  `std` mode (`cpp-class`). The workflows' containers still work (libutils unused).
- New classes / modules asked with the project: `cpp-class` (empty bodies, `SRC_SHARED` registration).
- **Windows** (always there): cross compiled from Linux with the template's toolchain
  `cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized`
  (needs `mingw64-gcc-c++ mingw64-libutils`, `libutils-install windows`), `<name>.exe` at the root, `mingw64-<name>*`
  packages with `package_release`, no `-as` variant. Portable code only: system calls through `utils::platform::`,
  `<windows.h>` before `<utils/utils.hpp>`, `#if UTILS_WINDOWS` for what differs (rules of `libutils`, Platforms).

## 4. Exceptions & tests
- Exceptions (libutils only): `cmake/config/exceptions/global.json` + `cmake/scripts/` + the CMake block come with the
  template; replace the example codes by the project ones with `libutils-exception`. A library exposing its own codes
  installs the generated header (see `libutils-exception`), otherwise the `InternalCode` of libutils.
- Tests: `tests/CMakeLists.txt` + `tests/<name>/<Core>.cpp` come with the template (`-DBUILD_TESTS=ON`, `unit_tests` at
  the root, linked on `<name>_lib` or built from `SRC_SHARED`, GTest fetched from the sources when cross compiling, run
  under `wine` by `ctest`). More tests: `cpp-tests`.

## 5. CI/CD
The template's workflows are kept as generated when the project is a GitHub repository (`git remote -v` shows
`github.com`, or the `Dépôt GitHub` extra was chosen):
| Workflow | What |
|---|---|
| `dispatch.yml` | every push on `main` / tag `v*` (not `*.md` / `*.txt` / `docs/` only): decides what runs from the ref / message |
| `unit-tests.yml` | always first: Linux (`unit-tests` image) and Windows (`mingw64-unit-tests`, under `wine`) |
| `build.yml` | build check only (Linux `ci`, Windows `mingw64-ci`) when no packages are built |
| `build-packages.yml` | RPM/DEB (+ `mingw64-*`), checked against `PACKAGE_*`; gh-pages: signed and published (5 last `-pre` kept) |
| `release.yml` | the GitHub release of a tag from its `CHANGELOG.md` section (`## vX.Y.Z - date` / `## [vX.Y.Z-pre] - date`) |
| `docs.yml` (gh-pages) | `docs/` copied on `gh-pages` at each push changing it |
- Triggers (`git-conventions`): `[build]` in a commit or a tag `vX.Y.Z-<suffix>` = pre-release packages (`-pre`), a tag
  `vX.Y.Z` = stable packages + GitHub release (its CHANGELOG section must exist), `[ignore]` = no CI, `[release]` =
  the commented `notify-downstream` job (a `repository_dispatch` example).
- `Personnalisée`: the **`ci-cd` skill** (load it) on top of these files (style check with xstyle, coverage...): it
  edits them, never rewrites them from scratch. `Aucune`: remove `.github/workflows/`. Without GitHub: no workflow,
  say it.
- To tell the user (never created / pushed without an explicit request): repository variable `RUNNER`
  (`gh variable set RUNNER --body <runner>`), the containers `ghcr.io/tsukini22/ci`, `unit-tests`, `mingw64-ci`,
  `mingw64-unit-tests`, `package`, `git`; gh-pages: secrets `GPG_PRIVATE_KEY` (base64), `GPG_KEY_ID`,
  `GPG_PASSPHRASE`, GitHub Pages on `gh-pages` (`gh api -X POST repos/<owner>/<repo>/pages -f 'source[branch]=gh-pages'
  -f 'source[path]=/'`), `git push origin gh-pages`; `DOWNSTREAM_REPO_TOKEN` only when the downstream job is enabled.

## 6. .gitignore of the produced files
After the CMake is final (and after every change of targets/outputs):
```bash
python3 SKILL_DIR/scripts/update_gitignore.py <dir>
```
It reads the CMake (and its `add_subdirectory`) and keeps a managed block in `.gitignore` with what the build writes
in the sources: executables written at the root (`/<name>`, `/<name>.exe`, `/unit_tests`), plugins/libraries written in
the sources (`/plugins/rules/*.so`...), generated files (exception header, `configure_file` outputs), packages
(`*.rpm`, `*.deb`, `_CPack_Packages/` when CPack is used) and `/build*`; lines already ignored (even by a global
`*.so`) are not repeated, the block is regenerated at each run. Other custom outputs (data files, logs, generated
assets of the project): add them under the block in a `## <Name>` group like the existing ones.

## 7. Docs & repository
- README and CHANGELOG are written by `setup.sh`: complete them (`readme-style`: what the project does, its usage),
  keep the CHANGELOG format (`git-conventions`, the release workflow reads it). `docs/index.html` is a "coming soon"
  placeholder: the real page with `html-doc` when chosen; `docs/graph.html` refreshes itself from the repository.
- Repository only on request: `git init -b main` (done by `setup.sh` with gh-pages), `gh repo create <owner>/<name>
  --public --source . --remote origin`, first commit `chore: setup c++20 project from cpp_project_template`
  (`git-conventions`, no AI attribution); team project -> `dev` branch + protection of `main` (`git-conventions`).

## 8. Check
- `make` (or `cmake -S . -B build && cmake --build build`), run the binary (`./<name> --help`), `-DBUILD_TESTS=ON` +
  `ctest`, `cmake --build build --target get_unregistered_files` (no unregistered `.cpp`).
- Windows: the toolchain build above, then `WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine ./<name>.exe --help`.
- Library: `cmake --install build --prefix <tmp>` then `find_package(<name>)` from a tiny consumer.
- `git status --ignored --short` after a build: every produced file is ignored, no source is.
- No placeholder left: `grep -rnwE 'template|Core|TEMPLATE_' --exclude-dir=.git --exclude-dir=build .` (the `docs/`
  graph and `include/utils/` aside); workflows valid (`python3 <ci-cd>/scripts/check_workflows.py .github/workflows/*.yml`).
- Report: created tree, kind, gh-pages or not, CI/CD and what the user still has to configure (variable, secrets,
  Pages, the push of `gh-pages`).
