+++
title = "Sous-global par catégorie"
summary = "Prévu : une session responsable par catégorie qui pilote les sessions de sa catégorie, sous la session globale."
section = "concepts"
order = 30
keywords = "sous-global responsable catégorie prévu max_leads ask auto off intégrer compiler git push"
+++

> [!PLANNED]
> Le sous-global est une conception, pas une fonction de la version actuelle : rien de ce qui suit n'existe encore dans le code, et les noms de réglages peuvent changer. La session globale, les catégories et la [taxonomie](page:taxonomy) sur lesquelles il s'appuie existent bien.

## L'idée {#idea}

Avec beaucoup de projets la session globale sait tout et porte le contexte de tout. Un **sous-global** est un responsable pour **une catégorie** (par exemple `work`, `perso`, `tools`) : il ne connaît que cette catégorie, pilote les sessions de ses projets et rend compte à la session globale, qui garde la vue d'ensemble.

```text
global
├── sous-global · work   ── session A, session B
├── sous-global · perso  ── session C
└── sous-global · tools  ── session D
```

## Conception {#design}

- **Au plus un** sous-global par catégorie.
- C'est une session ordinaire du pilote claude avec un jeu d'outils restreint et un prompt qui nomme sa catégorie ; elle coûte des tokens comme toute session, donc elle n'est créée que si c'est utile.
- La session globale **juge si c'est utile** (beaucoup de sessions actives dans une catégorie, un thème qui dure) et **vous demande toujours par une fenêtre** avant d'en créer un ; elle n'en crée jamais en silence.

## Droits {#rights}

| Il peut | Condition |
|---|---|
| gérer les sessions de sa catégorie : prompt, interruption, fermeture, déplacement, description | sa catégorie seulement |
| intégrer et compiler **une seule fois** pour sa catégorie | par l'[orchestrateur](page:orchestrator), donc les compilations se relaient toujours |
| créer des tâches et planifications pour sa catégorie | les [tâches et planifications](page:tasks-schedules) de sa catégorie seulement |
| `git push` | **seulement avec votre accord**, demandé à chaque fois |

Il ne peut pas changer les réglages, les autres catégories, la politique ni les réglages protégés.

## Réglage {#setting}

Un réglage choisit le comportement : `ask` (la session globale propose et vous décidez, par défaut), `off` (jamais), `auto` (créé sans demander, dans la limite de `max_leads`, le nombre maximal de sous-globaux vivants en même temps). Même en `auto` la session globale juge d'abord l'utilité.

## D'ici là {#today}

Vous pouvez obtenir un résultat voisin dès aujourd'hui avec une catégorie (`session_describe`), une sous-session dédiée comme responsable, et des tâches qui lui sont adressées ; la session globale parle alors à ce responsable plutôt qu'à chaque session.
