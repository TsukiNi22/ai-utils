## Table of Contents
 - [Packages](#packages)
 - [Quick Setup 1 (All)](#quick-setup---1-all)
 - [Quick Setup 2 (Limited)](#quick-setup---2-limited)

# Packages
> Only available after [Quick Setup 2 (Limited)](#quick-setup---2-limited)

> [!NOTE]
> Most packages have a pre-release/unstable version named `<package_name>-pre`.
> The `-pre` packages are marked as obsolete by any release/stable package (without `-pre`) of the same version or higher.

| File Name | Content |
| --------- | ------- |
| `{{package}}` | {{content}} |

# Quick Setup - 1 (all)
> Setup into `/usr/local`

### Clone the repository
```bash
git clone https://github.com/{{OWNER}}/{{REPO}}.git
cd {{REPO}}
```

### Install the lib
```bash
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR
sudo cmake --build $BUILD_DIR --target install --parallel $(nproc)
```

# Quick Setup - 2 (Limited)
> Setup into `/usr`

> [!WARNING]
> Restriction: `fedora-based (rpm)`, `debian-based (deb)`

> [!NOTE]
> The usage of `sudo` in the script can be remove using `--no-sudo` argument

Run the setup script directly, without cloning the repository manually:
1. {{what the script does, step by step}}

```bash
wget -qO- https://raw.githubusercontent.com/{{OWNER}}/{{REPO}}/main/setup.sh | bash -s
```

or with `curl`:

```bash
curl -fsSL https://raw.githubusercontent.com/{{OWNER}}/{{REPO}}/main/setup.sh | bash -s
```
