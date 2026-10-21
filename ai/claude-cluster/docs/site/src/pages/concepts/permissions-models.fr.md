+++
title = "Permissions et politique de modèles"
summary = "Modes de permission, qui peut approuver quoi, et la politique de modèle et de réflexion que suit la session globale."
section = "concepts"
order = 110
keywords = "permissions mode auto manual acceptEdits plan dontAsk bypassPermissions modèle politique effort réflexion coûteux routage advise force autorisés"
+++

## Modes de permission {#modes}

| Mode | Sens |
|---|---|
| `auto` | le classificateur de Claude Code autorise les actions sûres (défaut des nouvelles sessions : `[agent] permission_mode`) |
| `manual` (= `default`) | chaque outil demande |
| `acceptEdits` | les modifications de fichiers sont acceptées, le reste demande |
| `plan` | lecture seule, un plan d'abord |
| `dontAsk` | ce qui n'est pas autorisé est refusé sans demander |
| `bypassPermissions` | ne demande jamais (défaut de la session globale : `[global] permission_mode`) |

`Shift+Tab` parcourt auto, acceptEdits, plan, bypassPermissions, manual ; `/mode <mode>`, `claude-cluster mode <s> <mode>` ou `session_set_mode` en fixent un. Les sessions sont lancées avec `--allow-dangerously-skip-permissions` (`[agent] allow_bypass`) pour que bypass puisse être choisi plus tard ; le mode affiché est celui que Claude Code confirme. Les autres pilotes lancent un processus par tour, donc leurs permissions suivent le mode et ne peuvent pas être demandées en direct.

## Répondre {#answering}

Une permission en attente est une fenêtre sur la session, une notification (avec *Allow* / *Deny*) et une ligne dans `session_status`. Vous répondez par `Alt+Y`, `Alt+A` (toujours, garde la règle) ou `Alt+D` ; la session globale répond avec `session_allow`. La raison de chaque permission est montrée avec elle. En mode `auto`, quand le classificateur ne donne pas de verdict (une panne de son côté), une seule ligne comptée s'affiche, et après deux de suite la session passe en manuel pour le reste du tour.

## Politique de modèle et de réflexion {#policy}

La session globale tourne sur `[global] model` / `effort` et **fixe le modèle et la réflexion des sous-sessions** selon la difficulté de chaque tâche, dans les limites de **Réglages > Model policy** (`[policy]`, protégé : elle ne les change jamais).

| Réglage | Effet |
|---|---|
| modèle / réflexion par défaut | ce dont partent les sous-sessions |
| *How the default applies* (`model_mode`, `effort_mode`) | `free` : aucun, selon la tâche ; `advise` : le point de départ, s'en écarter pour une raison ; `force` : rien d'autre, les autres choix refusés avec la raison |
| modèles / efforts autorisés | les seuls qu'elle peut choisir (un nom ou une partie : `haiku, sonnet`) |
| demander avant un choix coûteux | les modèles **coûteux** (`opus`, `fable`) et efforts (`xhigh`, `max`, et au-dessus) : une question sur toutes les interfaces (Allow / Deny, une notification) |

Son prompt système porte toujours un **guide de routage** : modèle bon marché et réflexion basse pour les recherches et le travail mécanique, équilibré pour le code ordinaire, premium et réflexion haute seulement pour les problèmes difficiles ; commencer bas et monter sur un vrai échec ; ne jamais passer une grande session sur un modèle plus fort pour une étape (tout son contexte est relu au nouveau prix) mais ouvrir une nouvelle sous-session avec un bref énoncé. L'outil `models_policy` lui donne vos règles en direct : les modèles de son fournisseur avec niveau, prix, s'ils sont coûteux ou autorisés, et ce qu'un tour de chacun vous a vraiment coûté.

Un choix coûteux revient aussitôt en `asked` : la session garde son modèle jusqu'à votre réponse ; Allow l'applique et le dit à la session globale, Deny le garde et dit pourquoi. Ces questions sont les vôtres seules. Un changement que vous faites vous-même (palette, `/model`, `claude-cluster model`) n'est jamais filtré.

## Fournisseurs {#backends}

`--backend` et `[agent] provider` prennent `<fournisseur>[/<modèle>]` : `claude` (défaut), `anthropic-api`, `ollama`, `openrouter`, `deepseek`, `kimi`, `zai`, `minimax` (Claude Code sur un point d'accès compatible Anthropic), `openai` et `ollama-openai` (Qwen Code), `opencode/<fournisseur>/<modèle>`, `codex`, ou vos propres `[providers.<nom>]`. Le pilote **claude** est le complet : un processus par session, permissions en direct, le serveur MCP, Remote Control. `claude-cluster auth list` montre ce qui est prêt. Le coût affiché pour un fournisseur non Anthropic via Claude Code est une estimation de claude et s'affiche `local / unknown`.
