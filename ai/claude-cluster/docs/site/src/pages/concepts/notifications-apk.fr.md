+++
title = "Notifications et application téléphone"
summary = "Notifications du bureau, visage de l'icône, et l'application Android qui suit vos sessions depuis votre téléphone."
section = "concepts"
order = 120
keywords = "notifications icône visage android application apk téléphone service sondage notify_app canaux batterie permission POST_NOTIFICATIONS"
+++

## Notifications du bureau {#desktop}

La page de réglages *Notifications* les active ou désactive toutes, choisit *quand* (`unfocused` : seulement quand claude-cluster n'est pas au premier plan, `always`), chaque genre (la session globale a répondu, une session a fini, une permission, une question, une erreur, les limites, les appareils web, la file de tâches, les rappels, quelque chose de lent est prêt) et le son. Une notification cliquée (son corps ou *Open*) montre sa session. Jamais deux fois : celui qui a envoyé le dernier prompt d'une session est notifié pour elle, ici ou sur l'appareil web qui l'a envoyé. Une notification par session, urgente quand elle vous attend, avec des boutons dont la réponse part de la notification (les options d'une question, *Allow* / *Deny*).

## Le visage de l'icône {#icon}

L'icône est une pile de terminaux dont celui de devant a un visage fait de caractères de prompt ; le visage suit ce que fait claude-cluster, dans le titre du terminal, l'icône de la fenêtre, l'onglet de la page web, le dock (le nombre de sessions qui vous attendent, sinon en cours) et `claude-cluster icon [visage] [badge]` (son SVG, pour une barre d'état).

| Visage | L'application maintenant |
|---|---|
| `^_^` happy | la session globale vient de répondre |
| `>‿<` grin | une session vient de finir |
| `•_•` attentive | une question ou une permission attend, la voix écoute |
| `o_o` waiting | une erreur, le contrôle de sécurité de Claude Code |
| `-_-` idle | rien ne tourne |
| `>_<` squint | quelque chose tourne |

## L'application Android {#app}

Quand l'accès web est ouvert, la page propose une application sur `/app.apk`. Elle est compilée par `android/build.sh` (le seul SDK Android, sans Gradle) et signée par une clé à vous, `~/.config/claude-cluster/android/release.jks`.

- C'est la page web dans une coque légère : **l'ordinateur est approuvé une fois** (son autorité épinglée après que son empreinte a été montrée et comparée, donc sans avertissement), le micro et les notifications fonctionnent, plein écran sur Android 15.
- Onglets : *Chat* (le dernier chat ouvert, la session globale d'abord), *Sessions*, *This device* (version, build, ce téléphone ; *Check for updates* : installée depuis votre ordinateur, rien à télécharger ailleurs ; genres de notifications par téléphone).
- Une application **plus ancienne** que celle que sert l'ordinateur est bloquée par une fenêtre jusqu'à sa mise à jour (*Install*) ; une plus récente sur l'ordinateur est proposée, jamais une plus ancienne ; une application trop ancienne pour ce claude-cluster le dit.

## Notifications du téléphone {#phone}

L'application montre **chaque notification du bureau**. Le cœur journalise chacune (les 100 dernières, genres et `[ui] notify_*` comme sur le bureau) et l'application suit ce journal par une longue interrogation, `GET /api/notify?since=N` (réponse immédiate s'il y en a une nouvelle, sinon après 25 s). `[ui] notify_app = false` n'en publie aucune. Chaque téléphone choisit pour lui dans *This device* : activé / désactivé et quels genres, *Send a test*, *Allow notifications* (Android 13+, `POST_NOTIFICATIONS`) et *Keep running in the background*.

Elles s'affichent quand l'application **n'est pas au premier plan**, dans des canaux Android par famille (*Waiting for you*, *Sessions*, *Errors and limits*, *Reminders and messages* : haute ; *Information* : basse), groupées par session, un appui ouvre cette session. Le logo est l'icône de la barre d'état (un masque blanc plat généré à chaque densité) et la grande icône.

Il n'y a **aucun service Google**, donc pas de push : un service d'avant-plan garde sa propre connexion à l'ordinateur (une discrète notification *following your sessions*, lancée quand l'application quitte le premier plan et après un redémarrage ou une mise à jour, se reconnectant avec un délai croissant). Il ne peut pas joindre un ordinateur éteint ou hors réseau, ne survit pas à un Android qui tue le service malgré l'exemption batterie (certains constructeurs le font : autorisez le démarrage automatique), et ne peut pas répondre à une permission depuis la notification (elle ouvre la session). Loin de chez vous, utilisez l'accès web *Beyond the LAN* avec Tailscale ([Sécurité](page:security)).
