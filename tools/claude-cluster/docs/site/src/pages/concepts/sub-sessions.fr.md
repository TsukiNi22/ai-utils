+++
title = "Sous-sessions"
summary = "Ce qu'est une sous-session, et en quoi elle diffère d'un fork et d'un sous-agent de Claude Code."
section = "concepts"
order = 10
keywords = "sous-session session fork sous-agent subagent tâche agent processus globale"
+++

Tout, dans claude-cluster, est une **session** : un agent en cours (Claude Code par défaut) dans un dossier de projet, avec sa conversation, son mode, son modèle et ses permissions. Les sessions auxquelles vous parlez à travers la [session globale](page:global-session) sont ses **sous-sessions**.

## Ce qu'est une sous-session {#what}

Une sous-session est un vrai processus d'agent à part :

- un **dossier** (un projet de la carte de votre ordinateur), une conversation qui survit aux redémarrages (`--resume`), un **mode** (`auto`, `manual`, `acceptEdits`, `plan`, `dontAsk`, `bypassPermissions`), un **modèle** et un effort de réflexion ;
- elle apparaît dans toutes les interfaces (terminal, fenêtre, page web, téléphone) comme une ligne avec son état, sa courte description, sa catégorie et ses tags ;
- elle est créée par vous (`/new`, Ctrl+T, `claude-cluster spawn`) ou par la session globale (`session_spawn`), et fermée vers la corbeille (restaurable pendant `trash_days`) ou supprimée pour de bon ;
- le pilote claude garde **un processus par session** avec des permissions en direct et son propre petit serveur MCP ; les autres pilotes (Codex, opencode, Qwen Code) lancent un processus par tour et reprennent la conversation par son identifiant.

La session globale reste un niveau au-dessus : elle répartit vos demandes, lit les réponses (elles lui reviennent en `[reply from <nom> (<id>)]`, sans interrogation) et les fusionne.

## Comparée à un fork {#fork}

Un **fork** (`claude-cluster fork <s>`, palette *Fork the session*) n'est qu'une façon de *démarrer* une session : une copie d'une conversation Claude Code à ce jour, puis chaque copie suit son chemin. Le résultat, ce sont deux sous-sessions ordinaires. Utilisez-le pour donner à une nouvelle session le contexte d'une autre. La session globale ne peut forker que si vous l'avez autorisé (*Global session can fork*, désactivé par défaut).

## Comparée à un sous-agent de Claude Code {#subagent}

Un **sous-agent** est une fonction *à l'intérieur* d'une session Claude Code : l'agent confie un travail à un assistant avec sa propre fenêtre de contexte et reçoit un résumé. claude-cluster ne le voit pas.

| | Sous-session | Fork | Sous-agent Claude Code |
|---|---|---|---|
| Visible dans les interfaces | oui, une ligne à elle | oui, une fois démarré | non, dans son parent |
| Conversation propre conservée | oui | oui (copiée au départ) | non, un résumé revient |
| Mode / modèle / permissions propres | oui | oui | ceux de son parent |
| Vous pouvez lui parler | oui | oui | non |
| Démarrée par | vous ou la session globale | vous | l'agent lui-même |
| Coût | ses propres tokens | ses propres tokens | tokens de la session parente |

## Limites et bonnes pratiques {#limits}

- `[sessions] max_parallel` plafonne les sous-sessions qui travaillent en même temps ; les autres tâches attendent dans la file (`task_queue` la borne). Voir [Tâches et planifications](page:tasks-schedules).
- Les compilations et tests lourds passent par l'[orchestrateur](page:orchestrator), jamais plusieurs à la fois depuis plusieurs sessions.
- Une session faite pour un travail précis peut être créée `one_shot` : elle part à la corbeille d'elle-même une fois sa réponse relayée. Voir [Sessions à usage unique](page:one-shot).
- Passer une grande session sur un modèle plus fort relit tout son contexte au nouveau prix (demandé d'abord tant qu'il est chargé) : préférez une nouvelle sous-session avec un bref énoncé ([Permissions et modèles](page:permissions-models#model-change)).
- Le contexte d'une sous-session est plafonné par `[sessions] context` (`auto`, ou 100k à 1M : compacté au-delà) ; la session globale peut donner le sien à chacune quand elle la démarre (`session_spawn` `context` : `150k` pour un petit correctif), pour que chaque tour et un réveil après l'expiration de son cache coûtent ce dont la tâche a besoin ([Réglages](page:settings#context)).

> [!NOTE]
> Une sous-session du pilote claude reçoit un petit MCP restreint (`task_*`, `schedule_*`) : elle ne voit et ne gère que les tâches et planifications qu'elle a créées.
