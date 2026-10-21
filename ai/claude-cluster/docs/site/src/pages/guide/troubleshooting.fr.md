+++
title = "Dépannage"
summary = "Les problèmes courants et où regarder : compilation, fenêtre, accès web, certificats, voix, application téléphone, journaux."
section = "guide"
order = 70
keywords = "dépannage problème erreur pare-feu port certificat avertissement qt fenêtre journal debug reconnexion cœur claude introuvable voix apk mise à jour"
+++

## Ça ne compile pas {#build}

- `find_package(utils)` échoue : libutils 4.0.0 ou plus est absent ou trop ancien ; installez-le (voir [Installation](page:install)).
- FTXUI ne se télécharge pas : la première configuration le récupère (réseau nécessaire) ; les suivantes le réutilisent.
- Trop de compilateurs à la fois figent l'ordinateur : utilisez `nice -n 19` et `--parallel 2` au plus.

## `--gui` dit qu'il n'y a pas de fenêtre {#window}

Le binaire a été compilé sans Qt6 Widgets (ou avec `-DCLUSTER_GUI=OFF`), ou il n'y a pas d'affichage. Installez le paquet de développement Qt6 et recompilez ; l'interface terminal marche toujours.

## L'agent est introuvable {#agent}

claude-cluster lance `claude` (ou `qwen`, `opencode`, `codex`) par les noms de `[commands]`. Vérifiez `claude --version` et `claude-cluster auth list` : il dit quels fournisseurs sont prêts et pourquoi pas. Un fournisseur à clé demande `claude-cluster auth login <fournisseur>`.

## La page web ne se charge pas {#web}

- L'accès est-il ouvert ? En bas à droite de l'interface : `● web https://<adresse>:8787 · code ...` ; `claude-cluster web` donne l'état et pourquoi l'extérieur n'est pas joint.
- Le pare-feu peut bloquer le port (`[web] port`, 8787) : `firewall-cmd --add-port=8787/tcp`.
- Le navigateur doit avoir une adresse privée d'un sous-réseau de l'ordinateur ; un nom public pointé vers le réseau local est refusé par conception.
- *Votre connexion n'est pas privée* : le certificat appartient à l'autorité de cet ordinateur. Approuvez-la une fois par appareil depuis la page de connexion (*trust this computer once*), ou sur cet ordinateur avec `claude-cluster web trust-ca`.
- Le code est refusé : 5 codes faux bloquent l'adresse 15 minutes et changent le code ; le nouveau code est sur l'ordinateur.
- Rien ne se passe après le code : l'appareil attend **votre accord** sur l'ordinateur (une fenêtre et une notification avec *Allow*).

## La voix ne marche pas {#voice}

Lancez l'action de palette *Voice: what is missing*. Le micro demande https dans un navigateur. `claude-cluster engines` dit quels moteurs sont prêts. Voir [Voix](page:voice).

## L'application téléphone {#apk}

Une application plus ancienne que celle que sert l'ordinateur est bloquée par une fenêtre jusqu'à sa mise à jour (*Install* : elle télécharge `/app.apk` depuis l'ordinateur). Pas de notification ? Autorisez les notifications (Android 13+) et *Keep running in the background* dans *This device* ; certains constructeurs tuent les services d'arrière-plan : autorisez le démarrage automatique de l'application. Un ordinateur éteint ou hors réseau est inatteignable ; il n'y a pas de push Google. Voir [Notifications et application téléphone](page:notifications-apk).

## Reconnexion {#core}

Une fenêtre *Reconnecting* signifie que le cœur est perdu (tué, planté, en redémarrage) : elle réessaie à 1, 2, 4... 10 s d'écart, relance le cœur s'il a disparu et reprend les sessions. **Retry now** (`r` / Entrée) ou **Quit** (`q`). Si une autre version fait tourner le cœur (réinstallée pendant qu'il tournait), une nouvelle interface demande s'il faut le remplacer.

## Où sont les journaux {#logs}

| Quoi | Où |
|---|---|
| journal du cœur | `~/.local/share/claude-cluster/core.log` |
| conversations | `~/.local/share/claude-cluster/logs/<id>.jsonl` |
| service | `claude-cluster service logs` (le journal de l'unité utilisateur) |
| trace de debug | `[debug] enabled = true`, puis `debug.log` dans le dossier de données |
| ce qui a échoué | une fenêtre *ce que vous avez fait, où ça a échoué, l'erreur* avec **OK** / **Retry** |

Un rapport de bug peut inclure la trace de debug, la version et le commit affichés en bas de ce site.
