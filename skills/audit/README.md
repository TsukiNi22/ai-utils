# Audit skills

Audits of a project, each one delivered as a report (the [`report`](../docs/report/SKILL.md) skill, in `audit/`
at the root of the project): bugs and UB, security (fixed pass after pass until a clean one), quality, dependencies
(licenses, vulnerabilities), performance. [`/audit`](../routers/audit/SKILL.md) runs several at once.

## Skills
<!-- generated:skills -->
| Skill | What it does | Installs with it |
|---|---|---|
| [`audit-bugs`](audit-bugs/SKILL.md) | Bug / UB audit loop (sanitizers, tests, static tools, code review, verification of each finding), single agent or subagents on request, fixes only after confirmation. | [`cpp-style`](../cpp/cpp-style/SKILL.md), [`cpp-comments`](../cpp/cpp-comments/SKILL.md), [`libutils-exception`](../libutils/libutils-exception/SKILL.md), [`report`](../docs/report/SKILL.md), [`audit-deps`](../audit/audit-deps/SKILL.md), [`libutils`](../libutils/libutils/SKILL.md) |
| [`audit-deps`](audit-deps/SKILL.md) | Dependencies (direct + transitive): licenses to credit, copyleft / non-commercial / paid / unknown ones, THIRD_PARTY_NOTICES generation, and known vulnerabilities / compromised versions (OSV, dnf, GitHub advisories), as a PDF report. | [`report`](../docs/report/SKILL.md), [`license`](../legal/license/SKILL.md) |
| [`audit-quality`](audit-quality/SKILL.md) | Cleanliness / conventions audit of a project (`scripts/collect.py` metrics + review against the style skills) delivered as a PDF report. | [`report`](../docs/report/SKILL.md), [`cpp-style`](../cpp/cpp-style/SKILL.md), [`cpp-comments`](../cpp/cpp-comments/SKILL.md), [`comments`](../style/comments/SKILL.md), [`cpp-class`](../cpp/cpp-class/SKILL.md), [`cmake-style`](../cpp/cmake-style/SKILL.md), [`git-conventions`](../git/git-conventions/SKILL.md), [`readme-style`](../docs/readme-style/SKILL.md), [`cpp-tests`](../tests/cpp-tests/SKILL.md), [`license`](../legal/license/SKILL.md), [`audit-deps`](../audit/audit-deps/SKILL.md) |
| [`audit-security`](audit-security/SKILL.md) | Security / memory-safety audit of the whole project in passes back to back (sanitizers, dependencies, static tools, fuzzing, attacks on the running program, a review of memory, DoS, injection, auth, secrets, crypto, files, network), each finding verified and fixed at once, a new pass while the last one still found something; a PDF report of what was fixed. | [`audit-bugs`](../audit/audit-bugs/SKILL.md), [`audit-deps`](../audit/audit-deps/SKILL.md), [`report`](../docs/report/SKILL.md), [`cpp-style`](../cpp/cpp-style/SKILL.md), [`coding-style`](../style/coding-style/SKILL.md), [`comments`](../style/comments/SKILL.md) |
| [`benchmark`](benchmark/SKILL.md) | Profiling of the current program (perf stat / record, hyperfine, callgrind, heaptrack, flame graph; missing tools installed only after a yes): hot functions, why they are slow, quick wins vs complex changes with their gain, PDF report, optional before / after. | [`report`](../docs/report/SKILL.md) |
<!-- /generated:skills -->

The *Installs with it* column comes from each `requires.txt`: `./setup.sh install <skill>` installs them too.

## Examples
- *audit the bugs of this project with the sanitizers*
- *security audit of the whole project*
- *which dependencies have a vulnerability or a license to credit?*
- *profile the program and tell me the quick wins*

[All the categories](../README.md) · [Installation](../../INSTALL.md)
