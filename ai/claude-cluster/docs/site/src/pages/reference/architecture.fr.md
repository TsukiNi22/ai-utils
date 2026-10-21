+++
title = "Architecture"
summary = "Un cœur, plusieurs interfaces : les processus, les sessions, le serveur MCP et comment les parties se parlent."
section = "reference"
order = 10
keywords = "architecture cœur manager socket de contrôle interfaces ftxui qt web mcp agents processus pilotes orchestrateur voix organisation des sources"
+++

## Vue d'ensemble {#overview}

claude-cluster est un seul programme C++20. Un unique **cœur** par utilisateur possède les sessions ; chaque terminal, fenêtre, page web et téléphone est une **interface** de ce cœur.

:::html
<figure><svg viewBox="0 0 760 330" role="img" aria-label="Les interfaces parlent au cœur, qui lance les agents ; le serveur MCP et cluster-run rappellent le cœur">
<defs><marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" class="arrow"/></marker></defs>
<g id="i1"><rect class="box" x="20" y="14" width="150" height="40" rx="8"/><text class="small" x="95" y="39" text-anchor="middle">terminal (FTXUI)</text></g>
<g id="i2"><rect class="box" x="195" y="14" width="150" height="40" rx="8"/><text class="small" x="270" y="39" text-anchor="middle">fenêtre (Qt6)</text></g>
<g id="i3"><rect class="box" x="370" y="14" width="190" height="40" rx="8"/><text class="small" x="465" y="39" text-anchor="middle">page web, appli téléphone</text></g>
<g id="i4"><rect class="box" x="585" y="14" width="155" height="40" rx="8"/><text class="small" x="662" y="39" text-anchor="middle">CLI (sans interface)</text></g>
<g id="core"><rect class="box-alt" x="120" y="120" width="520" height="86" rx="10"/><text class="lbl" x="380" y="150" text-anchor="middle">cœur (claude-cluster serve)</text><text class="small" x="380" y="176" text-anchor="middle">Manager · socket Control · serveur Web · Orchestrator · Voice</text></g>
<g id="a1"><rect class="box" x="20" y="268" width="170" height="40" rx="8"/><text class="small" x="105" y="293" text-anchor="middle">session globale (claude)</text></g>
<g id="a2"><rect class="box" x="215" y="268" width="190" height="40" rx="8"/><text class="small" x="310" y="293" text-anchor="middle">sous-sessions (agents)</text></g>
<g id="a3"><rect class="box" x="430" y="268" width="150" height="40" rx="8"/><text class="small" x="505" y="293" text-anchor="middle">cluster-run</text></g>
<g id="a4"><rect class="box" x="605" y="268" width="135" height="40" rx="8"/><text class="small" x="672" y="293" text-anchor="middle">serveur MCP</text></g>
<path class="edge-accent" data-from="i1" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i2" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i3" data-to="core" marker-end="url(#arr)"/><path class="edge-accent" data-from="i4" data-to="core" marker-end="url(#arr)"/>
<path class="edge" data-from="core" data-to="a1" marker-end="url(#arr)"/><path class="edge" data-from="core" data-to="a2" marker-end="url(#arr)"/><path class="edge" data-from="a3" data-to="core" marker-end="url(#arr)"/><path class="edge" data-from="a4" data-to="core" marker-end="url(#arr)"/>
</svg><figcaption>Figure 1 : interfaces et agents autour du cœur.</figcaption></figure>
:::

## Le cœur {#core}

Le cœur est lancé par le premier terminal ou la première fenêtre (`claude-cluster serve --linger`) ou par le service systemd ; il s'arrête quand la dernière interface est fermée (sauf si une session travaille encore et que vous avez choisi de la garder). Il contient :

- **Manager** : les sessions, la corbeille, les tâches, planifications, groupes, la taxonomie, l'usage, les statistiques.
- **Control** : un socket unix servi dans un thread par connexion ; la CLI, le serveur MCP, les autres interfaces et `cluster-run` lui parlent (voir [le protocole](page:protocol)).
- **Web** : le serveur https de l'accès web, la page terminal et les docs.
- **Orchestrator** : la file des travaux lourds, avec son thread d'ordonnancement ([Orchestrateur](page:orchestrator)).
- **Voice** : les processus `voice-listen` et `voice-say`.

Les interfaces d'autres processus synchronisent leur état depuis le cœur et leurs actions s'y exécutent. Un identifiant de build (sa version et la date de son binaire) est vérifié à l'attachement : un autre build est refusé jusqu'au remplacement.

## Sessions et agents {#agents}

Chaque session est un processus fils du cœur, lancé par `env -C <dossier> VAR=...` avec l'environnement du fournisseur ; chaque extrémité de tube est close-on-exec pour qu'une session ne garde jamais les tubes d'une autre. Les agents tournent dans leur propre session (`setsid`) : les signaux de votre terminal ne les atteignent jamais.

```text
core
├── session globale  claude -p --input-format stream-json ... --mcp-config mcp.json
├── session s1       claude -p stream-json  (permissions : --permission-prompt-tool stdio)
├── session s2       qwen -p ... -o stream-json -r <id>       (un processus par tour)
└── session s3       opencode run --format json -s <id>        (un processus par tour)
```

Le **pilote claude** garde un processus par session avec permissions en direct et un serveur MCP ; les autres (Qwen Code, opencode, Codex) lancent un processus par tour et reprennent la conversation par son identifiant.

## Le serveur MCP {#mcp}

`claude-cluster mcp` est un petit serveur MCP sur stdio lancé par la session globale. Il transforme chaque appel d'outil en requête sur le socket de contrôle, avec `origin: "mcp"` (les limites que vous avez fixées pour la session globale), ou `origin: "sub"` plus l'identifiant de session pour le petit MCP propre d'une sous-session. Les résultats sont du JSON compact sans champs vides.

## Organisation des sources {#sources}

| Partie | Fichiers |
|---|---|
| cœur | `Core.cpp`, `Manager*.cpp`, `Session.cpp`, `Store.cpp`, `Config.cpp`, `Auth.cpp` |
| contrôle et outils | `Control.cpp`, `Mcp.cpp`, `Actions.cpp`, `Settings.cpp`, `Tools.cpp` |
| web | `Web.cpp`, `WebTty.cpp`, `WebVoice.cpp`, `templates/web.html`, `templates/tty.html` |
| interfaces | `ui/Tty.cpp`, `ui/TtyGraph.cpp`, `ui/Gui.cpp`, `ui/LineEditor.cpp`, `ui/Popup.cpp` |
| orchestrateur | `Orchestrator*.cpp`, `Hardware.cpp`, `RunProfile.cpp`, `run/cluster-run.cpp` |
| voix | `Voice.cpp`, `VoiceEngines.cpp`, `Vocabulary.cpp`, `templates/wake.py` |
| tâches | `Manager-Tasks.cpp`, `Manager-Schedules.cpp`, `Cron.cpp` |
| taxonomie, usage, stats | `Taxonomy.cpp`, `Manager-Usage.cpp`, `Usage.cpp`, `Stats.cpp` |

Dépendances : [libutils](https://github.com/TsukiNi22/libutils), nlohmann/json, toml++, FTXUI, Qt6 Widgets (optionnel) et OpenSSL (optionnel). Les prompts par défaut et le modèle de config sont embarqués dans le binaire depuis `templates/` à la compilation.
