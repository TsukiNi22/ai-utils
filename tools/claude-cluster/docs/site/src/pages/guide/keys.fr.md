+++
title = "Touches et palette"
summary = "Les touches de tous les jours, la palette de commandes, et où les changer."
section = "guide"
order = 40
keywords = "touches raccourcis palette ctrl+k f1 f9 alt souris prompt édition de texte"
+++

Chaque touche ci-dessous est une valeur par défaut : changez-la dans `[keys]` de `config.toml` ou dans la page de réglages. Le tableau complet est la [référence des touches](page:keys-reference). `F1` montre la liste en direct dans l'application.

## L'essentiel {#essentials}

| Touche | Action |
|---|---|
| <kbd>Ctrl+K</kbd> | palette de commandes : toutes les actions (nouvelle session, modes, panneaux, styles, dispositions, fournisseurs, voix, docs...) |
| <kbd>F1</kbd> / <kbd>F9</kbd> / <kbd>F10</kbd> | toutes les touches / réglages / statistiques |
| <kbd>Ctrl+G</kbd> | la session globale |
| <kbd>Ctrl+T</kbd> / <kbd>Ctrl+W</kbd> | nouvelle session / fermer la session (vers la corbeille) |
| <kbd>Ctrl+N</kbd> ou <kbd>Alt+N</kbd> / <kbd>Alt+P</kbd> | session suivante / précédente |
| <kbd>Alt+1</kbd> .. <kbd>Alt+9</kbd> | session N |
| <kbd>Ctrl+B</kbd> puis une flèche | la session de ce côté (grille : lignes et colonnes) |
| <kbd>Alt+Y</kbd> / <kbd>Alt+A</kbd> / <kbd>Alt+D</kbd> | autoriser / toujours autoriser / refuser une permission |
| <kbd>Shift+Tab</kbd> | mode de permission suivant |
| <kbd>Ctrl+C</kbd> | arrêter le tour en cours ; aucun : vider le prompt ; deux fois sur un prompt vide : quitter |
| <kbd>Ctrl+Q</kbd> | quitter |
| <kbd>F2</kbd> / <kbd>Alt+G</kbd> | disposition / vue graphe |
| <kbd>F5</kbd> / <kbd>F6</kbd> | appuyer pour parler / arrêter la lecture |

## Prompt {#prompt}

`Entrée` envoie (un tour en cours : le prompt est mis en file), `Alt+Entrée` ou `Shift+Entrée` ajoute une ligne. `@` complète les fichiers, `/` les commandes, skills et commandes de l'agent, `::` les sessions. Un premier caractère change le mode : `!commande` lance votre shell dans le dossier de la session, `:` donne des commandes à la vi (`:s/old/new/`, `:r fichier`, `:w`, `:set clé=valeur`). Détails : [Édition du prompt](page:prompt-editing).

| Touche | Action |
|---|---|
| <kbd>Ctrl+R</kbd> | historique des prompts de la session |
| <kbd>Ctrl+E</kbd> | éditer le prompt dans votre éditeur |
| <kbd>Ctrl+S</kbd> / <kbd>Ctrl+P</kbd> / <kbd>Ctrl+L</kbd> | mettre le prompt de côté / reprendre le dernier / lister la réserve |
| <kbd>Échap</kbd> | reprendre un prompt en file ou en cours ; sur un prompt vide : revenir en arrière dans la conversation |
| <kbd>Ctrl+O</kbd> | réflexion et résultats d'outils en entier / repliés |
| <kbd>Ctrl+F</kbd> | chercher dans la transcription |
| <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> | début / fin de la transcription |

## La palette {#palette}

`Ctrl+K` liste toutes les actions ; tapez des mots pour filtrer. La recherche tolère les fautes de frappe (`[ui] matching = c2dmp`, la comparaison de chaînes de libutils) : `modle` trouve *Model*. Un préfixe restreint à un genre : `mode:`, `setting:` (seulement les réglages, Entrée ouvre la page dessus), `share:`. Le même appariement complète les commandes `/`, les fichiers `@` et les programmes `!`.

## Souris {#mouse}

Dans le terminal un clic choisit une session, une touche de la barre du haut, une réponse de permission, un élément de liste, un réglage ; glisser sélectionne du texte (copié au relâchement) ; la molette fait défiler la transcription, les listes et les panneaux. Un clic droit sur une session ouvre son menu (ouvrir, renommer, décrire, déplacer, archiver, supprimer) ; un clic droit sur un message ou un bloc l'épingle ([Épingles](page:pins)).
