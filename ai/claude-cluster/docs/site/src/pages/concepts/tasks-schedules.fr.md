+++
title = "Tâches et planifications"
summary = "La file de tâches, les listes à faire et les prompts planifiés : options, conditions, la session Tasks et qui peut quoi."
section = "concepts"
order = 40
keywords = "tâche file planification cron after on_event priorité voie réessai délai rappel session tasks task_add schedule_add max_parallel"
+++

## La session Tasks {#tasks-session}

`☰ Tasks` est la première entrée de la liste (`/tasks`, palette *Tasks*) : elle montre ce que font les sessions et ce qui attend, à la place d'une conversation.

- **Listes à faire** de chaque session (les listes de ses propres outils TodoWrite).
- **File de tâches** : en cours, en attente, faites, échouées, annulées, avec qui a créé chaque tâche, où elle va et pourquoi elle a été annulée.
- **Planifié** : rappels et prompts à une heure, en attente ou exécutés.
- **Jobs** : la file de l'[orchestrateur](page:orchestrator), avec ses jauges.

Un clic sur une ligne ouvre ses actions : démarrer maintenant (au-delà de la limite de parallélisme), annuler ou supprimer (une raison est demandée et dite à la session qui l'a créée), archiver, ouvrir sa session. Sans souris : `↑↓` / `j` `k`, `Tab`, `Entrée`. Une session fermée perd ses tâches et planifications en attente.

## Tâches {#tasks}

Une tâche est un prompt mis en file pour une session. `claude-cluster task add [--target T] [--after t1,t2] <prompt>`, `task list`, `task cancel`, ou l'outil MCP `task_add`. `[sessions] max_parallel` sous-sessions travaillent en même temps et `task_queue` borne combien attendent.

| Option de `task_add` | Effet |
|---|---|
| `after`, `after_sessions` | attendre que des tâches soient faites / des sessions inactives |
| `on_event` | `session_idle`, `git_push` (un nouveau commit sur le distant), `command`, `file` (modifié) ; surveillé en arrière-plan. Une `command` est remplie quand elle sort à 0 (ou `code`) et affiche ce qui est demandé (`stdout` exactement, `contains`, `regex`), lancée toutes les `every` s (`constantly` = 2 s) ; `fail_after: N` abandonne après N échecs d'affilée (la tâche échoue, son auteur est prévenu) |
| `spawn` | la tâche crée sa propre session (`cwd`, `name`, `backend`, `mode`, `effort`, `profile`, `one_shot`) |
| `priority`, `max_concurrency` + `lane` | les plus hautes d'abord ; au plus N tâches d'une voie à la fois |
| `timeout_minutes`, `retry`, `max_cost`, `max_tokens` | arrêtée et signalée si dépassé, retentée après une erreur |
| `expect`, `notify`, `reply` | une réponse courte ou un fichier comme résultat ; une notification à sa fin ; si l'auteur est prévenu |

La réponse est gardée dans la tâche (`result`, jusqu'à `[sessions] task_result_chars`) et `{{t3.result}}` dans un prompt est remplacé par celle de la tâche `t3`, qui s'exécute alors d'abord. `task_act` : cancel, delete, archive, `retry`, `set`, `force`. Une tâche dont la cible n'existe pas, ou dont une dépendance a échoué, échoue avec sa raison, dite à son auteur. Les tâches finies sont archivées après 30 minutes.

## Planifications {#schedules}

`claude-cluster schedule add <quand> <texte> [--target s] [--every N] [--notify]`, `schedule list`, `schedule cancel`, ou `schedule_add`, `schedule_list`, `schedule_edit`, `schedule_act`.

| Option de `schedule_add` | Effet |
|---|---|
| `when` / `every` / `cron` / `reset` | une fois (`+30m`, `HH:MM`, `YYYY-MM-DD HH:MM`), toutes les N minutes, une expression cron (`*/15 9-17 * * mon-fri`), ou à chaque remise à zéro de la fenêtre de limite 5 h / 7 j |
| `days`, `window`, `quiet`, `tz` | seulement ces jours, dans / jamais dans ces heures, dans ce fuseau |
| `until`, `max_runs` | terminée après une heure ou un nombre d'exécutions |
| `if` | `session_idle`, `command` (sort à 0, ou le `code` / `stdout` / `contains` / `regex` demandé ; `fail_after` : la planification annulée après N échecs d'affilée), `file_changed`, `usage` (`5h>80`) : non remplie, l'exécution est sautée |
| `action` | `prompt` (défaut), `notify`, `spawn`, `task`, `command` (lancée en local **sans modèle** ; seule une sortie non vide est envoyée ou notifiée) |
| `ack`, `nag` | redemandée toutes les `nag` minutes jusqu'à acquittement |
| `dry_run` | rien n'est fait : dit quand elle se déclencherait |

`schedule_act` : pause, resume, snooze, ack, trigger (exécuter maintenant), cancel, delete, archive.

## Qui peut quoi {#scope}

La session globale gère tout. Une sous-session du pilote claude reçoit son propre petit MCP (`task_add`, `task_list`, `task_cancel`, `task_act`, `schedule_*`), limité aux tâches et planifications **qu'elle a créées** : sa tâche s'exécute pour elle après son tour et son résultat lui revient en `[tasks] ...`, une planification ne réveille qu'elle, et les commandes locales (actions `command`, `if.command`, `on_event: command`) et `force` ne sont jamais les siennes. Les réglages `[agent]` : `sub_read` (oui), `sub_tasks` (oui), `sub_schedules` (non), `sub_spawn` (non), `sub_spawn_cap` (2).
