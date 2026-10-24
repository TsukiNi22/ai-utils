---
name: audit-security
description: Security and memory-safety audit of the whole project in passes run back to back - memory errors that can crash it or be exploited (overflows, use after free, leaks a client can trigger, races), injection (shell, SQL, path, format strings), authentication / authorization holes, secrets, crypto, network exposure, files and permissions, untrusted input, known vulnerable dependencies - each finding verified with evidence and fixed at once, then a new full pass, again and again while the last pass still found something; ends with a PDF report (report skill) of everything found and fixed. Use whenever the user asks for a security audit, a vulnerability / flaw / exploit hunt, a hacking-proof check, a memory-error audit that must be fixed until clean, or "passe sur tout le projet pour les failles".
---

# Security audit (until a clean pass)

`<name>` (a skill) = the folder of that skill: `~/.claude/skills/<name>` (setup.sh) or `${CLAUDE_SKILL_DIR}/../../*/<name>` (plugin of the marketplace).

What the user wants: the **whole project** gone through for every flaw that crashes it, leaks, or lets someone in, **fixed
as found**, and the passes **started again at once** as long as the last one still found something - insisting, not one
pass and a list. The end: a **PDF only** report of what was found and fixed.

## 1. Before the first pass
- Scope: the whole repository (`git rev-parse --show-toplevel`, or the current folder), unless the request names a part.
- What it is: languages, build system, how it runs (binary, server, library, web page, mobile app), **what reaches it from
  outside** (network ports, HTTP routes, sockets, files it reads, command line, environment, IPC, plugins) - the attack
  surface, written down once and kept up to date across the passes.
- Its tests and how to build them: a pass ends with the build and the tests green.
- Fixes are part of this skill (the user asked for them): no question for each one. Ask (one AskUserQuestion call,
  French) only when a fix changes a behaviour the user relies on (a protocol, an option, a default, data on disk) or
  needs an architectural choice; the rest is fixed directly. Never commit or push (the user's git rules), never delete data.
- Mode: one agent by default. Subagents in parallel (one per area of the attack surface) only when the user asks or
  agrees (propose it with the number of agents and why); never a fork agent.

## 2. One pass (always the whole scope)
A pass covers everything, every time (a fix can open a new hole elsewhere): the tooling, then the review, then the
verification, then the fixes.

### Tooling
- **Sanitizers** (C / C++): `bash <audit-bugs>/scripts/sanitize_build.sh <project> all [--run "<cmd>"]` (a copy in
  /tmp, ASan + UBSan then TSan, the tests run): every report is a lead.
- **Dependencies**: `python3 <audit-deps>/scripts/vulns.py <project> --json /tmp/vulns.json --md /tmp/vulns.md` (known
  vulnerabilities, compromised versions, transitive too): a finding when the vulnerable code is reachable.
- **Static tools** when installed (ask before installing): `clang-tidy` (`bugprone-*,cert-*,clang-analyzer-security*,
  concurrency-*`), `cppcheck --enable=all`, `semgrep --config auto`, `bandit` (Python), `shellcheck`, `gosec`, `cargo audit`,
  `npm audit`, `gitleaks` / `trufflehog` (secrets in the files and the history). `xstyle --rtk` when installed (the rules
  that hide bugs: casts, unchecked returns, silent excepts).
- **Fuzz the parsers** that read outside input when a harness is cheap (libFuzzer / AFL on a C/C++ parser, a short
  `hypothesis` run in Python): a few minutes per parser, crashes are findings.
- **Run it**: start the program (or its server) in an isolated environment (its own config / data / port / socket) and
  try what an attacker would: oversized inputs, malformed frames, path traversal (`../`), shell metacharacters, no auth,
  replayed or forged tokens, concurrent requests, a client that disconnects mid-request.

### Review (read the code, every item searched explicitly)
| Category | Look for |
|---|---|
| Memory | buffer / heap / stack overflows, out-of-bounds reads (`[]`, `memcpy`, `strcpy`, `sprintf`, pointer arithmetic), use after free / move, double free, dangling references and views, uninitialized reads, integer overflow on sizes and lengths, signed / unsigned conversions, unbounded allocations from a length an outsider gives |
| Crash / DoS | unchecked syscalls and returns, exceptions escaping threads or callbacks (the process ended), unbounded loops, recursion or queues an input controls, resources never released (fds, threads, sockets), timeouts missing, a lock held on a slow call, a panic / abort reachable from input |
| Injection | shell (`system`, `popen`, `sh -c` with data), SQL, path traversal and symlink following, format strings, template / HTML / JS injection (XSS), header and log injection, deserialization of untrusted data, regex DoS |
| Auth & access | missing or bypassable authentication, authorization checked on the client only, a default password / token, tokens predictable or never expired, session fixation, CSRF, CORS / origin checks, privilege of what runs (root, setuid), a local socket or file other users can open |
| Secrets & crypto | keys, tokens, passwords in code, logs, errors, URLs, crash dumps or the git history; weak or home-made crypto, no TLS / certificate checks skipped, random numbers not cryptographic for secrets, comparisons not constant-time |
| Files & system | world-readable / writable files and folders (`umask`, `chmod`), temp files with fixed names, TOCTOU, unsafe `PATH` lookups, environment trusted blindly, untrusted downloads executed, update / install steps without verification |
| Network | listening on all interfaces when local is enough, no rate limit, no size limits on requests / frames, trust in `Host` / `X-Forwarded-*`, open redirects, SSRF |
| Concurrency | data races on shared state, check-then-act on files or maps, signals handlers doing too much |
Other languages: the same categories adapted (`eval`, `pickle`, `subprocess(shell=True)`, prototype pollution...).

### Verification (every finding, before fixing)
Prove it: a reproduction (a test, a request, an input, a sanitizer report) or the exact code path from the outside input
to the flaw. A finding without evidence is dropped (said in the report as a false positive with its reason). Rank it:
critical (remote code execution, auth bypass, secrets leaked, crash from the network), high, medium, low.

### Fixes (in the same pass)
- The root cause, minimal, in the project's style (`cpp-style`, `coding-style`, `comments`); never a check that hides the
  symptom only.
- A regression test for each fix when the project has tests (the reproduction becomes the test).
- Then rebuild and run every test: a fix that breaks something is reworked before the pass ends.

## 3. Insist: the next pass at once
- After a pass that **found and fixed anything**: start the next full pass right away, without asking (the fixed code is
  read again, and what the fixes changed is in the scope).
- Stop only after a pass that finds **nothing new** (its tooling clean, its review empty); then one last confirmation run
  of the tests and the sanitizers.
- A finding that cannot be fixed now (a choice for the user, a dependency without a fixed version): listed as open,
  it does not keep the loop going by itself.
- Safety cap: 10 passes; reaching it is said plainly in the report with what still came up.
- Between passes, a one-line status in the chat (pass n: found x, fixed y), nothing more.

## 4. Report (PDF only)
Through the `report` skill, format **PDF only** (no question about it): `audit/<YYYY-MM-DD>-security.pdf` at the root of
the repository (or of the current folder), never `docs/`; English unless asked otherwise (`md2pdf.py ... --format pdf`,
the Markdown source written in /tmp only).
- **Summary**: passes run (and why it stopped), findings per severity, fixed / open / false positives, the commit audited.
- **Attack surface**: what reaches the project from outside and how each entry is now protected.
- **Fixed**: per finding (most severe first) - `file:line`, category, severity, what an attacker could do, the evidence /
  reproduction, the fix, the regression test, the pass it was found in.
- **Open**: what was not fixed and why, the decision needed.
- **Tools**: what ran each pass, what was missing (and the command to install it).
In the chat: the verdict (counts per severity, passes) and the path of the PDF only.
