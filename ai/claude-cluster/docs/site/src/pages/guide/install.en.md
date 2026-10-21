+++
title = "Install"
summary = "Dependencies, install script, build from source, the Android app and the first checks."
section = "guide"
order = 10
keywords = "rpm deb dnf apt fedora debian ubuntu setup.sh cmake build apk android completion"
+++

claude-cluster is one C++20 program (`claude-cluster`) plus a small wrapper for heavy commands (`cluster-run`). Claude Code is the default agent: install and log in to `claude` first, claude-cluster drives it.

## Requirements {#requirements}

| What | Version | Fedora (`dnf`) | Debian / Ubuntu (`apt`) |
|---|---|---|---|
| clang++, CMake | C++20, 3.20 or newer | `clang cmake` | `clang cmake` |
| [libutils](https://github.com/TsukiNi22/libutils) | 4.0.0 or newer | `libutils` | `libutils` |
| nlohmann/json | 3.10 or newer | `json-devel` | `nlohmann-json3-dev` |
| toml++ | 3.0 or newer | `tomlplusplus-devel` | `libtomlplusplus-dev` |
| OpenSSL (optional: https of the web access, microphone of the web page) | any | `openssl-devel` | `libssl-dev` |
| Qt6 Widgets (optional: the window) | 6.2 or newer | `qt6-qtbase-devel` | `qt6-base-dev` |
| chafa, ImageMagick (optional: images in the terminal) | any | `chafa ImageMagick` | `chafa imagemagick` |
| an agent | | `claude` (default), `ollama`, `qwen`, `opencode`, `codex` | |

FTXUI (the terminal interface) is fetched by CMake at a fixed version: the first configure needs network access. Without Qt6 the window is not built (`--gui` says so) and the terminal interface stays. The voice uses the optional local `voice-listen` / `voice-say` tools.

> [!NOTE]
> The package names of libutils depend on the repository you installed it from; the `libutils-install` skill of the repository picks the right method for your system.

## Fedora and Debian {#distros}

The one-line way builds and installs for your user (no root):

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install claude-cluster
```

From a clone of the repository:

```bash
git clone https://github.com/TsukiNi22/ai-utils.git
cd ai-utils
./setup.sh install claude-cluster
./setup.sh remove claude-cluster
```

The binary goes to `~/.local/bin`, with the completion for bash, zsh and fish. When the Android SDK is present the same command also builds the phone app (see below).

> [!SOON]
> Ready-made RPM and DEB packages are not published yet. Until then the script above is the supported way on Fedora and on Debian / Ubuntu.

## From source {#source}

```bash
cmake -S ai/claude-cluster -B build -DCMAKE_BUILD_TYPE=Optimized
cmake --build build --parallel
cmake --install build --prefix ~/.local
```

Build modes are `Debug`, `Asan` and `Optimized` (the default). Options: `-DCLUSTER_GUI=OFF` skips the Qt6 window, `-DCLUSTER_TESTS=ON` builds the unit test of the line editor. Many sessions compiling at once saturate a computer: build with `nice -n 19` and a limited `--parallel`.

## The Android app {#apk}

The phone app is a thin shell around the web page of your own computer. It is built by `android/build.sh` with the Android SDK alone (no Gradle) and signed with a key made once in `~/.config/claude-cluster/android/release.jks`: updates need the same key, keep a copy.

Once the web access is open, the page offers the APK to an Android browser at `/app.apk`. See [Notifications and the phone app](page:notifications-apk) for what it does and [Downloads](page:downloads) for the file.

## Check it works {#check}

```bash
claude-cluster --version
claude-cluster            # terminal interface in a terminal, else the window
claude-cluster serve      # no interface: driven by the commands and the MCP server
```

Next: [your first session](page:first-session). If something fails, see [Troubleshooting](page:troubleshooting).
