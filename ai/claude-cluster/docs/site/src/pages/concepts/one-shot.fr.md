+++
title = "Sessions à usage unique"
summary = "Sessions faites pour une tâche précise qui s'archivent d'elles-mêmes une fois leur réponse relayée."
section = "concepts"
order = 150
keywords = "usage unique one-shot one_shot archive_one_shot session_spawn session temporaire corbeille"
+++

## Ce que c'est {#what}

Quand la session globale lance une session pour **une tâche précise** (une recherche, une revue, une compilation unique), jamais pour un projet à qui elle reparlera, elle met `one_shot: true` dans `session_spawn` (ou `spawn: {one_shot: true}` dans une tâche). Une fois la réponse de la session relayée, la session est **fermée vers la corbeille** (restaurable) au lieu de s'accumuler dans votre liste.

## Quand elle reste {#stays}

Le drapeau n'est respecté que si `[sessions] archive_one_shot` est activé (par défaut). La session est gardée quand :

- vous avez tapé dedans, ou vous la regardez en ce moment ;
- elle attend une permission ou une question ;
- un autre prompt est en file pour elle ;
- son tour a échoué ou a été interrompu.

Il faut que `reply` soit activé, car l'archivage suit la réponse relayée. Restaurez-la depuis la corbeille comme toute session : `Ctrl+Shift+T`, `/restore`, `claude-cluster restore <s>`.
