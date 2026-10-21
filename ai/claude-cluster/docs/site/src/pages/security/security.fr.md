+++
title = "Vue d'ensemble de la sécurité"
summary = "Modèle de menaces pour l'usage local, le réseau local, l'accès web et l'application téléphone ; permissions, secrets et vie privée."
section = "security"
order = 10
keywords = "sécurité modèle de menaces permissions secrets authentification certificat tailscale réglages protégés vie privée rgpd bypass accès web réseau local"
+++

claude-cluster pilote des agents qui exécutent des commandes avec vos droits, donc sa sécurité tient à **qui peut parler au cœur** et à **ce qu'un modèle manipulé peut faire**. Les constats détaillés de l'audit du code sont dans l'[audit de sécurité](page:audit-report).

## Qui peut atteindre quoi {#model}

| Acteur | Atteint | Confiance |
|---|---|---|
| vous, au clavier | tout | totale |
| tout programme exécuté par vous | le socket de contrôle et vos dossiers de config / données | celle de votre compte |
| un appareil de votre réseau local | la page https ; rien d'autre avant le code **et** votre accord | aucune, puis approuvé |
| un appareil de l'extérieur (Tailscale, routeur) | pareil, https seulement, accord toujours demandé | pareil |
| la session globale (un modèle) | les outils MCP, bornés par les réglages protégés | faible : elle obéit au texte qu'elle lit |
| une sous-session | son propre petit MCP | selon son mode de permission |

La conception suppose que les modèles peuvent être **manipulés par le texte qu'ils lisent** (une page web, un fichier, une réponse) : les garde-fous servent à borner ce que ce texte peut leur faire faire.

## Usage local {#local}

Tout passe par un socket unix de votre dossier d'exécution (`0700`, le socket en `0600`) : seul votre utilisateur (et root) l'atteint. Tout programme exécuté par vous, un script ou un agent en `bypassPermissions`, peut piloter claude-cluster par lui : la même confiance que votre compte. Les clés d'API vivent dans le trousseau (`secret-tool`) ou dans `credentials.json` (`0600`), jamais dans `config.toml`, les journaux ou les transcriptions ; les agents des sessions les reçoivent sur un tube, pas sur leur ligne de commande. Les dossiers de config et de données sont en `0700`. Les programmes lancés avec vos droits (`commands.*`, éditeur, visionneuse, commandes de voix) sont confirmés avant de changer.

## Accès web {#web}

- **https par défaut** avec une autorité propre à votre ordinateur (clé `0600`), TLS 1.2 ou plus ; une requête en clair est redirigée.
- **Deux facteurs par appareil** : le code à 6 chiffres (nouveau à chaque ouverture ; un mauvais code est répondu 1 s plus tard, 5 mauvais bloquent l'adresse 15 minutes et changent le code) **et** votre accord sur l'ordinateur. Un appareil de confiance (son propre secret de 32 octets) revient seul.
- **Les cookies** sont `HttpOnly`, `SameSite=Strict` et `Secure`, et la session est **liée à l'adresse** de l'appareil.
- **Portée** : seulement les adresses privées d'un sous-réseau de votre ordinateur, la plage du tailnet, ou (avec `remote = router`) toute adresse en https ; le `Host` est vérifié (pas de DNS rebinding) et un `POST` exige le bon `Origin`.
- **La page** : un nonce CSP par réponse, `X-Frame-Options: DENY`, `nosniff`, HSTS ; tout ce qui est affiché est échappé ; les pièces jointes sont isolées. 8 connexions par adresse et 64 en même temps au plus.
- **Ce qu'un appareil peut faire** : tout ce que fait le clavier, sauf un shell (`!commande`) et les réglages que vous ne lui avez pas ouverts (`[web] settings`). Un téléphone de confiance perdu ou déverrouillé, c'est votre claude-cluster : *Web devices > trust no more / ban*, et gardez le shell coupé sauf besoin.
- **De l'extérieur** : `tailscale` n'ouvre rien à Internet ; `router` rend la page de connexion joignable par n'importe qui, donc le code, votre accord et les bannissements sont ce qui tient. Le code, la conversation et les cookies sont chiffrés de bout en bout et la clé ne quitte jamais votre ordinateur.

> [!WARN]
> L'interrupteur du shell (`[web] shell`) n'empêche pas à lui seul un appareil approuvé de demander à un agent d'exécuter une commande : traitez chaque appareil approuvé comme une personne devant votre clavier. Voir l'audit, constats F7 et F8.

## La session globale et ses outils {#global}

La session globale tourne en `bypassPermissions` par défaut, donc elle obéit à ce qui lui parvient avec vos droits : c'est le but d'un orchestrateur et son risque, car un prompt injecté dans une sous-session lui revient comme réponse. Ses outils MCP s'exécutent sans demande, donc ce qu'ils peuvent faire est borné :

- les **réglages protégés** ne sont jamais changés par elle (`settings_set` et `config_write` les refusent : elle vous demande) : le web ouvert plus largement, les permissions des agents, les programmes lancés comme vous, les serveurs et outils MCP, les limites et la politique de modèles, les sources de plugins, et les interrupteurs `rewrite_*` ;
- elle n'écrit que des fichiers `.md` / `.txt` hors des dossiers cachés (`export`), ne donne jamais un fichier caché (`~/.ssh`, `~/.config`, les dossiers de claude-cluster) à une session comme contexte, et ne réécrit les prompts système que si vous l'avez permis ;
- **les touches de voix et les actions de palette** (connexion / déconnexion, tout quitter, recommencer) sont à l'utilisateur : jamais lancées par ses outils ni par la page web.

> [!NOTE]
> Ces limites bornent les **outils** de la session globale, pas le processus de la session lui-même, qui a son propre shell et ses outils de fichiers en `bypassPermissions`.

> [!WARN]
> Constaté par l'audit dans cette version : les interrupteurs `global_answers`, `global_fork` et `global_edit` sont censés être protégés mais une erreur de nom les laisse modifiables par la session globale (F2), la règle des fichiers cachés oublie le `path` d'un fichier affiché (F3), et une sous-session peut exécuter une commande par `heavy_run` sans demande (F1). Les corrections sont recommandées dans l'[audit](page:audit-report).

## Secrets {#secrets}

Clés d'API : trousseau ou fichier `0600`, données aux agents sur un tube. Le jeton de Claude Code n'est lu que pour interroger le point d'accès d'usage de claude.ai, passé par un fichier d'en-têtes lisible par vous seul, supprimé aussitôt, jamais journalisé. L'application Android épingle l'autorité de votre ordinateur la première fois (son empreinte affichée), ses mises à jour viennent de l'ordinateur et gardent sa clé de signature. `/ca.crt`, `/app.apk`, `/api/version` et `/docs` sont servis à quiconque atteint la page : rien de secret dedans.

## Vie privée et RGPD {#privacy}

Tout reste sur votre ordinateur : conversations, historique des prompts (en clair, dans votre dossier de données), pièces jointes, clés, carte de votre ordinateur. Il n'y a aucun serveur du projet, aucune télémétrie et aucun compte. Le **RGPD ne s'applique pas à claude-cluster lui-même** puisque le projet ne collecte ni ne traite de données personnelles : les données restent locales. Il s'applique toujours à ce que vous choisissez d'envoyer à un fournisseur : la carte de votre ordinateur (`machine.md`), les prompts et les fichiers qu'une session envoie sont lus par le fournisseur d'API que vous avez connecté.

## Signaler un problème {#report}

Ouvrez un ticket sur [le dépôt](https://github.com/TsukiNi22/ai-utils) avec la version et le commit affichés en bas de cette page. Pour une vulnérabilité, ne publiez pas les détails avant que l'auteur ait eu le temps de corriger.
