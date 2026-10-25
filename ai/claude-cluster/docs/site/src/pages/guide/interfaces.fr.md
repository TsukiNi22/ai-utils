+++
title = "Les interfaces"
summary = "Terminal, fenêtre, page web et téléphone : les mêmes sessions partout, synchronisées."
section = "guide"
order = 30
keywords = "supervisor superviseur instances update terminal tty fenêtre gui qt page web téléphone android application headless serve cœur partagé hub grille tableau"
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

## Mode simple {#simple}

`claude-cluster --simple` (ou `--ipad-kid`) ouvre une interface plus calme dans chaque interface, en anglais ou en français (`[ui] language`) ; `[ui] mode = "simple"` la garde. L'accueil montre d'abord ce qui vous attend (questions et permissions, à répondre sur place), puis une carte par aide (session) avec ce qu'elle fait en mots simples, son état et son temps ; une carte ouvre sa conversation avec le prompt habituel (`/`, `::`, `@`). Une tuile *Nouvelle tâche* s'adresse à l'assistant, qui décide comment répartir le travail. Ses réglages ont six pages : *Général* (langue, apparence, notifications, retour à l'interface classique), *Compte & fournisseur*, *Assistant* (comment il répartit le travail, quelle intelligence, quoi demander avant, aides en même temps), *Téléphone* (l'accès web et son QR code), *Raccourcis*, *À propos* (version, documentation, tous les réglages). `F10` ouvre les statistiques.

## Superviseur {#supervisor}

`claude-cluster --supervisor` (ou `s` dans le menu de départ) montre d'un coup d'œil chaque claude-cluster de cet ordinateur : le principal et chaque instance indépendante, en marche ou arrêtée avec des sessions gardées. Chacun affiche sa version, ses sessions comptées par état (au travail, inactives, en attente de vous, en arrière-plan, en file, en erreur), son accès web et sa mise à jour ; `F2` ajoute le détail (chaque session avec son état, ses prompts en file, ses agents, ♨, son modèle).

- `Entrée` une popup de ce qu'il faut faire (ouvrir, mettre à jour, forcer, arrêter, démarrer ; les risqués redemandés) ; `u` / `U` mettre à jour un / tous ; `f` / `F` forcer ; `s` / `S` arrêter / démarrer ; `w` son accès web ; `F9` ses réglages web et ses appareils ; `P` les performances de l'ordinateur (CPU, RAM, GPU / VRAM, disque) ; `F1` les touches ; `q` quitte (`Ctrl+C` jamais).
- Des visages comme ceux des sessions (`>_<` au travail, `•_• !` vous attend, `-_-` inactif, `z_z` arrêté), `◆` principal / `◇` indépendant ; en dessous, **les autres agents de l'ordinateur** (Claude Code, Codex, OpenCode, Qwen, Gemini, Aider, Ollama dans leurs propres terminaux), un point coloré pour leur état ; Entrée en ouvre un dans claude-cluster en copie.
- Un appareil qui demande l'accès web apparaît aussitôt ; l'accès web se ferme quand le superviseur quitte (ou plante) ; arrêter un claude-cluster ne ferme jamais le superviseur.
- Il relance de lui-même un binaire plus récent dès qu'il est installé.
- **Son accès web** est un cœur à part sur `[web] supervisor_port` (8788) : la page et l'app connectées montrent le superviseur (cartes, Détails, Mettre à jour / Forcer / Arrêter / Démarrer, Tout mettre à jour / Tout forcer) au lieu des sessions ; ses actions ne sont acceptées que depuis cet accès.
- La fenêtre (`--supervisor -g`) montre la même liste en arbre avec ses boutons.

## Sans interface {#headless}

`claude-cluster serve` lance le cœur sans interface. Tout est alors piloté par les commandes headless (`list`, `send`, `spawn`, `allow`, `task`, `schedule`, `web`... avec `--rtk` pour une sortie compacte faite pour une IA, `--json` brut) et par le serveur MCP. Voir le [protocole](page:protocol).
