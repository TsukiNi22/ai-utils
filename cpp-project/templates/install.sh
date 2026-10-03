#!/bin/bash
set -euo pipefail

BASE_URL="https://tsukini22.github.io/{{NAME}}"
KEY_NAME="RPM-GPG-KEY-tsukini"
PACKAGE="{{NAME}}"
SUDO="sudo"
INSTALL=true
RC=true

MARK_BEGIN="# >>> {{NAME}} completion >>>"
MARK_END="# <<< {{NAME}} completion <<<"

# =========================
# Parse arguments
# =========================
usage() {
    echo "Usage: $0 [--no-sudo] [--pre] [--no-install] [--no-rc]"
    echo "  --no-sudo      Run without sudo (requires the script to be run as root already)"
    echo "  --pre          Install the pre-release/unstable package ({{NAME}}-pre)"
    echo "  --no-install   Only setup the mirror & gpg key, don't install the package"
    echo "  --no-rc        Don't edit ~/.zshrc & ~/.bashrc for the auto-completion"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-sudo)
            SUDO=""
            ;;
        --pre)
            PACKAGE="{{NAME}}-pre"
            ;;
        --no-install)
            INSTALL=false
            ;;
        --no-rc)
            RC=false
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Error: unknown argument '$arg'" >&2
            usage
            ;;
    esac
done

if [ -z "$SUDO" ] && [ "$(id -u)" -ne 0 ]; then
    echo "Error: --no-sudo requires the script to be run as root." >&2
    exit 1
fi

# =========================
# Requirements
# =========================
require() {
    for cmd in "$@"; do
        if ! command -v "$cmd" > /dev/null 2>&1; then
            echo "Error: '$cmd' is required but not installed." >&2
            exit 1
        fi
    done
}
require curl

# =========================
# Find the os
# =========================
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS_ID="${ID:-unknown}"
    OS_ID_LIKE="${ID_LIKE:-}"
    OS_VERSION="${VERSION_ID:-unknown}"
else
    echo "Error: /etc/os-release not found, cannot detect OS." >&2
    exit 1
fi

is_rpm_based() {
    [[ "$OS_ID" =~ ^(fedora|rhel|centos|rocky|almalinux)$ ]] || [[ "$OS_ID_LIKE" =~ (fedora|rhel) ]]
}

is_deb_based() {
    [[ "$OS_ID" =~ ^(debian|ubuntu)$ ]] || [[ "$OS_ID_LIKE" =~ (debian|ubuntu) ]]
}

# =========================
# RPM
# =========================
install_rpm_repo() {
    echo "Detected RPM-based system (ID=$OS_ID). Setting up {{NAME}} repo..."

    # The mirror is only built for a fixed release (see: .github/workflows/build-packages.yml)
    if [ "$OS_ID" = "fedora" ] && ! curl -fsSL -o /dev/null "$BASE_URL/fedora/$OS_VERSION/$(uname -m)/repodata/repomd.xml"; then
        echo "Warning: no packages published for fedora $OS_VERSION ($(uname -m)), the installation will probably fail." >&2
    fi

    $SUDO curl -fsSL -o /etc/yum.repos.d/{{NAME}}.repo \
        "$BASE_URL/{{NAME}}.repo"

    $SUDO rpm --import "$BASE_URL/$KEY_NAME"

    echo "{{NAME}} repo installed."
}

install_rpm_package() {
    local pm="dnf"
    command -v dnf > /dev/null 2>&1 || pm="yum"

    echo "Installing $PACKAGE..."
    $SUDO "$pm" install -y "$PACKAGE"
}

# =========================
# DEB
# =========================
install_deb_repo() {
    echo "Detected DEB-based system (ID=$OS_ID). Setting up {{NAME}} repo..."
    require gpg dpkg

    $SUDO install -m 0755 -d /etc/apt/keyrings

    curl -fsSL "$BASE_URL/$KEY_NAME" \
        | gpg --dearmor | $SUDO tee /etc/apt/keyrings/{{NAME}}.gpg > /dev/null
    $SUDO chmod a+r /etc/apt/keyrings/{{NAME}}.gpg

    ARCH="$(dpkg --print-architecture)"

    echo "deb [arch=${ARCH} signed-by=/etc/apt/keyrings/{{NAME}}.gpg] ${BASE_URL}/debian stable main" \
        | $SUDO tee /etc/apt/sources.list.d/{{NAME}}.list > /dev/null

    $SUDO apt-get update

    echo "{{NAME}} repo installed."
}

install_deb_package() {
    echo "Installing $PACKAGE..."
    $SUDO apt-get install -y "$PACKAGE"
}

# =========================
# Shell rc (auto-completion)
# =========================
# The rc files belong to the user who launched the script (even through sudo)
TARGET_USER="${SUDO_USER:-$(id -un)}"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
TARGET_HOME="${TARGET_HOME:-$HOME}"

zsh_block() {
    cat <<'EOF'
# >>> {{NAME}} completion >>>
# Added by {{NAME}} setup.sh (remove this block to disable it)
# /usr/local: install from the sources (cmake), /usr: install from the packages
for _cf_dir in /usr/local/share/zsh/site-functions /usr/share/zsh/site-functions; do
    if [[ -r "$_cf_dir/_{{NAME}}" ]]; then
        (( ${fpath[(Ie)$_cf_dir]} )) || fpath=("$_cf_dir" $fpath)
        _cf_found=1
        break
    fi
done
if (( ${+_cf_found} )); then
    # compinit already done (oh-my-zsh, ...) -> only register the completion
    if (( ${+functions[compdef]} )); then
        autoload -Uz _{{NAME}} && compdef _{{NAME}} {{NAME}}
    else
        autoload -Uz compinit && compinit -i
    fi
fi
unset _cf_dir _cf_found
# <<< {{NAME}} completion <<<
EOF
}

bash_block() {
    cat <<'EOF'
# >>> {{NAME}} completion >>>
# Added by {{NAME}} setup.sh (remove this block to disable it)
# /usr/local: install from the sources (cmake), /usr: install from the packages
if [[ $- == *i* ]] && ! complete -p {{NAME}} > /dev/null 2>&1; then
    for _cf_file in /usr/local/share/bash-completion/completions/{{NAME}} /usr/share/bash-completion/completions/{{NAME}}; do
        if [[ -r "$_cf_file" ]]; then
            . "$_cf_file"
            break
        fi
    done
    unset _cf_file
fi
# <<< {{NAME}} completion <<<
EOF
}

# Replace (or add) the {{NAME}} block of a rc file
write_rc() {
    local file="$1" block="$2" tmp

    tmp="$(mktemp)"
    if [ -f "$file" ]; then
        # drop any previous block (the script can be launched multiple times) & the trailing empty lines
        sed "/^${MARK_BEGIN}\$/,/^${MARK_END}\$/d" "$file" \
            | awk '{line[NR] = $0} END {n = NR; while (n > 0 && line[n] == "") n--; for (i = 1; i <= n; i++) print line[i]}' > "$tmp"
    fi
    # keep one empty line between the user content and the block
    if [ -s "$tmp" ]; then echo >> "$tmp"; fi
    printf '%s\n' "$block" >> "$tmp"

    # keep the ownership/permission of the original file
    if [ -f "$file" ]; then
        cat "$tmp" > "$file"
    else
        cp "$tmp" "$file"
        chmod 0644 "$file"
    fi
    rm -f "$tmp"
    if [ "$(id -u)" -eq 0 ] && [ "$TARGET_USER" != "root" ]; then
        chown "$TARGET_USER": "$file"
    fi
    echo "Auto-completion setup in: $file"
}

setup_rc() {
    local zshrc="$TARGET_HOME/.zshrc"
    local bashrc="$TARGET_HOME/.bashrc"

    # respect ZDOTDIR only when the rc belongs to the current user
    if [ "$TARGET_USER" = "$(id -un)" ] && [ -n "${ZDOTDIR:-}" ]; then
        zshrc="$ZDOTDIR/.zshrc"
    fi

    # only setup the shells that are installed (or already configured)
    if command -v zsh > /dev/null 2>&1 || [ -f "$zshrc" ]; then
        write_rc "$zshrc" "$(zsh_block)"
    fi
    if command -v bash > /dev/null 2>&1 || [ -f "$bashrc" ]; then
        write_rc "$bashrc" "$(bash_block)"
    fi
    echo "Reload your shell to enable the auto-completion: exec \$SHELL"
}

# =========================
# Dispatch
# =========================
if is_rpm_based; then
    install_rpm_repo
    if $INSTALL; then install_rpm_package; fi
elif is_deb_based; then
    install_deb_repo
    if $INSTALL; then install_deb_package; fi
else
    echo "Error: unsupported distribution (ID=$OS_ID, ID_LIKE=$OS_ID_LIKE)." >&2
    echo "Supported: Fedora/RHEL-based (rpm) and Debian/Ubuntu-based (deb)." >&2
    exit 1
fi

if $RC; then setup_rc; fi

if $INSTALL; then
    echo "$PACKAGE installed. Next steps: {{NAME}} install-ollama, {{NAME}} pull <model>, {{NAME}} setup"
elif is_rpm_based; then
    echo "You can now run: $SUDO dnf install $PACKAGE"
else
    echo "You can now run: $SUDO apt install $PACKAGE"
fi
