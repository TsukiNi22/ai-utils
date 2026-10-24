+++
title = "Orchestrateur et cluster-run"
summary = "La file des travaux lourds : pourquoi, comment utiliser cluster-run et heavy_run, les limites qui suivent le matériel et les règles de pression."
section = "concepts"
order = 50
keywords = "orchestrateur cluster-run heavy_run travaux file compilation tests conteneur gpu pression charge nice ionice priorité slots matériel"
+++

Quand plusieurs sessions compilent et testent en même temps, un ordinateur sature : charge très supérieure aux cœurs, swap plein, compilateurs qui plantent. L'orchestrateur est un petit ordonnanceur de noyau pour les **travaux lourds** (compilations, tests, conteneurs, média, GPU) : ils attendent dans une file, un nombre limité tourne en même temps, et la limite suit le matériel, la charge du moment et vos réglages. Les commandes légères (`ls`, `grep`, `git status`) n'y passent jamais.

## L'utiliser {#use}

| Où | Comment |
|---|---|
| un shell (celui d'une session ou le vôtre) | `cluster-run [--class build\|tests\|container\|gpu\|custom] [--prio N] [--name x] -- <commande>` : attend (`cluster-run: queued: position 2, waiting for slot (2/2 running)` sur stderr), puis s'exécute en profil bas (nice, ionice, `-j` / `--parallel` / `MAKEFLAGS` / `CARGO_BUILD_JOBS` abaissés à la part accordée) ; sortie et code de retour sont transmis |
| MCP, une commande qui attend | `heavy_run` : rend le résultat (fini avec code, durée, fin de sortie ; échoué ; tué par qui) ou l'état d'attente |
| MCP, sans attendre | `jobs_add` (réveillé par `[job jN ...]`), `jobs_list`, `job_status`, `job_wait`, `job_kill`, `job_stop`, `job_resume`, `job_prio` |
| l'ordinateur | `hardware_scan`, `orchestrator_config` |
| interface | la session **Tasks** (une section Jobs : file, jauges, tuer / arrêter / reprendre / priorité), réglages *Orchestrator* |

`cluster-run` est installé à côté de `claude-cluster` et se trouve dans le `PATH` de chaque session. Sans cœur en marche, il lance la commande directement avec un avertissement.

## Qui voit quoi {#scope}

La session globale voit et gère tous les travaux. Une sous-session ajoute, suit, attend et tue **ses propres** travaux (`[orchestrator] sub_add`, `sub_kill_own`, `sub_see_others`). Une session fermée ou archivée perd ses travaux.

## États d'un travail {#states}

```text
queued -> running -> done | failed | killed
              \-> stopped (par vous, ou par la pression) -> running
```

Un travail ne démarre que si l'orchestrateur est actif, qu'il n'y a pas de blocage de pression, que les travaux actifs sont sous les slots, que sa session et sa classe sont sous leurs limites et que son estimation de RAM tient. Quand rien ne tourne un travail démarre quand même pour que rien ne reste affamé. L'ordre est `10 * prio` plus la priorité de session, un bonus pour la session que vous regardez, et le vieillissement (un travail en attente gagne un point toutes les `aging_seconds`), puis FIFO.

## Limites {#limits}

- **Automatiques** : slots = min(cœurs utilisables x facteur, (RAM - réservée) / RAM d'un travail), avec le facteur 0,5 / 1 / 1,5 pour le profil `low` / `normal` / `max` ; RAM par classe (build 2000 Mo, tests 1200, container 1500, gpu 2000, custom 1000).
- **Manuelles** : `max_jobs`, `max_per_session`, par classe (`gpu` 1, `container` 2 par défaut), `reserved_cpu`, `reserved_ram_mb`, `threads`.
- **Pression** (charge, `MemAvailable`, swap, `/proc/pressure`) : au niveau *hold* rien de nouveau ne démarre ; au niveau *reduce* les travaux en cours sont ralentis (renice) et le plus récent de plus basse priorité est arrêté (SIGSTOP) jusqu'à la baisse de charge, puis repris avec hystérésis. Jamais tués sauf `kill_on_oom = true`.
- **Détection automatique** : un groupe de processus hors file qui a consommé `detect_cpu_seconds` de CPU est listé et sa session est prévenue une fois d'utiliser `cluster-run`.

## Garde mémoire {#memory-guard}

`[orchestrator] memory_guard` (activé) surveille toutes les 5 s, sans aucun modèle, tout ce que les sessions font tourner - agents, leurs shells et commandes en arrière-plan, sous-agents, travaux lourds, la session globale aussi, jamais le cœur. Sous `guard_free_mb` de mémoire disponible (0 : 5 % de la RAM, au moins 500 Mo) il tue le plus gros d'entre eux, tout son arbre de processus, avant que le tueur de mémoire du système ne le fasse : d'abord une commande ou un travail (jamais sous `guard_min_mb`), un agent seulement quand rien d'autre n'est assez gros, la session globale en dernier. La session apprend ce qui a été tué, combien il utilisait et qu'elle peut le relancer avec moins de mémoire ; une notice et une notification le disent aussi. Réglages > About / Processes montre la mémoire de chaque session, la mémoire libre / totale, la garde et ses derniers arrêts.

## Matériel {#hardware}

`hardware_scan` enregistre CPU, cœurs, RAM, swap, disque, GPU et alimentation dans `hardware.json`, au démarrage et à la demande. L'échantillon de pression est lu chaque seconde dans `/proc`.

> [!NOTE]
> La conception complète (machine à états, protocole du wrapper, persistance) est dans `docs/orchestrator.md` des sources ; la page [protocole](page:protocol) résume les messages du wrapper.
