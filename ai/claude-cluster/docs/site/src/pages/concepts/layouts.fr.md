+++
title = "Dispositions"
summary = "Liste, grille, onglets et hub : comment les sessions sont disposées dans chaque interface et comment passer de l'une à l'autre."
section = "concepts"
order = 140
keywords = "disposition layout liste grille onglets hub tableau arbre f2 panneaux grid_open navigation"
+++

`[ui] layout` (réglages, `F2`, `--layout`, la palette) choisit comment les sessions sont montrées : `list`, `grid`, `tabs` ou `hub`. Le terminal montre `list` ou `grid` ; `tabs` est pour la fenêtre et la page web.

## Liste {#list}

La liste des sessions à gauche avec la conversation active et ses **panneaux** à droite : tokens (dernier prompt, courant, total), contexte (utilisé / fenêtre), coût (session, dernier tour, budget, limites 5 h / 7 j), usage (le fournisseur : aujourd'hui, 7 jours, total, les fenêtres du compte), skills, git (branche, graphe), diff, outils (chronologie), fichiers (modifiés). `[ui] panels` fixe ceux par défaut d'une nouvelle session ; chacun s'active par session depuis la palette. La liste défile (`↑ n more / ↓ n more`) et garde l'entrée **Tasks** en haut.

## Grille {#grid}

Chaque session est une cellule avec son état, son modèle et son dossier ; les cellules gardent une taille minimale (38 x 8) et les lignes qui ne tiennent pas défilent (`rows 1-3 of 5`). Sur un **prompt vide** les flèches passent d'une cellule à l'autre (haut / bas gardent la colonne, gauche / droite tournent dans la ligne) ; avec un prompt en cours d'écriture elles l'éditent et `Alt+flèches` déplacent toujours. `Shift+Entrée` ou `Alt+O` (`grid_open`) ouvre la session ciblée dans la disposition liste. `Ctrl+B` puis une flèche, et `Alt+PageUp` / `PageDown`, `Alt+Home` / `End` naviguent aussi.

## Onglets et hub {#tabs}

`tabs` (fenêtre, page web) met un onglet par session. Dans la page web les vues sont *Chat*, *Grid* (une carte par session avec un champ de message), *Board* (votre tableau de bord de tuiles déplacées par glisser-déposer, tailles S / M / L, gardé dans le navigateur), *Hub* (un graphe : voir [la taxonomie](page:taxonomy#graph)) et *Tree* (sessions par dossier de projet). Sur téléphone les vues sont une barre d'onglets.

## Navigation {#navigation}

`Ctrl+N` / `Alt+P` suivante / précédente, `Alt+1`..`Alt+9` session N, `Alt+flèches` et `Ctrl+B`-flèches par côté, `Alt+G` le graphe. La liste et la grille défilent pour garder la session visible. Voir [Touches](page:keys-reference).
