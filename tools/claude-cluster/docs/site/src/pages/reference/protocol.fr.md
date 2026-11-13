+++
title = "Protocole local"
summary = "Le socket de contrôle (un JSON par ligne), les messages du wrapper cluster-run, les points d'accès de l'API web et le pont MCP."
section = "reference"
order = 50
keywords = "protocole socket de contrôle json unix cmd hello sync origin cluster-run wrapper api web points d'accès notify mcp"
+++

## Socket de contrôle {#control}

Le cœur écoute sur un socket unix, `$XDG_RUNTIME_DIR/claude-cluster/control.sock` (`CLAUDE_CLUSTER_SOCKET` le remplace ; le dossier est en `0700`, le socket en `0600` : seul votre utilisateur l'atteint). Une connexion porte un nombre quelconque de requêtes ; chacune est **un objet JSON sur une ligne** et reçoit **une ligne JSON** en retour :

```json
{"cmd": "list"}
{"ok": true, "data": [ ... ]}
{"ok": false, "error": "no such session: s9"}
```

| Champ | Sens |
|---|---|
| `cmd` | la commande : `list`, `status`, `send`, `spawn`, `allow`, `mode`, `model`, `task_add`, `schedule_add`, `web`, `usage`, `taxonomy`, `settings_get`, `settings_set`, `heavy_run`, `job_*`... (les commandes headless de la CLI, plus celles des interfaces et des outils MCP) |
| `session` | identifiant (`s1`, `g`), nom ou préfixe de nom |
| `origin` | posé par le transport : `mcp` (la session globale : ses limites et ses interrupteurs `[mcp_tools]` s'appliquent), `sub` (le petit MCP d'une sous-session, avec `owner` : ses propres tâches et planifications seulement), `web` (un appareil web : lecture seule pour ce que les réglages n'ouvrent pas) |
| `client`, `v`, `build` | une interface d'un autre processus : son identifiant, la version de l'état qu'elle a, son identifiant de build |

Commandes spéciales : `hello` rend la version, l'identifiant de build, le pid et le binaire du cœur (une interface le demande avant de s'attacher) ; `sync` est une longue interrogation : elle rend tout l'état de l'interface dès que sa version `v` change (jusqu'à 10 s), et est refusée quand l'identifiant de build diffère.

Une requête n'arrête jamais le cœur : tout ce qu'elle lève est rendu en `{"ok": false, "error": "bad request: ..."}`.

## cluster-run {#run}

`cluster-run [--class c] [--prio n] [--name x] [--cwd d] [--session id] -- cmd...` parle au même socket avec un flux :

| Sens | Message |
|---|---|
| wrapper vers cœur | `{"cmd":"job_run","session","class","prio","name","cwd","argv","wrapper_pid"}` |
| cœur vers wrapper | `{"ok":true,"event":"queued","id","position","total","waiting_for","eta_s"}` (de nouveau quand ça change) |
| cœur vers wrapper | `{"ok":true,"event":"go","id","threads","profile","nice","io_class","io_level"}` |
| wrapper vers cœur | `{"event":"started","pid"}`, puis `{"event":"exit","code","signal","tail"}` |
| cœur vers wrapper | `{"event":"killed","by","reason"}` |

Le créneau est libéré à `exit` ou quand la connexion tombe (le wrapper est mort : le groupe de processus de la commande est terminé). Sans cœur le wrapper lance la commande directement ; quand la connexion tombe pendant l'attente, il réessaie 30 s puis lance directement.

## API web {#web}

L'accès web sert une page unique et une petite API JSON en https, seulement après connexion (le code à 6 chiffres, puis votre accord) ; le cookie de session est lié à l'adresse de l'appareil. Les points d'accès :

| Chemin | Usage |
|---|---|
| `/`, `/index.html`, `/sw.js` | la page et son service worker |
| `/api/login`, `/api/logout`, `/api/ping`, `/api/version` | authentification, aller-retour, version (la version est publique) |
| `/api/state` | l'état du cluster (longue interrogation) |
| `/api/cmd` | une commande de contrôle (`origin: "web"`) |
| `/api/complete` | complétions du prompt |
| `/api/attach`, `/api/attach/file`, `/api/attach/link`, `/api/attach/list`, `/api/attach/remove` | pièces jointes |
| `/api/display`, `/api/file` | l'image d'un affichage ; un fichier d'un affichage |
| `/api/notify?since=N` | le journal des notifications (longue interrogation, l'application Android) |
| `/api/listen`, `/api/say` | la voix de la page |
| `/api/tty/*` (`open`, `read`, `write`, `resize`, `close`, `ws`) | le mode terminal, sur son propre port (`[web] tty_port`) |
| `/ca.crt`, `/app.apk` | servis à tous : le certificat de l'autorité et l'application Android |
| `/docs/*` | cette documentation, servie sans connexion |

## Le pont MCP {#mcp}

`claude-cluster mcp` parle le Model Context Protocol sur stdio. `tools/list` rend les [outils](page:mcp-tools) ; `tools/call` associe le nom de l'outil à une commande de contrôle (`session_display` vers `display`, `jobs_add` vers `job_add`, ...) et pose `origin`. La variable d'environnement `CLAUDE_CLUSTER_SESSION` en fait le MCP d'une sous-session. Un outil désactivé dans `[mcp_tools]` est refusé aussitôt avec un message pour l'utilisateur.
