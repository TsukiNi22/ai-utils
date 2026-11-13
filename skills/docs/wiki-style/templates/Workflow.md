## Workflows
- The workflow `{{Dispatch (CI/CD)}}` runs on every push ({{branch / tag filters}}) and decides what to trigger from the ref / commit message
- The workflow `{{Unit Tests (CI)}}` builds the unit tests and executes them

> [!IMPORTANT]
> {{dependency between the workflows, what stops the pipeline}}

> [!TIP]
> A commit containing the string `[ignore]` (preferably in the description) skips the whole CI.

## Pre-Release (unstable)
> Depends on `{{Unit Tests (CI)}}` result

{{events that trigger it: tag `vx.x.x-<suffix>`, `[build]` in the commit}}

## Release (stable)
> Depends on `{{Unit Tests (CI)}}` result

{{event: tag `vx.x.x`}}

## Publication
{{where the packages / docs are published (gh-pages), signing, concurrency}}
