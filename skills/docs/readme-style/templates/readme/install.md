# Install

## Setup script
Each step of `setup.sh`, in order:
1. adds the package mirror of `<repo>` (`dnf` repo file or `apt` source)
2. imports the GPG key that signs the packages and the metadata
3. installs `<project>`

| Argument | Effect |
| -------- | ------ |
| `--no-sudo` | No `sudo` (run it as root) |

## Known errors

> [!WARNING]
> `invalid hash` from `dnf` / `apt`: clear its cache, then run `setup.sh` again.
