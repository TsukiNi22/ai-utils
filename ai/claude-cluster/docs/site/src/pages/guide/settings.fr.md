+++
title = "Réglages"
summary = "La page de réglages, config.toml, valeurs dangereuses, réglages protégés, l'onglet Services et la recherche d'un réglage."
section = "guide"
order = 60
keywords = "profiles profils sync shared partagés réglages config.toml f9 dangereux protégés services service systemd recherche profils fournisseurs styles"
+++

## La page de réglages {#page}

`F9` (ou la palette, *Settings* ; le ⚙ de la page web) ouvre une page façon BIOS : un onglet par section (Interface, Agent, Commands, Global session, Sessions, Voice, Keys, Web, Attachments, Notifications, Orchestrator, Services...), l'aide de l'option choisie à droite.

- Terminal : `↑↓` choisir, `←→` ou `Entrée` changer un choix, basculer ou modifier un nombre, `Entrée` édite un texte, `Tab` / `Shift+Tab` changent de section, `Ctrl+S` sauvegarde (la page reste), `F10` sauvegarde et quitte, `Échap` quitte (changements non sauvegardés : *Save the changes?*). `*` marque les modifiés.
- Les changements sont écrits dans `config.toml` (seulement les valeurs, ses commentaires conservés) et rechargés à chaud. Les options marquées *applied at the next start* demandent un redémarrage. Chaque option est listée dans la [référence des réglages](page:settings-reference), et son défaut dans la [référence de configuration](page:config-reference).
- Les profils (`[profiles.<nom>]`), fournisseurs propres (`[providers.<nom>]`) et styles propres (`[styles.<nom>]`) restent dans le fichier.

## Chercher un réglage {#search}

`/` ou `Ctrl+F` dans la page, ou `setting:` dans la palette. La recherche tolère les fautes, porte sur le nom bien avant la description, et dit où elle a trouvé. La recherche de la documentation, en haut de chaque page, couvre les mêmes réglages, outils et touches.

## Valeurs dangereuses {#danger}

Un **⚠** jaune marque chaque réglage qui peut être dangereux et son nom devient **rouge** tant que sa valeur est la dangereuse : `bypassPermissions` pour les nouvelles sessions ou la session globale, `allow_bypass`, `bypass_to_global`, des `extra_args` qui sautent les permissions, l'accès web sans approbation, ouvert à Internet (`remote = router`), son shell, http simple, un serveur MCP ajouté. En changer un ne demande rien ; la sauvegarde les liste avec leurs avertissements dans une seule fenêtre. `:set` exige `:set!` pour eux et le socket de contrôle / le MCP refusent sans `confirmed`.

## Réglages protégés {#protected}

La session globale peut changer la plupart des réglages (`settings_set`) mais jamais les **protégés** : le web ouvert plus largement, les permissions des agents, les programmes lancés comme vous (`commands.*`, éditeurs, visionneuses), les limites et la politique de modèles, les sources de plugins, les serveurs MCP. Elle vous demande, et vous les changez ici. La liste est dans [Sécurité](page:security#global).

## Ce dont un réglage a besoin {#needs}

Ce qui manque à un réglage est dit en rouge à sa droite (`✗ ...`) : un programme introuvable (éditeur, visionneuse, agent), `upnpc` pour `remote = router`, `tailscale` non connecté. Quand un paquet l'installe, `[i]` / *Install...* le fait (le terminal demande `sudo`, la fenêtre et la page web utilisent la fenêtre de mot de passe du bureau).

## L'onglet Services {#service}

Une carte par sous-service, avec son état et ses boutons :

- **Service (systemd)** : la phrase de réveil pendant que claude-cluster est fermé (`claude-cluster service setup | enable | disable | start | stop | restart | status | logs | remove`). L'unité `~/.config/systemd/user/claude-cluster.service` démarre avec la session graphique ; ce qu'elle ouvre est *Interface > Front-end* (une fenêtre, sinon un terminal).
- **Web access** : état, adresse, code, si l'extérieur est joint ; ouvrir / fermer, nouveau code.
- **Core** : pid, version et build, qui l'a lancé, ses interfaces par genre, sessions au travail, combien de temps il restera en arrière-plan ; redémarrer, tout arrêter.
- **Voice**, **MCP servers**, **Outside (router / tailscale)**.

## Profils {#profiles}

Chaque claude-cluster de l'ordinateur partage `config.toml` et le relit quand il change : un réglage changé quelque part l'est partout. **Réglages > Profiles** garde des copies nommées de tous les réglages : *Save the settings of now as…* (n'importe quel nom), puis sur chaque profil **Apply** (les réglages d'avant gardés : *Undo the last apply*), **Save the settings of now into it**, **Rename…**, **Note…**, **Archive** / **Unarchive**, **Delete** (la corbeille 7 jours : *Restore* ou *Delete for good*, puis supprimé). Les mêmes cartes dans le terminal, la fenêtre, la page et l'app.

## Comptes et limites {#accounts}

*Accounts & limits* liste chaque fournisseur avec son état (clé stockée, connexion faite, ou ce qui manque). `claude-cluster auth login <fournisseur>` demande une clé sans écho et la stocke dans le trousseau (`secret-tool`) ou `credentials.json` (`0600`), jamais dans `config.toml`. Les **limites** plafonnent ce que les sessions peuvent dépenser ensemble par jour, semaine et mois (`[limits]`), comptées pour les clés d'API seulement (`scope = keys`) ou tout, avec *Block at the limit* et *Warn at* une part.

## Plugins et skills {#plugins}

La page **Plugins** (`/plugins`, la palette *Plugins*, dans chaque interface) liste d'abord les **sources** - un dépôt, un dossier, un `.zip` - chacune avec ce qu'elle propose, a installé et a activé, sa dernière synchronisation et son dernier commit (ou sa date). Une source ouverte montre son offre en **liste à cocher** : cochez-en plusieurs (ou toutes), puis installez, retirez, activez ou désactivez les cochées d'un coup. Chaque source peut être synchronisée tout de suite, installée ou retirée en entier, désactivée (tout ce qu'elle a désactivé, gardé installé) puis réactivée, voir sa **synchro auto** coupée (tirée à la main seulement), ou être retirée. *Sync all* tire toutes les sources ; `[plugins] sync_hours` (24) fixe la fréquence des sources en auto. La page de réglages ne garde que les sources et cette fréquence. Le code d'une source s'exécute en votre nom : n'ajoutez que des sources de confiance.

## Debug et la vitrine {#debug}

*Debug* a une **vitrine** qui montre pour de vrai tout ce que claude-cluster sait dessiner (blocs d'affichage, fenêtres de question et de permission, états de session, fenêtres de progression et d'échec, notifications) dans une session à part dont l'agent n'est jamais sollicité : rien n'est dépensé.
