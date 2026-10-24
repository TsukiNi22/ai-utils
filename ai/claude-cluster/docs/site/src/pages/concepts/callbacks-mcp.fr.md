+++
title = "Rappels, questions et MCP"
summary = "Comment les réponses reviennent, comment les questions des agents sont traitées, le serveur MCP et sa page de réglages."
section = "concepts"
order = 100
keywords = "rappel réponse mcp serveur questions askuserquestion permission revue chat réponses globale mcp_tools serveurs personnalisés job tâches"
+++

## Rappels {#callbacks}

Rien dans claude-cluster n'interroge en boucle. Quand la session globale envoie un prompt (`session_send`, `session_spawn`, une tâche, un travail), le résultat **lui revient** sous forme d'un message qu'elle peut lire :

| Message | Quand |
|---|---|
| `[reply from <nom> (<id>)] ...` | une sous-session a fini le tour lancé pour la session globale |
| `[reply from ...]` avec une note | un tour ultérieur que l'agent a lancé lui-même, quand une tâche d'arrière-plan se termine - ou que claude-cluster l'a réveillé avec la `<task-notification>` de sa commande finie, un résultat `[job ...]` / `[tasks]` : son vrai résultat après une réponse « j'attends mon build » |
| `[error from ...]` | une erreur d'API (une limite, la limite hebdomadaire...) |
| `[interrupted: ...]` | un tour interrompu ou un agent qui s'est arrêté |
| `[job jN finished\|failed\|killed]` | un travail ajouté par `jobs_add`, avec code, durée, fin de sortie |
| `[tasks] ...` | le résultat d'une tâche, à la sous-session qui l'a mise en file |
| `[form "<titre>" (<id>) filled by the user] {...}` | un formulaire d'affichage que vous avez envoyé |

`reply: false` désactive un rappel. Les tests `callbacks.sh` et `send_prompt.sh` vérifient le comportement avec un faux `claude`.

## Questions des agents {#questions}

Un agent peut poser une question (`AskUserQuestion` de Claude Code, même en `bypassPermissions`). Elle s'affiche en **fenêtre** sur la session montrée, dans toutes les interfaces : une question à la fois avec ses options, `1`-`9` pour choisir, Entrée pour valider, `Other` pour taper, Échap pour décliner ; plusieurs choix se cochent avec Entrée ou Espace, et une ligne **Next →** après `Other` (*Review →* / *Submit →* sur la dernière question, comme Claude Code) passe à la suite en gardant les coches.

- **Une à la fois pour tout le cluster**, partagée par touches, souris, voix et page web. La question d'une sous-session s'affiche sur la session globale (`[sessions] questions_to_global`, activé) avec la vue propre de la sous-session grisée.
- **Revue** : après la dernière question toutes les réponses s'affichent sur une page : *Submit*, *Cancel*, ou retour pour corriger.
- **Chat about this** (`Ctrl+T`, un bouton, un champ) : discuter de la question avec le modèle de la session sans toucher à sa conversation (ni fork, ni reprise), partagé par les interfaces.
- **La session globale répond** à votre place quand vous appuyez sur `Ctrl+G` (terminal), *Global answers* (fenêtre, page web, notification) ou dites *la globale* : elle choisit d'après la conversation, le projet et la carte de votre ordinateur et vous dit ce qu'elle a répondu. `[sessions] global_answers` (désactivé) lui fait connaître les questions et choisir seule.
- **À la voix** : un numéro ou le libellé d'une option, *next*, *back*, *other ...*, *cancel*.

Le **contrôle de sécurité de Claude Code** (une suppression qu'il ne sait pas résoudre) ne peut être approuvé que par une personne : une sous-session en `bypassPermissions` le demande à la session globale (`bypass_to_global`, activé), qui vous demande en cas de doute.

## Le serveur MCP {#mcp}

La session globale dispose du serveur MCP `claude-cluster mcp`, dont les outils sont listés dans la [référence des outils MCP](page:mcp-tools) (générée depuis le code). Une sous-session du pilote claude en reçoit un plus petit (`task_*`, `schedule_*`, outils de travaux) limité à ce qu'elle a créé.

**Réglages > MCP** (`F9 > MCP`) liste chaque outil avec ce qu'il fait et un interrupteur (`[mcp_tools] <outil> = false` : refusé aussitôt avec un message, absent de sa liste d'outils à son prochain démarrage), et les **serveurs MCP personnalisés** qu'elle reçoit aussi (`[mcp] servers` : `"nom: commande args"` pour un programme local ou `"nom: https://url"`), écrits dans sa config MCP à son prochain démarrage. Ajouter un serveur est un réglage protégé, confirmé à la sauvegarde.

Les outils sont autorisés sans demande (`--allowedTools mcp__claude-cluster`) ; ce qu'ils peuvent faire est borné : voir [Sécurité](page:security#global).

## L'outil docs {#docs}

L'outil `docs` permet à la session globale de chercher dans cette documentation ou d'ouvrir une page pour vous (`search`, `page`, `open`). Voir la [référence des outils MCP](page:mcp-tools).
