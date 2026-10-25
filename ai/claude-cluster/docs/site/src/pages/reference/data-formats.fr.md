+++
title = "Formats de données"
summary = "Où claude-cluster garde ses fichiers, et le format de chacun : état, transcriptions, historique, réserve, taxonomie, usage, appareils."
section = "reference"
order = 60
keywords = "formats de données fichiers state.json logs jsonl historique stash taxonomie usage stats trusted banned hardware method rewind pièces jointes dossiers chemins"
+++

## Dossiers {#folders}

| Dossier | Défaut | Remplacement |
|---|---|---|
| config | `~/.config/claude-cluster` | `CLAUDE_CLUSTER_CONFIG_DIR` |
| données | `~/.local/share/claude-cluster` | `CLAUDE_CLUSTER_DATA_DIR` |
| socket | `$XDG_RUNTIME_DIR/claude-cluster/control.sock` | `CLAUDE_CLUSTER_SOCKET` |

Les dossiers de config et de données sont en `0700`. En dehors d'eux un binaire installé écrit son icône et son fichier desktop dans le thème d'icônes, et le service son unité systemd utilisateur.

## Dossier de config {#config}

| Fichier | Contenu |
|---|---|
| `config.toml` | la configuration, écrite commentée au premier lancement ; seules les valeurs changent à la sauvegarde ([référence](page:config-reference)) |
| `global.md`, `sessions.md` | les prompts système de la session globale et des sous-sessions (copies écrites une fois, à vous d'éditer) |
| `machine.md` | la carte de cet ordinateur |
| `vocabulary.txt`, `voice-words.txt` | le vocabulaire de la voix et les mots d'envoi / d'annulation |
| `credentials.json` | clés d'API quand le trousseau n'est pas utilisé (`0600`) |
| `web/` | `ca.pem` et sa clé (`0600`), le certificat du serveur, `trusted.json`, `banned.json` (`0600`) |
| `android/release.jks` | la clé de signature de l'application Android |

## Dossier de données {#data}

| Fichier | Format et contenu |
|---|---|
| `state.json` | un objet JSON : `sessions` (chacune : `id`, `name`, `cwd`, `backend`, `claude_id` pour `--resume`, `model`, `mode`, `effort`, `panels`, `global`, `created`, `cost`, `tokens`, `summary`, `category`, `tags`, `turns`, `pin`...), `trash`, `tasks`, `schedules`, compteurs `next*`, `ui` |
| `logs/<id>.jsonl` | la transcription d'une session, une entrée JSON par ligne (gardée dans `transcript_limit_mb`, 25 Mo ; en mémoire ses entrées récentes dans `memory_mb`, 1 Mo, toutes ensemble dans `memory_total_mb`, 100 Mo - les plus anciennes relues d'ici quand on remonte en haut) ; `logs/archive/` contient les conversations terminées par `/clear` ; chaque entrée a son `origin` (user, mcp, task, scheduled) |
| `history.jsonl` | les prompts envoyés : `{time, session, text, images, pastes}` par ligne, les `history_limit` (1000) derniers gardés |
| `stash.json` | les prompts mis de côté, global, avec l'heure et la session |
| `taxonomy.json` | la forêt de catégories et tags : nœud vers parent |
| `usage.jsonl` | le registre des tokens et du coût par tour, avec le détail (entrée, cache écrit 5 min / 1 h, cache lu, sortie, réflexion) |
| `stats.jsonl` | événements d'activité (démarrage, signe de vie chaque minute, arrêt, outil, décision de permission, limite, erreur) |
| `prices.json`, `limits.json` | prix appris de Claude Code ; état des limites |
| `orchestrator.json`, `hardware.json` | l'historique des travaux (les `history` derniers) ; le scan du matériel avec `scan_version` |
| `method.json` | détection de méthode de travail : compteur, proposition en attente |
| `core.log`, `debug.log` | le journal du cœur ; la trace de debug (seulement avec `[debug] enabled`) |
| `attachments/<session>/` | fichiers téléversés, `0600` ([Pièces jointes](page:attachments)) |
| `rewind/<session>-<heure>.json` | ce qu'un retour en arrière a retiré (l'ancienne feuille, les lignes de journal retirées) |
| `plugins/` | sources clonées et `index.json` ([Réglages](page:settings)) |
| `icons/` | les visages en SVG |
| `wake.py` | le détecteur de la phrase de réveil |

## Entrées de transcription {#entries}

Une entrée de `logs/<id>.jsonl` est un objet JSON `{time, kind, text, tool, origin}` : `kind` est un nombre qui nomme le type d'entrée (prompt, réponse, appel d'outil, réflexion, bloc d'affichage, notice...), `tool` l'outil concerné et `origin` d'où venait un prompt. Les blocs d'affichage de la session globale (`session_display`) y sont aussi stockés, donc cherchés et exportés en texte. Les anciennes transcriptions sans `origin` sont lues d'après les marqueurs de leur texte (`[from the global session]`, `[reply from`, `[scheduled w`, `[tasks]`). `claude-cluster export <s> [fichier]` écrit du Markdown ; `claude-cluster search <texte>` cherche dans tous les historiques.

## Échange {#interchange}

Tout est du JSON, JSONL, TOML ou Markdown simple : rien n'a besoin de claude-cluster pour être lu. La conversation de l'agent lui-même reste dans le stockage propre de l'agent (pour Claude Code `~/.claude/projects`) : claude-cluster la reprend par identifiant et ne la réécrit jamais, sauf `session_edit` et le retour en arrière explicites, qui écrivent une nouvelle conversation à côté de l'ancienne.
