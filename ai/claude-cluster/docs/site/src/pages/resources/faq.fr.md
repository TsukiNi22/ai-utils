+++
title = "FAQ"
summary = "Réponses courtes : données et vie privée, agents et modèles, accès de l'extérieur, plateformes, questions courantes."
section = "resources"
order = 30
keywords = "faq questions vie privée données licence mit ollama windows mac iphone gratuit extérieur bypass tokens coût"
+++

## Données et vie privée {#privacy}

**claude-cluster envoie-t-il mes données quelque part ?** Aucune télémétrie. Le seul trafic réseau est ce que vous lancez : vos agents parlent à leur fournisseur (Anthropic, OpenAI, Ollama sur votre machine...), les API de voix optionnelles (`groq`, `deepgram`, `elevenlabs`, `cartesia`) que vous avez configurées, le point d'accès d'usage de claude.ai lu avec votre jeton Claude Code, et l'accès web que vous avez ouvert. Cette documentation est servie par votre propre ordinateur et n'utilise aucune ressource externe.

**Où sont mes conversations ?** Sur votre ordinateur, dans le stockage propre de l'agent et dans `~/.local/share/claude-cluster/logs/`. Voir [Formats de données](page:data-formats).

**Que lit la session globale ?** La carte de votre ordinateur (`machine.md`) est lue par elle et donc envoyée à son fournisseur d'API ; l'historique des prompts garde en clair ce que vous avez tapé dans votre dossier de données. Voir [Sécurité](page:security#privacy).

**Est-ce conforme au RGPD ?** Le RGPD encadre le traitement de données personnelles par un responsable de traitement. claude-cluster est un logiciel que vous exécutez vous-même : ses données restent sur votre ordinateur, aucun service de notre part ne les reçoit, donc il ne s'applique pas à claude-cluster lui-même. Il s'applique toujours à ce que vous faites avec les fournisseurs que vous connectez.

## Agents et modèles {#agents}

**Ai-je besoin de Claude ?** Non : Claude Code est le pilote par défaut et le complet, mais Ollama (local), les points d'accès compatibles Anthropic, ceux compatibles OpenAI (Qwen Code), opencode et le CLI OpenAI Codex fonctionnent aussi. La session globale a besoin d'un fournisseur du pilote claude.

**Pourquoi la session globale tourne-t-elle en `bypassPermissions` ?** Elle orchestre : demander avant chaque outil irait contre le but. Les outils qu'elle détient sont bornés et les réglages protégés sont hors de sa portée ; vous pouvez mettre `[global] permission_mode = "auto"` pour qu'elle demande. Voir [Permissions](page:permissions-models).

**Combien ça coûte ?** claude-cluster est gratuit (licence MIT) ; les modèles coûtent ce que facture leur fournisseur. Les panneaux *Tokens*, *Cost* et *Usage* et `/usage` l'affichent, et `[limits]` le plafonne par jour, semaine et mois.

## Utilisation {#using}

**Catégorie ou tag ?** Une catégorie par session (son étiquette principale), plusieurs tags. Les deux vivent dans la [taxonomie](page:taxonomy).

**Comment repartir de zéro ?** Palette *Start over* : toutes les sous-sessions vont à la corbeille, les tâches en file s'arrêtent et la session globale reçoit une nouvelle conversation.

**Une session est bloquée.** `Ctrl+C` arrête le tour, F4 ou `/retry` redemande après une erreur d'API, `/continue` reprend après un arrêt. Une session qui s'est arrêtée est relancée sur sa conversation.

**Deux sessions peuvent-elles partager un dossier ?** Oui ; ce sont des processus indépendants, donc elles peuvent se marcher sur les fichiers comme deux terminaux. Utilisez un fork, un worktree ou une session par dossier.

## Accès de l'extérieur {#outside}

**Puis-je l'utiliser depuis mon téléphone loin de chez moi ?** Oui : `[web] remote = tailscale` (recommandé : rien n'est ouvert à Internet) ou `router`. L'application Android suit toute seule. Voir [Interfaces](page:interfaces#web) et [Sécurité](page:security).

**Y a-t-il une application iPhone ?** Non : la page web fonctionne dans le navigateur de l'iPhone, avec les étapes du certificat sur la page de connexion.

## Plateformes {#platforms}

**Quels systèmes ?** Linux : Fedora et Debian / Ubuntu sont documentés. La fenêtre demande Qt6, la voix ses outils locaux, l'application téléphone le SDK Android à la compilation.

**Où signaler un bug ?** Sur le dépôt, avec la version et le commit affichés en bas de cette page et, si utile, la trace de debug (`[debug] enabled = true`).
