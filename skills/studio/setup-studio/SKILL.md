---
name: setup-studio
description: Set up the creative studio of the user's computer - scans the tools there for audio (music, sound effects, voice), image, video, 3D (modeling, CAD) and simulation / VFX / rendering (programs, flatpaks, Python packages, MCP servers, claude.ai connectors, GPU), asks what to cover and whether the work is commercial, searches online for free tools without subscription for what is missing (license and commercial use checked, MCP servers too) and installs the accepted ones, then generates a /studio router and one studio-audio / image / video / 3d / vfx skill saying which installed tool to use for each task, where, how (MCP, command line, GUI) and why, with commands and license notes - in ~/.claude/skills or as a claude.ai personal skill; run again it refreshes them. Use whenever the user wants to set up, scan or refresh their creative / media / audio / video / 3D / VFX tools or MCP servers, or summons /setup-studio.
metadata:
  summary: 'Scans the creative tools of the computer (audio, image, video, 3D, simulation / VFX / rendering: programs, flatpaks, Python packages, MCP servers, claude.ai connectors, GPU), asks what to cover and whether the work is commercial, finds free tools without subscription for what is missing (license checked, MCP servers too) and installs the accepted ones, then generates a `/studio` router and one `studio-<domain>` skill per domain (which tool for each task, where, how, why, commands, licenses) in `~/.claude/skills` or as a claude.ai personal skill; run again to refresh.'
---

# Setup studio: the creative tools of this computer, as skills

`SKILL_DIR` = the directory of this file. Like `setup-writing-style`, this skill **builds other skills**: it finds what
the computer has for creative work, fills the gaps with free tools the user accepts, then writes a `/studio` router and
one skill per domain that says, for each task, **which tool, where, how and why**.

| Domain | Generated skill | Roles (`reference/catalog.json`) |
|---|---|---|
| Audio: music, sound effects, voice | `studio-audio` | edit, compose, synth, sfx, process, stems, voice, transcribe, generate, assets |
| Image: 2D, retouching, painting, vector, generation | `studio-image` | edit, paint, vector, photo, batch, optimize, generate, upscale, cutout, vectorize, assets |
| Video: editing, effects, motion, generation | `studio-video` | edit, cli, compositing, motion, record, subtitles, transcode, generate, download |
| 3D: modeling, CAD, texturing | `studio-3d` | model, cad, mesh, texture, lowpoly, generate, realtime, assets |
| Simulation, VFX and rendering | `studio-vfx` | sim, render, compositing, cfd, physics, realtime |

The catalog lists ~70 free tools with their roles, detection, license, **commercial use** (yes / conditions / no),
install commands, MCP servers known, GPU needs and ready commands. It is the starting point, not the limit.

## 0. Where it runs
- **Claude Code on the user's computer** (a shell there): everything below. Work files in a temp folder
  (`WORK=$(mktemp -d)`, or the job's tmp folder).
- **claude.ai / no shell on the user's computer**: the sandbox is not their computer, never scan it. Ask what they
  have per domain (step 2, options taken from the catalog) and use the connectors of the session; then
  `inventory.py --declare <ids> --connectors <names>` makes the inventory (its hardware lines are the sandbox's: ask
  the GPU, put it in the notes) and step 6 saves one combined skill.
- `$HOME/.claude/skills/studio/reference/state.json` exists: **refresh** (step 7).

## 1. Scan
```bash
python3 SKILL_DIR/scripts/inventory.py --json "$WORK/inventory.json" --connectors <Name,Name>
claude mcp list                     # also the MCP servers of plugins, and whether each one answers
```
- `--connectors`: the claude.ai connectors of this session, read from the tool list (`mcp__claude_ai_<Name>__*`).
- The summary gives, per domain, what was found and the **roles nothing free covers**; the JSON has the paths,
  versions, flatpaks, MCP servers (never their environment or tokens), GPU / VRAM, RAM, free disk, installers.
- An MCP server or a program the catalog does not know but that looks creative: note it for step 2 (its domain).

## 2. Ask (`interactive-questions`; `clarify` for real doubts, `ideation` when the user wants proposals)
Never ask what the scan answered. Round 1, one popup:
1. **Domains** to cover (multi, all five recommended).
2. **Commercial use**: `Oui, travail commercial` (the tools marked *no* are left out) · `Non, usage perso` ·
   `Selon le projet` (asked each time, commercial-safe tools first). Saved as `commercial`: `yes` / `no` / `ask`.
3. **Installs allowed** (multi): system packages (root, the user's way: `SUDO_ASKPASS=... sudo -A dnf ...`), Flatpak
   (`--user`, no root), Python tools (`uv tool install`, isolated), MCP servers, AI models (big downloads), none.
4. **Local AI generation**: show what the GPU allows (VRAM: 4 GB = SD 1.5 / SDXL with offloading, Whisper small, no
   local video; 8 GB = FLUX, Stable Audio Open; 12 GB+ = video models) - yes / no / only the light ones.

Round 2, only where it matters: several installed tools for the same role and the choice changes the work (two video
editors, two DAWs): which one first (`prefer`, ex: `video.edit = kdenlive`). An unknown MCP server: its domain.

## 3. Fill the gaps (online search)
For each domain chosen and each missing role (and roles covered only by a connector spending credits):
- Start from the catalog candidates (free, commercial-compatible when needed, fit for the GPU and the free disk).
- **Search online** (WebSearch) for better or newer free options: `best free open source <role> linux <year>`,
  `<tool> license commercial use`, `<tool> MCP server`. Keep a tool only when checked **at its source** (official site,
  `LICENSE` of the repository): free, **no subscription** (freemium / credits said as such, never proposed as the main
  choice), installable on this system (dnf / Flatpak / pip / AppImage), released in the last two years. An MCP server:
  its repository read (license, activity, what it runs, local only, no paid API key unless the user wants it).
- Present them per domain in a QCM (multi): label = name, description = what for, license, **commercial yes / no /
  conditions**, how it installs and its size, GPU; the recommended first; `Rien pour ce domaine`. What is refused goes
  to `declined` (not proposed again at the next refresh unless the user asks).

## 4. Install (only what was accepted)
- Packaged in the distribution: dnf (root, the user's way); else `flatpak install --user -y flathub <id>`; Python
  tools `uv tool install <pkg>`; models in a folder the user names, the size said first. Check each name before
  (`dnf info`, `flatpak remote-info flathub <id>`), never `curl | sh` from an unknown site.
- MCP server: `claude mcp add -s user <name> -- <command>` (or `--transport http <name> <url>`), then
  `claude mcp list` until it answers; its own steps too (ex: `blender-mcp` needs its `addon.py` installed in Blender
  and *Connect* clicked). It works in the next session.
- After each install: its `--version` or a smoke run; a failure is reported as it is, never hidden. Then scan again
  (`inventory.py --json`).

## 5. Choices
Write `$WORK/choices.json` from the answers:
```json
{"commercial": "yes", "domains": ["audio", "image", "video", "3d", "vfx"],
 "prefer": {"video.edit": "kdenlive"}, "skip": [], "declined": ["houdini-apprentice"],
 "mcp": {"my-daw": "ardour", "other-server": "domain:audio"},
 "howto": {"kdenlive": "MCP kdenlive: open_project, add_clip, render ... (read with ToolSearch)"},
 "extra": [{"id": "x", "name": "X", "kind": "cli", "domains": {"audio": ["sfx"]}, "where": ["bin:/usr/bin/x"],
            "license": "MIT", "commercial": "yes", "why": "...", "recipes": [["what", "command"]]}],
 "notes": {"audio": "- a first note of the user"}}
```
`howto` = what the scan cannot know (the real tools of an MCP server: load them with ToolSearch and name the main
ones); `extra` = a tool found online and installed that is not in the catalog (its license checked).

## 6. Generate and save
```bash
python3 SKILL_DIR/scripts/generate.py --inventory "$WORK/inventory.json" --choices "$WORK/choices.json"
```
Writes `$HOME/.claude/skills/studio` (router `/studio`, its `state.json` for the refresh) and `studio-<domain>` per
domain, from `templates/router.md` and `templates/domain.md`. It never overwrites a skill it did not generate (`--force` only after
asking), keeps the notes between the `studio:keep` markers, removes the generated domains the user dropped;
`--dry-run` shows what it would write. Then **read every generated file** and fix what the scan could not know (a
wrong pick, a tool used another way here): change `choices.json` and generate again rather than editing by hand.

Save the way `setup-writing-style` does, by what this session has:
1. **Claude Code**: the files above, nothing else to do (new skills are listed from the next session at the latest).
2. **claude.ai with a `save_skill` tool**: `generate.py --single --out "$WORK/single"` (one skill `studio` with every
   domain), then `save_skill` with `name: "studio"`, its description and the body (everything below the frontmatter);
   `overwrite: true` only when a `studio` skill is already listed (its exact name). One skill: the quota is spared.
3. **claude.ai / Cowork without it**: `generate.py --single --zip "$WORK/zip"` and give `studio.zip` to upload
   (Customize > Skills > Upload); from Claude Code too when the user wants the same studio on claude.ai (the programs
   then run on their computer, the connectors in claude.ai). The `state.json` is never put in a zip.

## 7. Refresh (run again)
```bash
python3 SKILL_DIR/scripts/inventory.py --json "$WORK/inventory.json" --connectors <Name,Name> \
    --compare "$HOME/.claude/skills/studio/reference/state.json"
```
Start from the `choices` of `state.json`: ask only about what changed (tools added or removed, new missing roles, a
domain to add); `declined` stays declined. Then step 6 again (the notes are kept).

## 8. Report
In the user's language, short: per domain the tools used (MCP / command line / GUI), what was installed now, what is
still missing or declined, the licenses that limit commercial use, where the skills are (or the save still pending),
and how to use them: `/studio <request>` or simply the request (the domain skills load by themselves).

## Rules
- Nothing installed, no MCP server added, no model downloaded without a yes; nothing paid or with a subscription
  proposed as a solution (only said when nothing free exists).
- A license is never guessed: unknown = `check`, said to the user.
- No secret in the files: the MCP environment, headers and URL queries are never read into them.
- The generated skills describe this computer only: never committed to a repository.
