# Routers

Skills invoked **by hand** (`/dev`, `/cpp`...): they read the request, the project and the language, then load
the right skills by themselves. `/dev` is enough most of the time; the others narrow it to one domain. Installing a
router installs every skill it can load.

## Skills
<!-- generated:skills -->
| Router | What it does | Installs with it |
|---|---|---|
| [`/dev`](dev/SKILL.md) | Global development entry point: looks at the request, the project and the language, then loads the right skills of every category (and the other routers). | 32 skills |
| [`/audit`](audit/SKILL.md) | Asks which audits to run (checklist: `audit-bugs`, `audit-security`, `audit-quality`, `audit-deps`, `legal-check`, `coverage`, `benchmark`; coverage and benchmark checked and run automatically) and the report format, runs them and writes a summary report in `audit/`. | 10 skills |
| [`/cpp`](cpp/SKILL.md) | Loads `cpp-project`, `cpp-class`, `cpp-style`, `cpp-comments`, `cmake-style`, `cpp-tests`, `audit-bugs`, `audit-quality`, `libutils`, `libutils-exception`, `libutils-install`, `libutils-setup`, `coverage`, `benchmark`, `ci-cd`. | 18 skills |
| [`/doc`](doc/SKILL.md) | Loads `readme-style`, `html-doc`, `html-style`, `explain-doc`, `protocol-doc`, `better-html-doc`, `better-html-style`, `better-explain-doc`, `better-protocol-doc`, `report`, `docs-update`, `git-conventions` (CHANGELOG), `comments`, `cpp-comments`. | 16 skills |
| [`/git`](git/SKILL.md) | Loads `git-conventions`, `git-commit`, `ci-cd`, `pre-commit`. | 4 skills |
| [`/legal`](legal/SKILL.md) | Loads `license`, `audit-deps`, `legal-check`, `legal-setup`. | 5 skills |
| [`/python`](python/SKILL.md) | Loads `python-project`, `python-class`, `python-style`, `python-comments`, `tests`, `ci-cd`, `benchmark`. | 9 skills |
| [`/setup-project`](setup-project/SKILL.md) | A whole new project in order: architecture (`cpp-project` / `python-project` / `coding-style`), base unit tests (`cpp-tests` / `tests`), `license`, `git-conventions` + `pre-commit`, `ci-cd`, `readme-style`, then a quick architecture audit. | 15 skills |
| [`/style`](style/SKILL.md) | Loads `cpp-style`, `python-style`, `python-comments`, `coding-style`, `cpp-comments`, `comments`, `cmake-style` (by file type), `audit-quality`. | 9 skills |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- */dev add a `Timer` class in `utils::system` and its tests*
- */setup-project a new C++ shared library `netio` with packages and a CI*
- */audit (asks which audits to run, then one summary report)*
- */doc update the README and the wiki after the last release*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
