# <project-name>

> [!TIP]
> Documentation [<owner>/<repo>](https://<owner>.github.io/<repo>) (v1.0.0).

<What the project is in one sentence: `<project> <mode>` does X, using Y.>
<What happens when something is missing (fallback / limits).>

### Table of Contents
 - [Dependencies](#dependencies)
 - [Packages](#packages)
 - [Quick Setup 1 (All)](#quick-setup---1-all)
 - [Quick Setup 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Unit tests](#unit-tests)
 - [Workflows/Release](#workflowsrelease)

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
> The package have a pre-release/unstable version named `<project>-pre`.
> The `-pre` package is marked as obsolete by any release/stable package (without `-pre`) of the same version or higher.

| File Name | Content |
| --------- | ------- |
| `<project>` | Stable binary |
| `<project>-pre` | Pre-release/unstable binary |

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

> [!NOTE]
> The usage of `sudo` in the script can be remove using `--no-sudo` argument

Run the setup script directly, without cloning the repository manually. It:
1. Setup the mirror
2. Import the GPG key used to sign the packages/metadata
3. Install the package

```bash
wget -qO- https://raw.githubusercontent.com/<owner>/<repo>/main/setup.sh | bash -s
```

or with `curl`:

```bash
curl -fsSL https://raw.githubusercontent.com/<owner>/<repo>/main/setup.sh | bash -s
```

| Argument | Effect |
| -------- | ------ |
| `--no-sudo` | Run without sudo (requires the script to be run as root already) |

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
- The workflow `Dispatch (CI/CD)` runs on every push (branch `main` or tag `v*`) and decides what to trigger based on the ref/commit message

### Pre-Release (unstable)
- Pushing a tag that matches the regex `vx.x.x-pre` (`x` stands for the version number: `major`, `minor`, `fix`)
- Or pushing a commit containing the string `[build]`, preferably in the description

### Release (stable)
The release (channel `stable`) can only be triggered by pushing a tag that matches the regex `vx.x.x`
