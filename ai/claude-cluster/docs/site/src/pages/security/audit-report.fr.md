+++
title = "Rapport d'audit de sécurité"
summary = "Un audit par lecture du code de l'accès web, du socket de contrôle, des pièces jointes, des garde-fous de permission, de la protection des prompts et de l'orchestrateur - avec un PDF à télécharger."
section = "security"
order = 20
keywords = "rapport audit pdf sécurité constats authentification web socket de contrôle pièces jointes permissions orchestrateur rgpd mythos"
+++

> [!OK]
> **Télécharger le rapport** : [claude-cluster-audit-report.pdf](downloads/claude-cluster-audit-report.pdf), 6 pages, écrit à partir de la lecture du code de la version 1.4.62 (commit `2139c37`). Le rapport est rédigé en anglais.

## Ce qu'il couvre {#scope}

| Domaine | Question posée |
|---|---|
| authentification de l'accès web et des appareils | peut-on entrer, ou garder une session, sans le code et votre accord ? |
| le socket de contrôle | qui peut l'atteindre, et qu'est-ce qui le protège ? |
| accès aux fichiers des pièces jointes | un chemin peut-il sortir des dossiers autorisés ? |
| les garde-fous de permission du shell et du MCP | qu'est-ce qui empêche un modèle ou un appareil d'exécuter une commande ? |
| protection des prompts et des réglages | que peut réécrire la session globale ? |
| l'orchestrateur | une session peut-elle s'en servir pour contourner son mode de permission ? |

Méthode : chaque point d'entrée a été suivi depuis l'endroit où une requête arrive jusqu'au garde-fou et à l'action, et chaque garde-fou comparé à ce que promettent le README et l'aide des réglages. Rien n'a été exploité sur une instance en marche ; aucun test dynamique.

## Constats en un coup d'œil {#findings}

| Id | Niveau | Constat |
|---|---|---|
| F1 | **Élevé** | une sous-session exécute des commandes shell quelconques par `heavy_run` sans demande de permission |
| F2 | Moyen | `sessions.global_fork`, `global_edit`, `global_answers` sont censés être protégés mais ne le sont pas (une erreur de nom) |
| F3 | Moyen | le garde-fou des fichiers cachés des affichages vérifie `files`, pas `path` / `paths` |
| F4 | Moyen | le repli `/tmp/claude-cluster-<uid>` du dossier d'exécution est prévisible et non vérifié |
| F5 | Moyen | l'autorité de certification locale vaut 20 ans sans contrainte de noms |
| F6 | Moyen | une exécution ponctuelle de description / chat met une clé d'API sur une ligne de commande |
| F7 | Faible | `[web] settings = all` revient à une exécution de code |
| F8 | Faible | `web.shell = false` arrête `!commande`, pas un agent à qui on demande de l'exécuter |
| F9 | Faible | jeton comparé par `==` ; tout appareil peut changer le code |
| F10 | Faible | les fichiers jusqu'à 512 Mo sont lus en entier en mémoire |
| F11-F13 | Info | ordre de création des fichiers, portée des protections, générateur d'identifiants de pièces jointes |

Ce sont des constats sur le code du commit lu, pas sur une version publiée ; ils sont signalés pour être corrigés. Le rapport détaille la preuve, l'impact et une correction pour chacun, puis liste ce qui a été vérifié et tient.

## Ce qui tient {#holds}

La connexion web (un code comparé en temps constant, 5 mauvais codes qui bloquent une adresse, votre accord, des cookies liés à l'adresse de l'appareil), les contrôles Host et Origin, les en-têtes et le nonce CSP, les limites de requêtes, les permissions du dossier du socket et des secrets, les contrôles de chemin des pièces jointes, la détection du type des téléversements et le passage des secrets sur un tube pour les sessions sont tous vérifiés dans le code. Voir la [vue d'ensemble de la sécurité](page:security).

## Vie privée et RGPD {#gdpr}

Les données de claude-cluster restent sur votre ordinateur et le projet n'a aucun serveur, donc **le RGPD ne s'applique pas à claude-cluster lui-même**. Il s'applique à ce que vous envoyez aux fournisseurs que vous connectez.

## Ce qui n'y est pas {#next}

> [!SOON]
> Un audit de sécurité plus large du dépôt, mené avec l'audit *mythos* de Claude Code, arrive bientôt et ne fait pas partie de ce rapport.

Non couverts non plus : les agents eux-mêmes, le code Android au-delà de son modèle de confiance, et libutils. Les tests dynamiques ne faisaient pas partie de ce rapport : le second audit ci-dessous les a menés.

## Le second audit, version 1.10.0 {#second}

Tout claude-cluster relu en plusieurs passes, chacune avec AddressSanitizer + UBSan et ThreadSanitizer sur la suite de tests, du fuzzing de l'accès web (`tests/web_fuzz.py`) et des affichages (`tests/display_shapes.py`), chaque constat reproduit ou suivi jusqu'à son chemin de code exact, puis corrigé avec son test de non-régression.

| Id du premier audit | Maintenant |
|---|---|
| F1 | soumis à `[orchestrator] sub_add` |
| F2, F3, F4, F6 | **corrigés** (les noms protégés, les chemins cachés d'un affichage, le dossier d'exécution, la clé d'API hors de la ligne de commande) |
| F9 | le jeton comparé en temps constant ; un appareil peut toujours changer le code par de mauvais codes (l'appairage seulement, rien d'autre) |
| F10 | **corrigé** : les fichiers de la page sont envoyés en flux |
| F5 | **ouvert** : l'autorité locale est valable 20 ans sans contrainte de nom ; la changer demande à chaque appareil de faire confiance à une nouvelle (une décision pour vous) |
| F7, F8 | documentés dans la [vue d'ensemble de la sécurité](page:security) |

Ce que le second audit a trouvé et corrigé, le plus grave d'abord :

- **`[web] shell = false` pouvait être contourné** par un appareil de la page : `:set web.shell true` dans un prompt, une planification ou une tâche lançant une commande, un caractère d'étiquette avant `!`. Le contrôle est maintenant à un seul endroit, sur chaque façon d'envoyer un prompt.
- **Ce qui terminait le cœur** : du JSON imbriqué très profond (une ligne de 200 000 `[` : la pile épuisée), une requête utilisant ce que la fermeture avait détruit, une exception sortant d'un thread, deux pages ouvrant le terminal en même temps, un enregistrement de l'état concurrent d'un autre (`state.json` corrompu, les sessions perdues au lancement suivant).
- **Ce qui terminait un terminal ou prenait la mémoire de l'ordinateur** : un affichage d'une forme inattendue (à nouveau à chaque lancement : il reste dans le journal), les nombres d'un graphique (`[1e300, -1e300, 1]` : une barre sans fin).
- **Maintien au chaud** : un réveil reprenant son propre prompt arrêtait les shells et agents en arrière-plan, et pouvait effacer un vrai prompt donné au même moment.
- **Un collage n'est jamais des touches** : une lettre d'un texte collé pouvait autoriser un appareil, quitter, lancer une mise à jour ; dans le terminal de la page, une fin de collage dans un texte copié faisait du reste des touches.
- `/api/listen` et `/api/attach` étaient mélangés depuis l'ajout de la voix de la page : un fichier envoyé partait vers le moteur de reconnaissance vocale.

Une relecture indépendante de chaque sous-système a suivi ces corrections et confirmé 99 points de plus, chacun reproduit ou suivi jusqu'au code, tous corrigés :

- **Le shell, coupé, restait atteignable** : `Ctrl+E` sur la page terminal ouvrait un éditeur (`:!cmd`, `:w ~/.bashrc`) ; une `command` qui n'est pas du texte dans une planification ou une tâche passait par `/bin/sh` ; un octet NUL dans un chemin trompait le contrôle des fichiers cachés, et un appareil pouvait écraser `~/.bashrc`.
- **Un fichier d'un agent, ouvert par un appareil, s'exécutait dans l'origine de la page** : un formulaire de cette page pouvait envoyer n'importe quelle commande. Il s'ouvre maintenant comme du texte, et un simple formulaire n'atteint plus les routes qui prennent du JSON.
- **Secrets** : un affichage donné avec un épinglage lisait `~/.ssh/id_rsa` sans contrôle, la session globale pouvait le joindre quand la session est dans le dossier personnel, et `voice.words` pouvait la diriger vers n'importe quel fichier.
- **Ce qui arrêtait le cœur ou un terminal** : les réglages réécrits sous les threads qui les lisent, un motif de parenthèses profondes dans `:s`, deux requêtes `usage` en même temps, `max_cost: inf` écrit comme un null qui bloquait tous les démarrages suivants, `: ` tapé dans le prompt, un affichage ou une question d'une taille ou d'une forme inattendues.
- **Ce qui perdait du travail** : une reprise de keep warm coupant un vrai tour ou un vrai prompt, la réponse à la question de restauration remplaçant la conversation de la session globale, un prompt envoyé pendant un remplacement, une tâche envoyée à la session d'un autre projet après un redémarrage, le menu de démarrage supprimant une instance dont l'état était illisible.

- **Ce que faisait un gros log** : le fichier de sortie d'un shell en arrière-plan était relu en entier à chaque image de sa liste, dans le terminal, la fenêtre et la requête `agents` (des gigaoctets de mémoire pour une compilation bavarde), et une fifo donnée comme fichier bloquait le thread pour toujours. Seuls ses derniers octets sont lus maintenant, et seulement dans un fichier normal.

La liste complète, avec la preuve de chacun, est dans le rapport `audit/2026-11-03-security.pdf` du dépôt.
