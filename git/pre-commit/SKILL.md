---
name: pre-commit
description: Set up the xstyle pre-commit hook of a git repository in Tsukini's way - checks the style of what is committed with xstyle (only the staged lines by default), refuses the commit on a major / unforgivable issue, warns on minor ones, only warns when xstyle is not installed - every option asked first with several choices (scope, fail / warn levels, safe auto fixes, missing xstyle, output, languages / excludes / ignored rules, local or shared .githooks), the needed tools checked and installed on request (xstyle, cmake, clang++, libutils), an existing hook kept and chained, and a throwaway demo repository to try it. Use whenever the user wants a pre-commit hook, a git hook checking the style, to block commits with style errors, or to change / remove that hook.
---

# xstyle pre-commit hook

`SKILL_DIR` = the directory of this file.
- `templates/pre-commit`: the hook (bash, config block at the top, readable by the user).
- `scripts/install_hook.sh install|remove|status [options]` (`--help`), also `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- hook [command] [options]`
  run from the repository.
- `scripts/demo_repo.sh [dir] [options]`: throwaway repository (`/tmp/xstyle-hook-demo`) with one file per verdict
  and `TRY.md` listing the scenarios.

The hook runs **locally** at each `git commit` (before the commit exists), never on GitHub: the remote check is the
style job of `ci-cd` (`templates/style.yml`). Both are complementary (local = fast feedback, skippable; CI = enforced).

## Behavior (defaults)
| Case | Result |
|---|---|
| xstyle not installed | warning `xstyle not found`, commit accepted (`--missing warn`) |
| unforgivable / major issue | issues listed, **commit refused** (`--fail-on major`) |
| minor issue only | issues listed + warning, commit accepted (`--warn-on minor`) |
| negligible only / clean | silent, commit accepted |
| xstyle crashes | warning, commit accepted (never blocks on a tool error) |
Skip once: `git commit --no-verify` or `XSTYLE_HOOK=0 git commit ...`. Fix: `xstyle --fix --staged` then `git add -u`.
Output: human (colors, source) in a terminal, `--rtk` automatically under Claude Code (`CLAUDECODE`) or `XSTYLE_RTK=1`.

## 1. Check the context (in parallel)
- `git rev-parse --show-toplevel` (not a repository: propose `git init`), an existing hook
  (`install_hook.sh status`), `git config core.hooksPath`, a `.pre-commit-config.yaml` (framework pre-commit: propose
  a `local` hook `xstyle --staged --fail-on major` in it instead, ask).
- Tools: `command -v xstyle` (`xstyle --version`), else what building it needs: `cmake`, `clang++`, libutils
  (`pkg-config --exists utils` or `/usr/local/include/utils`).
- Languages of the repository (`git ls-files | sed 's/.*\.//' | sort | uniq -c | sort -rn | head`) and the build /
  generated / vendored folders (`build`, `_deps`, `third_party`...) to propose in the questions.

## 2. Ask every option (AskUserQuestion, French, recommended first)
Ask all of them, even when a default exists (only skip what the request already says). Two calls of up to 4 questions:

**Call 1**
- **Bloquer le commit à partir de** (`--fail-on`): `major (Recommandé: major + unforgivable)` · `unforgivable seulement` ·
  `minor (strict)` · `jamais (avertir seulement)`.
- **Avertir à partir de** (`--warn-on`): `minor (Recommandé)` · `negligible (tout)` · `aucun avertissement`.
- **Portée** (`--scope`): `Lignes indexées seulement (Recommandé: n'impose pas de corriger le code existant)` ·
  `Fichiers indexés entiers` · `Tout le projet (lent)`.
- **xstyle absent** (`--missing`): `Avertir et laisser passer (Recommandé)` · `Refuser le commit` · `Ignorer en silence`.

**Call 2**
- **Corrections automatiques** (`--fix`): `Non, seulement signaler (Recommandé)` · `Oui, les corrections sûres des
  fichiers entièrement indexés (ré-ajoutés au commit)`.
- **Installation** (`--shared` / `--link`): `Locale à ce clone (.git/hooks, Recommandé)` · `Partagée et versionnée
  (.githooks + core.hooksPath, toute l'équipe)` · `Lien vers le template (suit les mises à jour des skills)`.
- **Filtres** (multiSelect, `--extra`): `Exclure les dossiers générés (<the folders found>)` · `Seulement <top
  languages>` · `Ignorer des règles (lesquelles ?)` · `Aucun`.
- **Sortie** (`--output`): `Auto (Recommandé: lisible au terminal, compacte sous Claude Code)` · `Toujours lisible` ·
  `Toujours compacte (--rtk)`.
Then, when xstyle is missing (only if): **Installer xstyle ?** `Oui, via le setup.sh des skills (Recommandé)` (`curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- install xstyle`) ·
`Non, le hook avertira seulement`; libutils missing for the build: the `libutils-install` skill (ask first), cmake /
clang++ missing: give the `dnf` / `apt` command (root through `SUDO_ASKPASS=~/.local/bin/sudo-askpass sudo -A`, ask).
Last: **Dépôt de démonstration ?** `Oui, /tmp/xstyle-hook-demo pour essayer` · `Non`.

## 3. Install
```bash
bash SKILL_DIR/scripts/install_hook.sh install --repo <root> --fail-on major --warn-on minor --scope staged \
    --missing warn --fix none --output auto [--extra "-e build,_deps -l cpp,py -i CPP-AUTO"] [--shared | --link]
```
- A hook already there (not xstyle's) is kept as `pre-commit.local` and run first (`--force` drops it: ask).
- A reinstall starts from the defaults + the given options (the config block is rewritten); `--link` keeps the
  template as is (no option).
- `--shared`: commit `.githooks/` (with `git-conventions`, only if the user asks for the commit) and tell the team
  command `git config core.hooksPath .githooks`.
- Demo: `bash SKILL_DIR/scripts/demo_repo.sh /tmp/xstyle-hook-demo <same options>`, then point to its `TRY.md`.

## 4. Verify and report
`install_hook.sh status --repo <root>` (config shown), then one real run without committing:
`git stash -k` is never used; run the hook directly: `.git/hooks/pre-commit; echo $?` (it checks the staged lines; with
nothing staged it exits 0). Report: where the hook is, the chosen options, what is refused / warned, how to skip it,
how to change it (`curl -fsSL https://raw.githubusercontent.com/TsukiNi22/skills/main/setup.sh | bash -s -- hook --fail-on minor`) or remove it (`... | bash -s -- hook remove`):
the commands given to the user always use the `curl` form (the user may not have the skills repository locally).
