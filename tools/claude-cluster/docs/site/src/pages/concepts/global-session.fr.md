+++
title = "La session globale"
summary = "La session qui pilote les autres : son prompt, la carte de votre ordinateur, les rappels, la répartition, ce qu'elle peut et ne peut pas faire."
section = "concepts"
order = 20
keywords = "session globale orchestrateur mcp boîte à outils carte machine fanout rappel réponse remote control recommencer prompt système global.md"
+++

La **session globale** est une session Claude Code à part entière (`Ctrl+G` ; `[global]` : dossier, modèle, effort, mode de permission). Vous lui parlez en langage courant ou à la voix ; elle répartit le travail aux [sous-sessions](page:sub-sessions) avec son propre serveur MCP et vous rend compte. Ce n'est pas un programme séparé : c'est une session dont les outils sont le cluster lui-même.

## Ce qu'elle a {#has}

- Un **prompt système** `~/.config/claude-cluster/global.md`, écrit une fois et à vous d'éditer (réglages *Global session > System prompt* ; elle redémarre sur la même conversation), ajouté à votre `CLAUDE.md` global. Il se termine par une **boîte à outils** compacte : les outils MCP par famille et quand utiliser chacun. Les guides de rendu et de routage viennent du binaire (`rendering.md`, `routing.md`).
- La **carte de votre ordinateur** `machine.md` : vous, votre système et matériel, outils et versions, alias de shell (les dangereux et ceux qui remplacent une commande courante), identité git, config de Claude Code, conteneurs, hôtes SSH et dépôts git de votre dossier personnel. Locale ; lue quand une demande touche un projet local ou le système. Ce qu'elle lit est envoyé à son fournisseur d'API.
- Les **outils** du [serveur MCP](page:mcp-tools) `claude-cluster mcp`, autorisés sans demande mais bornés (voir [Sécurité](page:security#global)).

## Comment elle travaille avec les autres {#callbacks}

- Elle envoie un prompt avec `session_send` ; la réponse **lui revient** en `[reply from <nom> (<id>)]` quand la sous-session a fini. Elle n'interroge jamais (`session_read` relit un contexte entier).
- Chaque fin de tour lancé pour elle la rappelle une fois : le premier prompt d'un spawn, une tâche de la file, une erreur d'API (`[error from ...]`), un tour interrompu, un agent qui s'est arrêté, une tâche d'arrière-plan qui se termine.
- **Plusieurs projets à la fois** : une demande qui touche plusieurs projets devient un groupe (`session_fanout`) : une tâche par projet (sessions ou dossiers créés pour eux, jusqu'à un motif `match`), un chaînage (`after`), une attente qu'elle choisit (`all`, `first`, `count`, avec un délai), une tâche en échec retentée une fois. Elle est réveillée **une seule fois** avec toutes les réponses et les fusionne (`merge: session` pour une grosse fusion, `table` pour une comparaison). `[sessions] max_parallel` sous-sessions travaillent en même temps ; les autres attendent dans la [file de tâches](page:tasks-schedules).
- Entre sessions elle parle sobrement (lignes `clé: valeur`, JSON compact sans champs vides), comme `rtk` : ce que vous lisez reste normal.

## Ce qu'elle peut et ne peut pas faire {#rights}

Elle peut créer, solliciter, interrompre, fermer, restaurer, renommer, déplacer, décrire et catégoriser des sessions, changer modes et modèles dans les limites de votre [politique](page:permissions-models), gérer tâches, planifications et travaux, montrer affichages et épingles, lire et changer la plupart des réglages. Elle ne change jamais les **réglages protégés**, et quelques pouvoirs sont coupés tant que vous ne les autorisez pas : forker des sessions (`global_fork`), éditer la conversation d'une sous-session (`global_edit`), réécrire son propre prompt système (`rewrite_global_prompt`), répondre à votre place aux questions des agents (`global_answers`). L'autonomie suit son prompt : ce que vous demandez explicitement est fait sans redemander ; ce qu'elle décide seule est proposé d'abord.

## Rester en vie {#alive}

Elle reste : si son processus s'arrête de lui-même il redémarre sur sa conversation (une fois par minute au plus). **Start over** (palette) envoie toutes les sous-sessions à la corbeille, arrête les tâches et groupes en file, et démarre une nouvelle conversation, après un oui / non. `/clear` ne repart qu'avec le contexte de claude ; la transcription reste. **Remote Control** (`remote on`) met la session globale elle-même sur claude.ai et l'application Claude, en direct dans les deux sens.

## Son mode de permission {#mode}

Par défaut elle tourne en `bypassPermissions` (`[global] permission_mode`) : elle ne demande jamais, ce qui est le but d'un orchestrateur et son risque, car un prompt injecté dans une sous-session (une page web qu'elle a lue) lui revient comme réponse. Régler `auto` la fait demander avant une commande ; les outils MCP de claude-cluster restent bornés dans les deux cas.
