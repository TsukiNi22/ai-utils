+++
title = "Pièces jointes"
summary = "Envoyer fichiers, images et photos avec un prompt : où ils vont, ce qui est vérifié, et les réglages."
section = "concepts"
order = 80
keywords = "pièces jointes fichiers images photos téléversement trombone glisser déposer coller exif lien global_max caméra"
+++

## Joindre {#attach}

Fichiers, images et photos partent avec un prompt comme dans une messagerie : le **trombone** à côté du prompt (fenêtre, page web, application), le **glisser-déposer** sur le prompt, le **collage** (`Ctrl+V` d'une image ou de fichiers). Le terminal garde `@chemin` et le collage d'image. Sur téléphone le sélecteur propose *Take a photo / video* (l'appareil photo, demandé une fois avec sa raison). Une pastille par pièce jointe s'affiche au-dessus du prompt avec un **✕** pour la retirer avant l'envoi.

Une image arrive à l'agent comme l'image elle-même (`[Image #N]`) ; tout autre fichier en `@chemin` (texte, code, PDF...). La transcription les montre avec une vignette et un lien pour ouvrir ou télécharger.

## Ce qui est vérifié {#checks}

- **Téléversement** : le fichier est envoyé au cœur et stocké dans `<data>/attachments/<session>/<id>-<nom>` en mode `0600`, jamais exécutable. Le nom est assaini, le type est **deviné d'après le contenu** (un programme, ELF ou PE, est refusé), les images sont ré-encodées **sans EXIF** et réduites à `max_pixels` (demande ImageMagick).
- Seul un appareil authentifié téléverse ; une session ne lit que son propre dossier, qui part avec la session quand elle est supprimée pour de bon.
- **Lien plutôt que copie** : un fichier de **cet** ordinateur joint par son chemin devient un lien symbolique, rien n'est copié et l'original n'est jamais touché. Un téléphone ou un autre ordinateur téléverse toujours. `[attachments] link = false` le désactive.

## La session globale {#global}

Elle voit les pièces jointes dans `session_messages` (chemin, type, taille) et peut joindre des fichiers locaux avec `session_send` (`attachments: [chemins]`), seulement depuis le dossier du projet de la session et votre dossier personnel (aucun dossier caché, rien de claude-cluster lui-même), au plus `global_max` fichiers. `[attachments] global = false` l'interdit.

## Réglages {#settings}

`[attachments]` (page de réglages *Attachments*) : `enabled`, `max_mb` (20), `max_files` (10), `keep` (200 par session, les plus anciennes partent), `max_pixels` (2048), `strip_exif`, `dir`, `link`, `global`, `global_max` (5). Dans l'application Android le sélecteur système ne donne accès qu'aux fichiers choisis, donc aucune permission média n'est demandée.
