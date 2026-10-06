---
name: cpp-project
description: Set up a new C++20 project in Tsukini's way from the cpp_project_template repository (TsukiNi22/cpp_project_template) - name/core class renaming, kind of project (binary, binary distributed as RPM/DEB packages, static .a or shared .so installable library, header-only), CMake, libutils or not, exception codes, GTest tests, CI/CD GitHub workflows (simple build check or full dispatch/tests/packages/gh-pages mirror), install script, README/CHANGELOG/docs - using the other skills (cmake-style, cpp-class, libutils-exception, readme-style, git-conventions, html-doc). Use whenever the user wants to create, bootstrap, initialize or scaffold a new C++ project or repository.
---

# New C++ project

The base is always **`cpp_project_template`** (local clone `~/personal_delivery/cpp/cpp_project_template`,
otherwise `https://github.com/TsukiNi22/cpp_project_template.git`): executable `template`, core class `Core`
(`init(argc, argv)` with an `ArgParser` + `--verbose`, `run()`), `main.cpp` (catches the libutils and the standard
exceptions), libutils `>= 3.0.0`, exception JSON + generator (checks the codes are C++ identifiers, `Undefined`
reserved), `Makefile`, `Doxyfile`, `.gitignore` (`/<name>`, `/build`, `compile_commands.json`, `/.cache`), CI
`build.yml`. Skill synced with the template at `aa33b8b` (2026-10-05, libutils v3): when its `HEAD` is newer
(`git -C <clone> log --oneline aa33b8b..origin/main`), check what changed against the steps below and the copies
in the other skills (workflows, exception scripts, `main.cpp`, CMake templates) before relying on them. Then adapt it with the other skills:
`cmake-style` (CMake), `cpp-class` (files, header question, main), `libutils-exception` (codes),
`readme-style`, `git-conventions` (CHANGELOG, commits, branches), `html-doc` (docs). Load each one when its step comes.
`SKILL_DIR` = the directory of this file.

## libutils freshness
First, once per conversation: the freshness check of the `libutils` skill (`bash <libutils skill>/scripts/check.sh`,
section "Freshness check" of `libutils/SKILL.md`). Outdated -> reuse the answer already given in this conversation,
else ask once (continue / update the installed libutils with `libutils-install` / the project's requirement with
`libutils-setup` / the skills reference) and remember it.

## 1. Ask (one AskUserQuestion call, French)
Name (lower case, `_` preferred: the template's own `setup.sh` only accepts C++ identifiers; `-` is accepted by
`new_project.sh`, which then uses the name without `-` as namespace) and core class (PascalCase) are usually in the
request: ask in plain text if missing.
1. **Type de projet**: `Binaire` · `Binaire + paquets RPM/DEB` · `Bibliothèque (.a / .so, installable)` · `Header-only`.
   For a library, ask after: static `.a` (default) or shared `.so`, and with or without packages.
2. **CI/CD**: `Build simple (Recommandé)` (build check on push) · `Complète` (dispatch, unit tests, packages,
   signed RPM/DEB mirror on gh-pages) · `Aucune`.
3. **libutils**: `Oui (Recommandé)` · `Non`.
4. **Extras** (multiSelect): `Tests GTest` · `Doc HTML` · `README + CHANGELOG` · `Dépôt GitHub (git init + gh)`.
Plus the header banner question of `cpp-class` (once, reused for every generated file).

## 2. Base
```bash
bash SKILL_DIR/scripts/new_project.sh <dir> <name> <Core> [--no-libutils]
```
Copies the committed template (local clone or GitHub), removes the template-only files (`setup.sh`, `setup.yml`,
`scripts.zip`, the template repository condition of `build.yml`), renames `template` -> `<name>` and `Core` ->
`<Core>` (files, folders, CMake, sources, workflows, guard `<CORE>_H`), namespace = name without `-`, sets the
header dates to today, writes a minimal README, fills the `.gitignore` block (step 7). Never commits.

## 3. Adapt by type (rules and templates of `cmake-style`)
| Type | CMake | Sources |
|---|---|---|
| Binaire | keep the template CMake (already in style); `find_package(utils 3.0.0)` minimum (the template needs v3: sections + `_Attribute`, new names), higher if the project uses something newer (`/usr/include/utils/version.hpp`); an installed libutils older than 3.0.0 -> `libutils-install` | keep `main.cpp`, `<Core>-init.cpp`, `<Core>.cpp` |
| Binaire + paquets | `cmake-style/templates/app/CMakeLists.txt` (Options, CPack, install), keep the `SRC` list of the template | same + root `setup.sh` from `templates/install.sh` |
| Bibliothèque `.a` / `.so` | `cmake-style/templates/lib/CMakeLists.txt` + `cmake/package/<name>Config.cmake.in` (`SHARED` + `OUTPUT_NAME` for `.so`) | remove `main.cpp` and `<Core>-init.cpp` (`ArgParser` is for executables); public headers in `include/<name>/` |
| Header-only | lib template with `src/nothing.cpp` + the `message(WARNING ...)` (`cmake-style`) | headers only |
- Without libutils (`--no-libutils`): `main.cpp` from `cpp-class/templates/main-std.cpp`, `<Core>-init.cpp`
  reduced to an empty `init`, remove `find_package(utils ...)`, `utils::utils`, the exception header block and
  `include/exception` (`cmake-style`); attributes in `std` mode (`cpp-class`).
- New classes/modules asked with the project: `cpp-class` (empty bodies, CMake registration).

## 4. Exceptions (libutils only)
The template ships `cmake/config/exceptions/global.json` + `cmake/scripts/` + the CMake block. Replace the example
codes by the project ones with `libutils-exception`. A library exposing its own codes must also install the
generated header (see `libutils-exception`), otherwise use the `InternalCode` of libutils.

## 5. Tests (if chosen)
`tests/CMakeLists.txt` from `cmake-style/templates/tests/`, first test `tests/<name>/<Core>.cpp` from
`templates/tests/Core.cpp`, `option(BUILD_TESTS ...)` + `add_subdirectory(tests)` in the root CMake, `unit_tests`
in `.gitignore`.

## 6. CI/CD (`templates/workflows/`, placeholders `{{NAME}}`, `{{NAME_UPPER}}` (`-` -> `_`), `{{DNF_DEPS}}`)
| Choice | Workflows (`.github/workflows/`) |
|---|---|
| Build simple | `build.yml` (already in the template, renamed): build with `make re` + check the executable. Library: `build-libraries.yml` triggered on push (replace `workflow_dispatch` by `push: branches: [main]`). |
| Complète, binaire | `dispatch.yml`, `unit-tests.yml`, `build-binary.yml`, `build-packages.yml` (remove `build.yml`) |
| Complète, bibliothèque | `dispatch-library.yml` (as `dispatch.yml`), `unit-tests.yml`, `build-libraries.yml`, `build-packages-library.yml` (as `build-packages.yml`) |
- `{{DNF_DEPS}}`: Fedora packages of the dependencies (`find_package`/`pkg_check_modules`), remove the
  `Install dependencies` step when there are none (libutils and GTest are in the containers).
- Requirements to tell the user: repository variable `RUNNER` (`gh variable set RUNNER --body <runner>`), containers
  `ghcr.io/tsukini22/ci`, `unit-tests`, `package`; for the packages: secrets `GPG_PRIVATE_KEY` (base64),
  `GPG_KEY_ID`, `GPG_PASSPHRASE`, and a `gh-pages` branch with `templates/gh-pages/NAME.repo` (copied as `<name>.repo`),
  the public key `RPM-GPG-KEY-tsukini` (same as libutils' gh-pages) and `templates/gh-pages/sync-packages.yml`
  in its `.github/workflows/`. Never create secrets or push branches without an explicit request.
- Release flow (`git-conventions`): `[build]` in a commit = pre-release packages, tag `vX.Y.Z` = stable,
  `vX.Y.Z-pre` = pre-release, `[ignore]` = skip.
- `templates/install.sh` (root `setup.sh` of a packaged project): mirror + GPG key + install
  (`--no-sudo`, `--pre`, `--no-install`); remove the shell completion part (`--no-rc`, `*_block`, `setup_rc`)
  when the project has no completion files.

## 7. .gitignore of the produced files
After the CMake is final (and after every change of targets/outputs):
```bash
python3 SKILL_DIR/scripts/update_gitignore.py <dir>
```
It reads the CMake (and its `add_subdirectory`) and keeps a managed block in `.gitignore` with what the build writes
in the sources: executables written at the root (`/<name>`, `/unit_tests`), plugins/libraries written in the
sources (`/plugins/rules/*.so`...), generated files (exception header, `configure_file` outputs), packages
(`*.rpm`, `*.deb`, `_CPack_Packages/` when CPack is used) and `/build`; lines already ignored (even by a global
`*.so`) are not repeated, the block is regenerated at each run. Other custom outputs (data files, logs, generated
assets of the project): add them under the block in a `## <Name>` group like the existing ones.

## 8. Docs & repository
- README (`readme-style`, full template for a packaged project, small one otherwise), `CHANGELOG.md`
  (`git-conventions/templates/CHANGELOG.md`), HTML docs (`html-doc`) if chosen.
- Repository only on request: `git init -b main`, `gh repo create TsukiNi22/<name> --public --source . --remote origin`,
  first commit `chore: setup c++20 project from cpp_project_template` (`git-conventions`, no AI attribution);
  team project -> `dev` branch + protection of `main` (`git-conventions`).

## 9. Check
- `make` (or `cmake -S . -B build && cmake --build build`), run the binary (`./<name>`), `-DBUILD_TESTS=ON` + `ctest`,
  `cmake --build build --target get_unregistered_files` (no unregistered `.cpp`).
- Library: `cmake --install build --prefix <tmp>` then `find_package(<name>)` from a tiny consumer.
- `git status --ignored --short` after a build: every produced file is ignored, no source is.
- Workflows: valid YAML (`python3 -c "import yaml; yaml.safe_load(open(f))"`), no `{{...}}` left
  (`grep -rn '{{' .`), no `template` / `Core` left from the base.
- Report: created tree, type, CI/CD and what the user still has to configure (variables, secrets, gh-pages).
