#!/bin/bash
# Install / update / remove libutils on this computer, depending on the OS.
#
# Usage: libutils.sh <command> [options]
#
# Commands:
#   status             installed packages/version, mirror, last available version (no root needed)
#   install            setup the mirror if needed, then install libutils (default: the channel with the newest version)
#   update             update the installed libutils packages (install them if missing)
#   remove             remove every libutils package (--repo: also the mirror & key)
#   repo               only setup the mirror & GPG key
#   channel            the channel install would use (the newest of stable / pre), no root
#   windows            libutils for Windows x86_64 (v4.1.0+): its packages mingw64-libutils* (dnf/apt, in the MinGW sysroot);
#                      none on the mirror or --source: built from the sources (MinGW-w64) into --prefix, no root
#
# Options:
#   --stable / --pre   force the stable channel (libutils) or the pre channel (libutils-pre); default: the newest one
#   --variants <list>  extra builds: db (debug), as (asan), all (default: none, only the optimized one)
#   --source [<ref>]   build & install from the sources into /usr/local (other OS, or forced), ref default main
#   --repo             with remove: also remove the mirror & key
#   --local            with remove: also remove the source install of /usr/local
#   --no-sudo          already root
#   --prefix <dir>     with windows: where it goes (default: ~/.local/share/libutils/windows)
#   -y                 don't ask for confirmation (dnf/apt -y)

set -euo pipefail

BASE_URL="https://tsukini22.github.io/libutils"
SETUP_URL="https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh"
REMOTE="https://github.com/TsukiNi22/libutils.git"
SUDO="sudo"
CHANNEL="auto"
VARIANTS=""
SOURCE=false
REF="main"
REPO=false
LOCAL=false
YES=""
COMMAND="${1:-}"
[[ $# -gt 0 ]] && shift

PREFIX="$HOME/.local/share/libutils/windows"

usage() { sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --stable) CHANNEL="" ;;
        --pre) CHANNEL="-pre" ;;
        --variants) VARIANTS="$2"; shift ;;
        --source) SOURCE=true; if [[ $# -gt 1 ]] && [[ "$2" != -* ]]; then REF="$2"; shift; fi ;;
        --repo) REPO=true ;;
        --local) LOCAL=true ;;
        --no-sudo) SUDO="" ;;
        --prefix) PREFIX="$2"; shift ;;
        -y) YES="-y" ;;
        -h|--help) usage 0 ;;
        *) echo "Error: unknown argument '$1'" >&2; usage 1 ;;
    esac
    shift
done
[[ -z "$COMMAND" ]] && usage 1
[[ "$VARIANTS" = "all" ]] && VARIANTS="db,as"
for v in ${VARIANTS//,/ }; do [[ "$v" =~ ^(db|as)$ ]] || { echo "Error: unknown variant '$v' (db, as, all)" >&2; exit 1; }; done

# =========================
# OS
# =========================
OS_ID="unknown"; OS_LIKE=""; OS_VERSION=""
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    OS_ID="${ID:-unknown}"; OS_LIKE="${ID_LIKE:-}"; OS_VERSION="${VERSION_ID:-}"
fi
FAMILY="other"
if [[ "$OS_ID" =~ ^(fedora|rhel|centos|rocky|almalinux)$ ]] || [[ "$OS_LIKE" =~ (fedora|rhel) ]]; then FAMILY="rpm"
elif [[ "$OS_ID" =~ ^(debian|ubuntu)$ ]] || [[ "$OS_LIKE" =~ (debian|ubuntu) ]]; then FAMILY="deb"
fi
$SOURCE && FAMILY="source"

# Packages to install: libutils[-pre] (dev + optimized) + the variants
packages() {
    local list="libutils${CHANNEL}"
    for v in ${VARIANTS//,/ }; do
        case "$v" in
            db|as) list="$list libutils-${v}${CHANNEL}" ;;
            *) echo "Error: unknown variant '$v' (db, as, all)" >&2; exit 1 ;;
        esac
    done
    echo "$list"
}
other_channel() { [[ -z "$CHANNEL" ]] && echo "-pre" || echo ""; }
available() { # <package>: its newest version on the mirror ("" unknown)
    case "$FAMILY" in
        rpm) dnf -q repoquery --latest-limit 1 --qf '%{version}\n' "$1" 2>/dev/null | head -1 ;;
        deb) apt-cache policy "$1" 2>/dev/null | sed -n 's/^ *Candidate: \([^-]*\).*/\1/p' | grep -v none | head -1 ;;
    esac
    return 0
}
resolve_channel() { # [<base>] auto: the channel with the newest version (the pre channel may lag behind a stable release)
    [[ "$CHANNEL" != "auto" ]] && return 0
    local base="${1:-libutils}" pre stable
    pre="$(available "$base-pre")"; stable="$(available "$base")"
    if [[ -n "$pre" ]] && [[ "$pre" != "$stable" ]] && [[ "$(printf '%s\n%s\n' "$pre" "${stable:-0}" | sort -V | tail -1)" == "$pre" ]]; then CHANNEL="-pre"; else CHANNEL=""; fi
    echo "Channel: $base${CHANNEL} (stable ${stable:-?}, pre ${pre:-?})"
}

installed_rpm() { rpm -qa --qf '%{NAME} %{VERSION}-%{RELEASE}\n' 'libutils*' 'mingw64-libutils*' 2>/dev/null | sort; }
installed_deb() { dpkg-query -W -f '${Package} ${Version} ${Status}\n' 'libutils*' 'mingw64-libutils*' 2>/dev/null | awk '$NF=="installed"{print $1, $2}' | sort; }
installed() { case "$FAMILY" in rpm) installed_rpm ;; deb) installed_deb ;; *) true ;; esac; }
header_version() {
    for d in /usr/include /usr/local/include; do
        [[ -f "$d/utils/version.hpp" ]] && echo "$(sed -n 's/.*__LIBUTILS_VERSION__ "\(.*\)".*/\1/p' "$d/utils/version.hpp" | head -1) ($d)"
    done
}
repo_ready() {
    case "$FAMILY" in
        rpm) [[ -f /etc/yum.repos.d/libutils.repo ]] ;;
        deb) [[ -f /etc/apt/sources.list.d/libutils.list ]] ;;
        *) false ;;
    esac
}
need_root() {
    [[ -z "$SUDO" ]] && return 0
    [[ "$SUDO" == *"-A"* ]] && return 0
    sudo -n true 2> /dev/null && return 0
    [[ -t 0 ]] && return 0
    # No terminal: graphical password prompt (sudo -A + SUDO_ASKPASS) when a display is available
    local askpass="${SUDO_ASKPASS:-$HOME/.local/bin/sudo-askpass}"
    [[ -x "$askpass" ]] || askpass="$(cd "$(dirname "$0")" && pwd)/sudo-askpass" # copy shipped with the skill
    if [[ -x "$askpass" ]] && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
        export SUDO_ASKPASS="$askpass"
        SUDO="sudo -A"
        return 0
    fi
    echo "Error: root rights needed, no terminal and no graphical askpass (SUDO_ASKPASS)." >&2
    echo "Run it yourself: bash $0 $COMMAND ..." >&2
    exit 2
}
setup_repo() {
    repo_ready && return 0
    need_root
    echo "Setting up the libutils mirror ($FAMILY)..."
    # The official script calls sudo by itself: run it as root once (--no-sudo) to ask the password only once
    if [[ -n "$SUDO" ]]; then $SUDO bash -c "curl -fsSL '$SETUP_URL' | bash -s -- --no-sudo"; else curl -fsSL "$SETUP_URL" | bash -s -- --no-sudo; fi
    [[ "$FAMILY" = "deb" ]] && $SUDO apt-get update -q
    return 0
}

# =========================
# Source build (other OS / forced)
# =========================
source_install() {
    command -v cmake > /dev/null && command -v clang++ > /dev/null && command -v git > /dev/null \
        || { echo "Error: git, cmake, clang++, python3 and OpenSSL (dev) are required to build libutils" >&2; exit 1; }
    tmp="$(mktemp -d)" # global: the EXIT trap runs after the function
    trap 'rm -rf "${tmp:-}"' EXIT
    git clone -q --depth 1 --branch "$REF" "$REMOTE" "$tmp/libutils"
    cmake -S "$tmp/libutils" -B "$tmp/libutils/build" > /dev/null
    need_root
    $SUDO cmake --build "$tmp/libutils/build" --target install_release --parallel "$(nproc)"
    echo "libutils ($REF) installed into /usr/local (Debug, Asan and Optimized builds)"
}

# =========================
# Windows (MinGW-w64 cross compilation)
# =========================
MINGW="x86_64-w64-mingw32"
windows_packages() { # mingw64-libutils[-pre] (dev + optimized) + the debug variant (no asan with MinGW)
    local list="mingw64-libutils${CHANNEL}"
    for v in ${VARIANTS//,/ }; do
        case "$v" in
            db) list="$list mingw64-libutils-db${CHANNEL}" ;;
            as) echo "note: no asan variant with MinGW (Asan is built like Debug)" >&2 ;;
        esac
    done
    echo "$list"
}
windows_hint() {
    echo "A project for Windows (find_package(utils) finds it in the MinGW sysroot):"
    if command -v mingw64-cmake > /dev/null; then
        echo "  mingw64-cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Optimized && cmake --build build-windows"
    else
        echo "  cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=<libutils>/cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized"
    fi
}
windows_install() {
    # its packages first (v4.1.0+: mingw64-libutils*, same mirror), else built from the sources into --prefix
    if ! $SOURCE && { [[ "$FAMILY" = "rpm" ]] || [[ "$FAMILY" = "deb" ]]; }; then
        setup_repo
        [[ "$FAMILY" = "deb" ]] && $SUDO apt-get update -q > /dev/null 2>&1 || true
        if [[ -n "$(available mingw64-libutils)$(available mingw64-libutils-pre)" ]]; then
            resolve_channel mingw64-libutils
            need_root
            local pkgs; pkgs="$(windows_packages)"
            case "$FAMILY" in
                rpm) $SUDO dnf install $YES --refresh $pkgs ;;
                deb) $SUDO apt-get install $YES $pkgs ;;
            esac
            echo "libutils for Windows installed ($pkgs) into the MinGW sysroot"
            windows_hint
            return 0
        fi
        echo "No mingw64-libutils package on the mirror (libutils v4.1.0+): built from the sources into $PREFIX"
    fi
    command -v "$MINGW-g++" > /dev/null || { echo "Error: MinGW-w64 is required: dnf install mingw64-gcc-c++ mingw64-openssl (Fedora), apt install g++-mingw-w64-x86-64 + an OpenSSL built for MinGW (Debian/Ubuntu)" >&2; exit 1; }
    command -v cmake > /dev/null && command -v git > /dev/null || { echo "Error: git and cmake are required" >&2; exit 1; }
    tmp="$(mktemp -d)" # global: the EXIT trap runs after the function
    trap 'rm -rf "${tmp:-}"' EXIT
    git clone -q --depth 1 --branch "$REF" "$REMOTE" "$tmp/libutils"
    [[ -f "$tmp/libutils/cmake/toolchain/mingw-w64.cmake" ]] || { echo "Error: libutils $REF has no Windows support (v4.0.0+)" >&2; exit 1; }
    cmake -S "$tmp/libutils" -B "$tmp/build" -DCMAKE_TOOLCHAIN_FILE="$tmp/libutils/cmake/toolchain/mingw-w64.cmake" -DCMAKE_BUILD_TYPE=Optimized > /dev/null
    cmake --build "$tmp/build" --parallel "$(nproc)"
    cmake --install "$tmp/build" --prefix "$PREFIX" > /dev/null
    # its toolchain kept with it: a project builds for Windows with the same one
    command mkdir -p "$PREFIX/share/toolchain"
    cp "$tmp/libutils/cmake/toolchain/mingw-w64.cmake" "$PREFIX/share/toolchain/mingw-w64.cmake"
    echo "libutils ($REF) for Windows installed into $PREFIX"
    echo "A project for Windows:"
    echo "  cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=$PREFIX/share/toolchain/mingw-w64.cmake -DCMAKE_FIND_ROOT_PATH=$PREFIX -DCMAKE_BUILD_TYPE=Optimized"
}
windows_version() { # the packages (MinGW sysroot), else the source build of --prefix
    for d in /usr/$MINGW/sys-root/mingw/include /usr/$MINGW/include "$PREFIX/include"; do
        if [[ -f "$d/utils/version.hpp" ]]; then
            echo "$(sed -n 's/.*__LIBUTILS_VERSION__ "\(.*\)".*/\1/p' "$d/utils/version.hpp" | head -1) (${d%/include})"
            return 0
        fi
    done
    return 0
}

# =========================
# Commands
# =========================
case "$COMMAND" in
    status)
        echo "OS: $OS_ID $OS_VERSION ($FAMILY)"
        echo "Mirror: $(repo_ready && echo configured || echo 'not configured')"
        echo "Installed packages:"; installed | sed 's/^/  /' | grep . || echo "  none"
        echo "Headers: $(header_version | paste -sd ' ' || true)"; [[ -z "$(header_version)" ]] && echo "  none"
        if [[ -d /usr/local/include/utils ]] && [[ -d /usr/include/utils ]]; then
            echo "WARNING: a source install in /usr/local shadows the packages (compilers and find_package search /usr/local first)."
            echo "         remove it to use the packages: libutils.sh remove --local (or keep it on purpose)"
        fi
        echo "Windows build: $([[ -n "$(windows_version)" ]] && windows_version || echo "none (libutils.sh windows)")"
        case "$FAMILY" in
            rpm) echo "Last available:"; dnf -q repoquery --latest-limit 1 --qf '  %{name} %{version}-%{release}\n' libutils-pre libutils 2>/dev/null | sort -u || true ;;
            deb) echo "Available:"; apt-cache policy libutils-pre libutils 2>/dev/null | grep -E '^[a-z]|Candidate' | sed 's/^/  /' || true ;;
        esac
        ;;

    windows)
        windows_install
        ;;

    channel)
        resolve_channel
        ;;

    repo)
        [[ "$FAMILY" = "rpm" ]] || [[ "$FAMILY" = "deb" ]] || { echo "No package mirror for this OS: use --source" >&2; exit 1; }
        setup_repo
        ;;

    install|update)
        if [[ "$FAMILY" = "source" ]] || [[ "$FAMILY" = "other" ]]; then
            [[ "$FAMILY" = "other" ]] && echo "No package for '$OS_ID': building from the sources"
            source_install; exit 0
        fi
        setup_repo
        resolve_channel
        need_root
        PKGS="$(packages)"
        # Switching channel: the stable and pre packages conflict (the Windows ones apart: libutils.sh windows)
        if [[ -n "$CHANNEL" ]]; then CONFLICT="$(installed | awk '{print $1}' | grep -v '^mingw64-' | grep -v -- '-pre$' || true)"
        else CONFLICT="$(installed | awk '{print $1}' | grep -v '^mingw64-' | grep -- '-pre$' || true)"; fi
        if [[ -n "$CONFLICT" ]]; then
            echo "Removing the other channel: $(echo $CONFLICT)"
            case "$FAMILY" in rpm) $SUDO dnf remove $YES $CONFLICT ;; deb) $SUDO apt-get remove $YES $CONFLICT ;; esac
        fi
        if [[ "$COMMAND" = "update" ]]; then
            # Keep the variants already installed (db/as) in the selected channel
            for name in $(echo "$CONFLICT" | sed 's/-pre$//'); do [[ -n "$CHANNEL" ]] && PKGS="$PKGS ${name}-pre" || PKGS="$PKGS $name"; done
            for name in $(installed | awk '{print $1}'); do PKGS="$PKGS $name"; done
            PKGS="$(echo $PKGS | tr ' ' '\n' | grep -vE '^libutils-dev' | sort -u | paste -sd ' ')"
            case "$FAMILY" in
                rpm) $SUDO dnf upgrade $YES --refresh $PKGS 2>/dev/null || $SUDO dnf install $YES --refresh $PKGS ;;
                deb) $SUDO apt-get update -q; $SUDO apt-get install $YES $PKGS ;;
            esac
        else
            case "$FAMILY" in
                rpm) $SUDO dnf install $YES --refresh $PKGS ;;
                deb) $SUDO apt-get install $YES $PKGS ;;
            esac
        fi
        echo; installed
        ;;

    remove)
        if [[ "$FAMILY" = "rpm" ]] || [[ "$FAMILY" = "deb" ]]; then
            PKGS="$(installed | awk '{print $1}' | paste -sd ' ')"
            if [[ -n "$PKGS" ]]; then
                need_root
                case "$FAMILY" in rpm) $SUDO dnf remove $YES $PKGS ;; deb) $SUDO apt-get remove $YES $PKGS ;; esac
            else
                echo "No libutils package installed"
            fi
            if $REPO; then
                need_root
                case "$FAMILY" in
                    rpm) $SUDO rm -f /etc/yum.repos.d/libutils.repo ;;
                    deb) $SUDO rm -f /etc/apt/sources.list.d/libutils.list /etc/apt/keyrings/libutils.gpg ;;
                esac
                echo "Mirror removed"
            fi
        fi
        if [[ -d /usr/local/include/utils ]] || ls /usr/local/lib*/libutils*.a > /dev/null 2>&1; then
            if $LOCAL; then
                need_root
                $SUDO rm -rf /usr/local/include/utils /usr/local/lib/libutils*.a /usr/local/lib64/libutils*.a \
                    /usr/local/lib/cmake/utils /usr/local/lib64/cmake/utils /usr/local/share/licenses/utils
                echo "Source install removed from /usr/local"
            else
                echo "A source install exists in /usr/local (use --local to remove it)"
            fi
        fi
        ;;

    *) usage 1 ;;
esac
