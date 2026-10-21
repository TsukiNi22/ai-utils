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

Non couverts non plus : les tests dynamiques (fuzzing, test d'intrusion), les agents eux-mêmes, le code Android au-delà de son modèle de confiance, et libutils.
