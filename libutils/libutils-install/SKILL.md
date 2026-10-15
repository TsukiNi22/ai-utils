---
name: libutils-install
description: Install, update, remove or check libutils (TsukiNi22/libutils) on this computer, choosing the method from the OS by itself (dnf on Fedora/RHEL, apt on Debian/Ubuntu, build from the sources otherwise), the newest version by default (the stable or the pre channel, whichever is newer), optional debug/asan builds, mirror setup, and libutils for Windows (v4.1.0+: its packages mingw64-libutils* in the MinGW sysroot; else cross compiled from the sources, no root). Use whenever libutils must be installed, upgraded, removed, repaired, or when a build fails because libutils is missing or too old (find_package(utils) error, utils/utils.hpp not found, version requirement).
---

# libutils on the system

Everything goes through `scripts/libutils.sh` (`SKILL_DIR` = directory of this file):
```bash
bash SKILL_DIR/scripts/libutils.sh status                      # always first (no root needed)
bash SKILL_DIR/scripts/libutils.sh install [--variants db,as|all] [--stable|--pre]
bash SKILL_DIR/scripts/libutils.sh channel                     # the channel install would take (no root)
bash SKILL_DIR/scripts/libutils.sh update                      # keeps the installed variants
bash SKILL_DIR/scripts/libutils.sh remove [--repo] [--local]
bash SKILL_DIR/scripts/libutils.sh repo                        # mirror + GPG key only
bash SKILL_DIR/scripts/libutils.sh install --source [<git ref>] # build from the sources into /usr/local
bash SKILL_DIR/scripts/libutils.sh windows [--variants db] [--source] [--prefix <dir>]   # libutils for Windows
```

## libutils freshness
First, once per conversation: the freshness check of the `libutils` skill (`bash <libutils skill>/scripts/check.sh`,
section "Freshness check" of `libutils/SKILL.md`). Outdated -> reuse the answer already given in this conversation,
else ask once (continue / update the installed libutils with `libutils-install` / the project's requirement with
`libutils-setup` / the skills reference) and remember it.

## What it does
- OS from `/etc/os-release`: `rpm` family -> `dnf`, `deb` family -> `apt`, anything else -> source build
  (clone, `cmake --build build --target install_release` = Debug + Asan + Optimized into `/usr/local`).
- Mirror: runs the official `TsukiNi22/libutils/main/setup.sh` (`/etc/yum.repos.d/libutils.repo` or
  `/etc/apt/sources.list.d/libutils.list` + the GPG key) only when it isn't configured yet.
- Channel: **the newest one by default** (`channel` tells it): the pre channel can lag behind a stable release (ex:
  stable `libutils` 4.0.0 while `libutils-pre` is still 2.14.0: the stable one is taken); `--stable` / `--pre` force
  one. The two channels conflict: switching removes the other one first.
- Packages: `libutils[-pre]` (= headers + CMake config + optimized `libutils.a`), variants `libutils-db[-pre]`
  (debug, `libutils_debug.a`) and `libutils-as[-pre]` (asan, `libutils_asan.a`); a project built in `Debug`/`Asan`
  needs the matching variant (the CMake config of libutils stops with an explicit message otherwise).
- `status` warns when a source install in `/usr/local` shadows the packages (`/usr/local` is searched first by
  the compiler and `find_package`): headers of one version, library of another. `remove --local` deletes it.

## Windows (libutils v4.1.0+)
- `windows`: **its packages** first, from the same mirror: `mingw64-libutils[-pre]` (= `mingw64-libutils-dev` +
  `mingw64-libutils-op`), `--variants db` adds `mingw64-libutils-db` (no asan with MinGW: Asan is built like Debug),
  the newest channel like `install`; installed in the MinGW sysroot (`/usr/x86_64-w64-mingw32/sys-root/mingw` on
  Fedora, `/usr/x86_64-w64-mingw32` on Debian), where `find_package(utils)` finds them with the MinGW toolchain - root
  needed (ask first). It prints how a project builds: `mingw64-cmake -S . -B build-windows` (Fedora), else
  `-DCMAKE_TOOLCHAIN_FILE=<libutils>/cmake/toolchain/mingw-w64.cmake`.
- No package on the mirror (older than v4.1.0, another OS) or `--source [<ref>]`: libutils built from the sources
  with its MinGW-w64 toolchain (`Optimized`) into `--prefix` (default `~/.local/share/libutils/windows`, the toolchain
  in `share/toolchain/`), no root; a project then adds `-DCMAKE_FIND_ROOT_PATH=<prefix>`. `status` shows either.
- Needs MinGW-w64: Fedora `mingw64-gcc-c++ mingw64-openssl` (and `wine` to run the `.exe`), Debian/Ubuntu
  `g++-mingw-w64-x86-64` + an OpenSSL built for MinGW (no package): system packages, ask first.
- A project built in `Debug` for Windows needs `mingw64-libutils-db` (the CMake config of libutils names the package
  to install); `Asan` with MinGW stops with an explicit message.

## Rules
- `status` can always be run. **install / update / remove / repo need root**: without a terminal (Claude's shell)
  the script switches by itself to `sudo -A` with the graphical prompt `~/.local/bin/sudo-askpass` (zenity window
  showing the command, the user types the password). Without any display/askpass it stops (exit code 2): give the
  user the command (`SUDO_ASKPASS=~/.local/bin/sudo-askpass bash .../libutils.sh <command>`). Never work around sudo.
- Ask before `install`/`update`/`remove` and `windows` with its packages (system change); `status`, `channel`, reading
  versions and a `windows --source` build (into the home folder) need no confirmation.
- Variants: install `db` (and `as`) when the project is built in `Debug`/`Asan` (`buildD`/`buildA` aliases of the
  user, `CMAKE_BUILD_TYPE`), otherwise the optimized build is enough.
- After an install/update: `status` again, then rebuild the project (`make re`) and report the version used.
- Setup of libutils **in a project** (CMake, exceptions, includes): the `libutils-setup` skill.
