+++
title = "Épingles"
summary = "Garder un message ou un bloc d'affichage au-dessus du prompt, quel que soit le défilement."
section = "concepts"
order = 70
keywords = "épingle désépingler session_pin bloc épinglé affichage liste à cocher"
+++

## Ce qu'est une épingle {#what}

Un message ou un bloc d'affichage d'une transcription (une liste de tâches, un formulaire, un bloc d'étapes, un tableau, une note, une réponse simple) peut être **épinglé** : il reste en bas de la vue de sa session, juste au-dessus du prompt, dans une zone compacte encadrée, quel que soit le défilement de la transcription. Son titre le replie, *Unpin* le retire, il défile à l'intérieur quand il est grand. Un bloc vivant (liste à cocher, étapes, progression, formulaire) reste interactif et est mis à jour sur place tant qu'il est épinglé.

Une épingle par session : en épingler une autre remplace la première. L'épingle suit la session (redémarrage, archivage / restauration ; `/clear` l'abandonne) et est indiquée par `session_list`, `session_status` et les infos de session.

## Comment épingler {#how}

| Où | Comment |
|---|---|
| terminal | clic droit sur un message ou un bloc, ou `Ctrl+K` : *Pin the last answer*, *Pin: <bloc>*, *Unpin: <titre>* |
| fenêtre | clic droit : *Pin this ...* / *Unpin* |
| page web | clic droit (appui long sur téléphone) : *Pin* / *Unpin* ; `Alt+P` épingle la dernière réponse ou désépingle |
| session globale | MCP `session_pin` (`display` : un id ou un bloc `{type, ...}`, ou `index` : une entrée de `session_transcript` ; `title`), `session_unpin`, `session_pin_get` |

## Qui peut épingler {#rights}

Vous, toujours. Les réglages `[agent] global_pin` (la session globale peut épingler sur n'importe quelle session, activé) et `sub_pin` (une sous-session ne peut épingler que sur elle-même, activé) contrôlent les agents.
