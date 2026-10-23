+++
title = "Les interfaces"
summary = "Terminal, fenêtre, page web et téléphone : les mêmes sessions partout, synchronisées."
section = "guide"
order = 30
keywords = "terminal tty fenêtre gui qt page web téléphone android application headless serve cœur partagé hub grille tableau"
+++

Chaque interface est une vue d'un seul **cœur** (`claude-cluster serve --linger`, lancé par le premier terminal ou la première fenêtre). Plusieurs terminaux, fenêtres, pages web et téléphones peuvent être ouverts en même temps, synchronisés : mêmes sessions, prompts, questions et notices (`[ui] shared_core`, activé). En fermer une laisse les autres ; la dernière fermée arrête le cœur, sauf si le [service d'arrière-plan](page:settings#service) est le cœur.

## Terminal {#terminal}

`claude-cluster` dans un terminal (ou `--tty`). Construit avec FTXUI : une liste de sessions, la conversation, des panneaux, des fenêtres, la souris (clic, glisser pour sélectionner, molette) et les touches de [la page des touches](page:keys).

- Dispositions (`F2`, `--layout`) : `list` (liste plus panneau actif) et `grid` (chaque session dans une cellule ; les flèches passent d'une cellule à l'autre sur un prompt vide, `Shift+Entrée` / `Alt+O` en ouvre une). Voir [Dispositions](page:layouts).
- `Alt+G` ouvre la [vue graphe](page:taxonomy#graph) : la session globale au milieu, les sessions autour.
- Le terminal reste au premier plan : `Ctrl+Z` est « annuler » dans le prompt, jamais une suspension.
- Les images de la transcription demandent `chafa` ou ImageMagick (`magick`).

## Fenêtre {#window}

`claude-cluster --gui` (compilée quand Qt6 Widgets est trouvé) : les mêmes fonctions dans une fenêtre native avec onglets, un en-tête par session, des dialogues de réglages et un graphe hub. `claude-cluster -g --tty-mode` montre l'interface terminal dans la fenêtre (`[ui] gui_mode = tty` le garde). Le service l'ouvre quand vous dites la phrase de réveil.

## Page web {#web}

Ouvrez l'accès web (réglages *Web*, la palette *Web access: open*, `claude-cluster web on` ou `--web`) et un navigateur du même réseau pilote claude-cluster : sessions, transcriptions, prompts, Stop, permissions et questions, nouvelles sessions, réglages.

- **Vues** (les mêmes cinq dans le terminal, la fenêtre, la page et l'app ; chaque démarrage sur la liste) : *List*, *Grid* (une tuile par session, Tasks en dernier), *Graph* (global, puis les catégories / sous-globales de la [taxonomie](page:taxonomy), puis les sessions), *Hub* (toutes les sessions autour de global) et *Tree* (sessions par dossier).
- **Entrer** : https avec un certificat de votre propre autorité (approuvée une fois par appareil depuis la page de connexion, plus d'avertissement ensuite), un **code à 6 chiffres** qui change à chaque ouverture, puis **votre accord** sur l'ordinateur (*Allow*, *Allow and trust*, *Deny*). Les appareils de confiance reviennent seuls.
- **Mode terminal** : le bouton ⌨ lance un vrai `claude-cluster --tty` pour cet appareil dans le navigateur.
- **Hors du réseau local** : `[web] remote = tailscale` (recommandé) ou `router`. Voir [Sécurité](page:security).
- **Docs** : cette documentation est servie par le cœur sur `/docs` et s'ouvre dans un panneau avec `Ctrl+K` puis *Docs*. Sans l'accès web, `claude-cluster docs serve [port] [lan]` la sert seule, en lecture seule (port 8790, cet ordinateur seulement sauf avec `lan`).

Par défaut seules les adresses privées des sous-réseaux de votre ordinateur sont servies, et depuis la page on ne peut ni ouvrir un shell ni changer les réglages web sans l'avoir autorisé (voir [Sécurité](page:security)).

## Téléphone {#phone}

Tout navigateur de téléphone convient. Sur Android la page propose une **application** (`/app.apk`) : la même page dans une coque légère qui fait confiance à votre ordinateur une fois, affiche les notifications du bureau et peut utiliser le micro. Voir [Notifications et application téléphone](page:notifications-apk). iPhone : la page du navigateur, avec les étapes du certificat sur la page de connexion.

## Sans interface {#headless}

`claude-cluster serve` lance le cœur sans interface. Tout est alors piloté par les commandes headless (`list`, `send`, `spawn`, `allow`, `task`, `schedule`, `web`... avec `--rtk` pour une sortie compacte faite pour une IA, `--json` brut) et par le serveur MCP. Voir le [protocole](page:protocol).
