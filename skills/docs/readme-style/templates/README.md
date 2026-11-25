# <project-name>

<p align="center">
  <img alt="Version" src="https://img.shields.io/github/v/release/<owner>/<repo>">
  <img alt="CI" src="https://github.com/<owner>/<repo>/actions/workflows/dispatch.yml/badge.svg">
  <img alt="Last commit" src="https://img.shields.io/github/last-commit/<owner>/<repo>">
  <img alt="License" src="https://img.shields.io/github/license/<owner>/<repo>">
</p>

> [!TIP]
> Documentation [<owner>/<repo>](https://<owner>.github.io/<repo>) (v1.0.0).

<One line: what it does (`<project> <mode>` does X).>
<One line, only if any: the limit or fallback.>

### Table of Contents
 - [Features](#features)
 - [Dependencies](#dependencies)
 - [Packages](#packages)
 - [Quick Setup 1 (All)](#quick-setup---1-all)
 - [Quick Setup 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Unit tests](#unit-tests)
 - [Workflows/Release](#workflowsrelease)
 - [License](#license)

## Features
- **<Label>:** <a few words>
- **<Label>:** <a few words>
- **<Label>:** <a few words>

## Dependencies

> [!CAUTION]
> This project's license does not apply to the content of the dependencies used by `<project>`.

| Name + Link | Status | Last Update |
| ----------- | ------ | ----------- |
| [libutils](https://github.com/TsukiNi22/libutils) | ![CD - Dispatch](https://github.com/TsukiNi22/libutils/actions/workflows/dispatch.yml/badge.svg) | ![](https://img.shields.io/github/last-commit/TsukiNi22/libutils) |

Build requirements (only needed for the [Quick Setup 1](#quick-setup---1-all)):

| Name | Version | Fedora (`dnf`) | Debian/Ubuntu (`apt`) |
| ---- | ------- | -------------- | --------------------- |
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` | `clang cmake` |

## Packages

> [!NOTE]
> `<project>-pre` is the pre-release: a stable package of the same version or newer replaces it.

| File Name | Content |
| --------- | ------- |
| `<project>` | Stable binary |
| `<project>-pre` | Pre-release binary |

## Quick Setup - 1 (all)
> Setup into `/usr/local`

### Clone the repository
```bash
git clone https://github.com/<owner>/<repo>.git
cd <repo>
```

### Build & install
```bash
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR
cmake --build $BUILD_DIR --parallel $(nproc)    # build the binary
sudo cmake --install $BUILD_DIR                 # install the binary
```

## Quick Setup - 2 (Limited)
> Setup into `/usr`

> [!WARNING]
> Restriction: `fedora-based (rpm)`, `debian-based (deb)`

No clone needed. The script:
1. adds the mirror
2. imports its GPG key
3. installs the package

```bash
curl -fsSL https://raw.githubusercontent.com/<owner>/<repo>/main/setup.sh | bash -s
```

or with `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/<owner>/<repo>/main/setup.sh | bash -s
```

| Argument | Effect |
| -------- | ------ |
| `--no-sudo` | No `sudo` (run it as root) |

Known errors: [`readme/install.md`](readme/install.md).

## Usage
```bash
<project> -h                # help
<project> <mode> <args>     # what it does
```

| Mode | Flags |
| ---- | ----- |
| `<mode>` | `-x\|--example <value>` |

## Unit tests
```bash
cmake -S . -B $BUILD_DIR -DBUILD_TESTS=ON
cmake --build $BUILD_DIR --parallel $(nproc)
ctest --test-dir $BUILD_DIR --output-on-failure
```

## Workflows/Release
### Workflows
- `Dispatch (CI/CD)`: on each push to `main` or a `v*` tag, picks the jobs

### Pre-Release (unstable)
- a tag `vX.Y.Z-pre`, or a commit with `[build]`

### Release (stable)
- a tag `vX.Y.Z` only

## License
<!-- or Credits / Contributors / Acknowledgements: the closing section -->
[MIT](https://github.com/<owner>/<repo>/blob/main/LICENSE)
