---
name: libutils-install
description: Install, update, remove or check libutils (TsukiNi22/libutils) on this computer, choosing the method from the OS by itself (dnf on Fedora/RHEL, apt on Debian/Ubuntu, build from the sources otherwise), last version by default through the pre channel (libutils-pre), optional debug/asan builds, mirror setup. Use whenever libutils must be installed, upgraded, removed, repaired, or when a build fails because libutils is missing or too old (find_package(utils) error, utils/utils.hpp not found, version requirement).
---

# libutils on the system

Everything goes through `scripts/libutils.sh` (`SKILL_DIR` = directory of this file):
```bash
bash SKILL_DIR/scripts/libutils.sh status                      # always first (no root needed)
bash SKILL_DIR/scripts/libutils.sh install [--variants db,as|all] [--stable]
bash SKILL_DIR/scripts/libutils.sh update                      # keeps the installed variants
bash SKILL_DIR/scripts/libutils.sh remove [--repo] [--local]
bash SKILL_DIR/scripts/libutils.sh repo                        # mirror + GPG key only
bash SKILL_DIR/scripts/libutils.sh install --source [<git ref>] # build from the sources into /usr/local
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
- Channel: **`-pre` by default** (last version, ex: `libutils-pre` 2.14.0 while the stable `libutils` is 2.13.2; check with `status`),
  `--stable` for the stable one. The two channels conflict: switching removes the other one first.
- Packages: `libutils[-pre]` (= headers + CMake config + optimized `libutils.a`), variants `libutils-db[-pre]`
  (debug, `libutils_debug.a`) and `libutils-as[-pre]` (asan, `libutils_asan.a`); a project built in `Debug`/`Asan`
  needs the matching variant (the CMake config of libutils stops with an explicit message otherwise).
- `status` warns when a source install in `/usr/local` shadows the packages (`/usr/local` is searched first by
  the compiler and `find_package`): headers of one version, library of another. `remove --local` deletes it.

## Rules
- `status` can always be run. **install / update / remove / repo need root**: without a terminal (Claude's shell)
  the script switches by itself to `sudo -A` with the graphical prompt `~/.local/bin/sudo-askpass` (zenity window
  showing the command, the user types the password). Without any display/askpass it stops (exit code 2): give the
  user the command (`SUDO_ASKPASS=~/.local/bin/sudo-askpass bash .../libutils.sh <command>`). Never work around sudo.
- Ask before `install`/`update`/`remove` (system change); `status` and reading versions need no confirmation.
- Variants: install `db` (and `as`) when the project is built in `Debug`/`Asan` (`buildD`/`buildA` aliases of the
  user, `CMAKE_BUILD_TYPE`), otherwise the optimized build is enough.
- After an install/update: `status` again, then rebuild the project (`make re`) and report the version used.
- Setup of libutils **in a project** (CMake, exceptions, includes): the `libutils-setup` skill.
