# Workflows

| Workflow | Runs on | Does |
| -------- | ------- | ---- |
| `Dispatch (CI/CD)` | each push to `main`, each `v*` tag | picks the jobs from the ref / commit message |
| `Build - Packages (CI/CD)` | a pre-release or a release | builds the RPM / DEB, publishes them |
| `Unit tests (CI)` | each push | builds and runs the tests |

## Pre-release (channel `unstable`)
- a tag `vX.Y.Z-pre`
- or a commit with `[build]`, best in its description

## Release (channel `stable`)
- a tag `vX.Y.Z` only
