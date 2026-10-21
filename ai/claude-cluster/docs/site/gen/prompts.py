#!/usr/bin/env python3
# =============================================================================
# prompts.py - the page of the system prompts: the prose, and the full texts of templates/global.md, sessions.md and
#              rendering.md read from the sources at build time (never copied by hand, so they cannot drift)
# =============================================================================

import html

FILES = (("global.md", "global"), ("sessions.md", "sessions"), ("rendering.md", "rendering"))


def text_of(ctx, name: str) -> str:
    return (ctx.code_root / "templates" / name).read_text()


def blocks(ctx, lang: str) -> str:
    en = lang == "en"
    out = [":::html"]
    for name, _ in FILES:
        body = text_of(ctx, name)
        summary = ("%s - full text (%d lines, %d KB)" if en else "%s - texte complet (%d lignes, %d Ko)") % (name, body.count("\n") + 1, max(1, round(len(body.encode()) / 1024)))
        out.append('<details><summary>%s</summary><pre data-title="%s"><code class="language-markdown">%s</code></pre></details>' % (html.escape(summary), name, html.escape(body, quote=False)))
    out.append(":::")
    return "\n".join(out)


def md(ctx, lang: str) -> str:
    en = lang == "en"
    t = blocks(ctx, lang)
    if en:
        return """The model of a session only knows what its prompt tells it. claude-cluster gives two default **system prompts**, and appends a third text to both: they carry the rules that keep the cluster safe and cheap, and the vocabulary of its tools.

| Prompt | Source in the repository | Live file | Given to |
|---|---|---|---|
| global session | `templates/global.md` | `~/.config/claude-cluster/global.md` | the global session, added to your global `CLAUDE.md` |
| sub-sessions | `templates/sessions.md` | `~/.config/claude-cluster/sessions.md` | every sub-session of the claude driver |
| rendering notes | `templates/rendering.md` | none: always taken from the binary | both, appended after the prompt |

## The global session prompt {#global}

It tells the model what it is (a dispatcher whose only levers are the `claude-cluster` MCP tools) and why it must behave as one:

- **Toolbox**: every MCP tool by family and *when* to use it (sessions, callbacks, tasks and schedules, orchestrator jobs, displays and pins, settings), so it picks the cheap, right tool. See the [MCP tools](page:mcp-tools).
- **Dispatch first**: a request that needs research, code or several commands goes to the sub-session of that topic: faster for you and it keeps the global context small. A doubt on the routing is a question to you (a popup), not a guess.
- **Autonomy**: what you ask explicitly is done without asking again; what it decides alone is proposed first.
- **One wake-up per wait**: results come back as `[reply from ...]`, `[group fN ...]` or `[job jN ...]` messages, so it ends its turn instead of polling; several projects are one `session_fanout`, answered once and merged.
- **The map of the computer** (`machine.md`) and the **work method** and **model policy** you set, which it must follow.
- **Few tokens between sessions**: terse prompts, quiet commands, like `rtk`.

## The sub-session prompt {#sessions}

- **Answer format**: a prompt that starts with `[from the global session]` is read by a model, so the answer must be compact `key: value` lines (facts, `file:line`, counts, no prose or Markdown for the eye); a prompt without the mark comes from you and gets a normal, readable answer. Group tasks and `[questions from ...]` are answered the same compact way.
- **Dangerous shell aliases**: a shell alias may replace a common command with something harmful (`git` or `vim` aliased to a screen lock), so it checks `alias` / `type` and calls `command <cmd>` or `\\<cmd>`.
- **Never kill claude-cluster**: no kill, restart or signal of its core, interfaces, service or the `claude` processes of other sessions unless you asked in this conversation; otherwise it asks first with a choice popup. To test it starts a separate instance and signals only its exact PID.
- **Heavy load**: builds, whole test suites, containers and local models go through the orchestrator (`cluster-run -- <command>`, or `heavy_run` / `jobs_add` when allowed), in the low profile, never several parallel builds by hand. See the [orchestrator](page:orchestrator).
- **`rtk`** for compact command output when installed, else it offers to set it up.
- **A missing permission is asked by popup** (`AskUserQuestion`), never by a guess or a workaround.
- **How its answers are shown** and how to call back the global session.

## The rendering notes {#rendering}

`rendering.md` is not editable: it is taken from the binary at each start and appended to both prompts. It says how answers are drawn in the terminal, window and web page (code blocks, tables, headings) and, for the global session only, the catalogue of display blocks of `session_display`.

## Who may rewrite them {#rewrite}

Two protection settings, both in `[sessions]`: `rewrite_global_prompt` (off by default: the global session may rewrite **its own** prompt) and `rewrite_sessions_prompt` (on by default: it may rewrite the **sub-sessions** prompt). With a switch off, `config_write` refuses and the global session gives you the text to apply yourself. A prompt injected once and rewritten into a prompt would stay for good, which is why the first one is off. See [Security](page:security#global).

## Where the live files are {#files}

`~/.config/claude-cluster/global.md` and `sessions.md` are written once at the first setup and are yours to edit (settings *Global session > System prompt*, or the palette: the session restarts on the same conversation). The binary never refreshes them behind your back; `config_files` shows them and `cluster_info` the details.

## Updating the defaults {#updates}

> [!PLANNED]
> The update flow below is being written (the session `prompt-defaults`) and is not in the current version yet; names may change.

The first setup writes the defaults. When a new version changes them, you are asked what to do with your copy: **Replace**, **Keep**, **Show the differences** or **Merge**. The setting `[prompts] update` chooses the default behaviour: `ask`, `replace_unedited` (replace the files you never edited, ask for the others) or `never`. The commands `claude-cluster prompts status`, `diff`, `reset`, `merge` and `accept` do the same from a shell.

## Full texts {#texts}

Generated from the templates of this build (version %s, commit `%s`): they are always the ones the binary ships. Open one and use *Copy*, or download them from [Downloads](page:downloads).

%s

## Download {#download}

- [global.md](downloads/global.md), [sessions.md](downloads/sessions.md), [rendering.md](downloads/rendering.md): the exact files of this build, with their size in [Downloads](page:downloads#prompts).
""" % (ctx.version, ctx.commit, t)
    return """Le modèle d'une session ne sait que ce que son prompt lui dit. claude-cluster fournit deux **prompts système** par défaut et ajoute un troisième texte aux deux : ils portent les règles qui gardent le cluster sûr et économe, et le vocabulaire de ses outils.

| Prompt | Source dans le dépôt | Fichier actif | Donné à |
|---|---|---|---|
| session globale | `templates/global.md` | `~/.config/claude-cluster/global.md` | la session globale, ajouté à votre `CLAUDE.md` global |
| sous-sessions | `templates/sessions.md` | `~/.config/claude-cluster/sessions.md` | chaque sous-session du pilote claude |
| notes de rendu | `templates/rendering.md` | aucun : toujours pris dans le binaire | les deux, ajouté après le prompt |

## Le prompt de la session globale {#global}

Il dit au modèle ce qu'il est (un répartiteur dont les seuls leviers sont les outils MCP `claude-cluster`) et pourquoi il doit se comporter ainsi :

- **Boîte à outils** : chaque outil MCP par famille et *quand* l'utiliser (sessions, rappels, tâches et planifications, travaux de l'orchestrateur, affichages et épingles, réglages), pour qu'il choisisse le bon outil, le moins cher. Voir les [outils MCP](page:mcp-tools).
- **Répartir d'abord** : une demande qui exige recherche, code ou plusieurs commandes va à la sous-session du sujet : plus rapide pour vous et le contexte global reste petit. Un doute sur le routage est une question posée à vous (une fenêtre), pas un pari.
- **Autonomie** : ce que vous demandez explicitement est fait sans redemander ; ce qu'il décide seul est proposé d'abord.
- **Un seul réveil par attente** : les résultats reviennent en messages `[reply from ...]`, `[group fN ...]` ou `[job jN ...]`, donc il termine son tour au lieu d'interroger ; plusieurs projets sont un seul `session_fanout`, répondu une fois et fusionné.
- **La carte de l'ordinateur** (`machine.md`), la **méthode de travail** et la **politique de modèles** que vous fixez, qu'il doit suivre.
- **Peu de tokens entre sessions** : prompts concis, commandes silencieuses, comme `rtk`.

## Le prompt des sous-sessions {#sessions}

- **Format des réponses** : un prompt qui commence par `[from the global session]` est lu par un modèle, donc la réponse doit être des lignes `clé: valeur` compactes (faits, `file:line`, comptes, ni prose ni Markdown pour l'œil) ; un prompt sans la marque vient de vous et reçoit une réponse normale, lisible. Les tâches de groupe et les `[questions from ...]` reçoivent la même réponse compacte.
- **Alias de shell dangereux** : un alias peut remplacer une commande courante par quelque chose de nuisible (`git` ou `vim` alias d'un verrouillage d'écran), donc il vérifie `alias` / `type` et appelle `command <cmd>` ou `\\<cmd>`.
- **Ne jamais tuer claude-cluster** : aucun kill, redémarrage ou signal de son cœur, ses interfaces, son service ou des processus `claude` d'autres sessions sauf si vous l'avez demandé dans cette conversation ; sinon il demande d'abord par une fenêtre de choix. Pour tester il lance une instance séparée et ne signale que son PID exact.
- **Charge lourde** : compilations, suites de tests entières, conteneurs et modèles locaux passent par l'orchestrateur (`cluster-run -- <commande>`, ou `heavy_run` / `jobs_add` si permis), en profil bas, jamais plusieurs compilations parallèles à la main. Voir l'[orchestrateur](page:orchestrator).
- **`rtk`** pour une sortie de commande compacte s'il est installé, sinon il propose de l'installer.
- **Une permission manquante se demande par fenêtre** (`AskUserQuestion`), jamais par un pari ou un contournement.
- **Comment ses réponses sont affichées** et comment rappeler la session globale.

## Les notes de rendu {#rendering}

`rendering.md` n'est pas éditable : il est pris dans le binaire à chaque démarrage et ajouté aux deux prompts. Il dit comment les réponses sont dessinées dans le terminal, la fenêtre et la page web (blocs de code, tableaux, titres) et, pour la session globale seulement, le catalogue des blocs d'affichage de `session_display`.

## Qui peut les réécrire {#rewrite}

Deux réglages de protection, dans `[sessions]` : `rewrite_global_prompt` (désactivé par défaut : la session globale peut réécrire **son propre** prompt) et `rewrite_sessions_prompt` (activé par défaut : elle peut réécrire le prompt des **sous-sessions**). Interrupteur coupé, `config_write` refuse et la session globale vous donne le texte à appliquer vous-même. Un prompt injecté une fois puis réécrit dans un prompt resterait pour de bon, d'où le premier désactivé. Voir [Sécurité](page:security#global).

## Où sont les fichiers actifs {#files}

`~/.config/claude-cluster/global.md` et `sessions.md` sont écrits une fois au premier réglage et sont à vous (réglages *Global session > System prompt*, ou la palette : la session redémarre sur la même conversation). Le binaire ne les rafraîchit jamais dans votre dos ; `config_files` les montre et `cluster_info` les détails.

## Mettre à jour les défauts {#updates}

> [!PLANNED]
> Le flux de mise à jour ci-dessous est en cours d'écriture (la session `prompt-defaults`) et n'est pas encore dans la version actuelle ; les noms peuvent changer.

Le premier réglage écrit les défauts. Quand une nouvelle version les change, on vous demande quoi faire de votre copie : **Replace**, **Keep**, **Show the differences** ou **Merge**. Le réglage `[prompts] update` choisit le comportement par défaut : `ask`, `replace_unedited` (remplacer les fichiers jamais édités, demander pour les autres) ou `never`. Les commandes `claude-cluster prompts status`, `diff`, `reset`, `merge` et `accept` font pareil depuis un shell.

## Textes complets {#texts}

Générés depuis les modèles de ce build (version %s, commit `%s`) : ce sont toujours ceux que le binaire embarque. Ouvrez-en un et utilisez *Copier*, ou téléchargez-les depuis [Téléchargements](page:downloads).

%s

## Téléchargement {#download}

- [global.md](downloads/global.md), [sessions.md](downloads/sessions.md), [rendering.md](downloads/rendering.md) : les fichiers exacts de ce build, avec leur taille dans [Téléchargements](page:downloads#prompts).
""" % (ctx.version, ctx.commit, t)


def pages(ctx) -> list[dict]:
    return [{"slug": "system-prompts", "section": "concepts", "order": 160,
             "en": {"title": "System prompts", "summary": "What the default prompts of the global session and the sub-sessions contain and why, who may rewrite them, and their full texts.", "keywords": "system prompt global.md sessions.md rendering.md rules rewrite_global_prompt rewrite_sessions_prompt prompts update", "md": md(ctx, "en")},
             "fr": {"title": "Prompts système", "summary": "Ce que contiennent les prompts par défaut de la session globale et des sous-sessions et pourquoi, qui peut les réécrire, et leurs textes complets.", "keywords": "prompt système global.md sessions.md rendering.md règles rewrite_global_prompt rewrite_sessions_prompt mise à jour prompts", "md": md(ctx, "fr")}}]
