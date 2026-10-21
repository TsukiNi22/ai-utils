+++
title = "Voix"
summary = "Parler à la session globale : modes d'écoute, confirmation, mots, vocabulaire, moteurs et phrase de réveil."
section = "guide"
order = 50
keywords = "voix micro appuyer pour parler mot de réveil ok claude parole écoute piper kokoro whisper parakeet moteurs vocabulaire"
+++

La voix est optionnelle et locale par défaut : `[voice] enabled = true`. Elle utilise les outils `voice-listen` (parole vers texte, sherpa-onnx) et `voice-say` (texte vers parole, Piper) ; l'action de palette *Voice: what is missing* liste ce qu'il reste à installer (outils, enrôlement, voix, modèles, clés d'API).

## Modes d'écoute {#modes}

| Mode (`[voice] mode`) | Écoute | Prompt |
|---|---|---|
| `push` | appuyer pour parler : `F5` démarre, `F5` arrête | chaque phrase entendue pendant l'écoute |
| `auto` | toujours ; `F5` met en pause / reprend | chaque phrase entendue |
| `wake` | toujours | seulement après la phrase de réveil (`wake_word = "ok claude"`) : *ok claude, liste les sessions* l'envoie ; la phrase seule arme l'écoute pendant `wake_seconds` |

`--voice auto|wake|push` le fixe pour une exécution. Le modèle de parole se charge au démarrage (environ 10 s) ; `only_me = true` n'accepte que votre voix enrôlée (`voice-enroll`).

## Confirmation {#confirm}

Ce qui est entendu s'affiche en direct dans la barre de voix, puis attend `confirm_seconds` (3) avec sa cible et un compte à rebours, avec les boutons **Send now**, **Drop**, **Correct** (ou Entrée, Échap, Ctrl+C). Dites seul un **mot d'envoi** (*envoie*, *valide*, *go*, *send*) ou un **mot d'annulation** (*annule*, *stop*, *cancel*) et il agit aussitôt. Les mots sont dans `voice-words.txt` du dossier de config (réglages *Voice > Voice words*), un par ligne sous `[cancel]` et `[send]`. `<session>: ...` s'adresse à une session par son nom exact.

La session globale reçoit le texte marqué `[voice]` et redemande quand un mot semble mal entendu ou que la demande est ambiguë.

## Vocabulaire {#vocabulary}

Les noms propres que la reconnaissance déforme sont réécrits avant l'affichage de la phrase : `vocabulary.txt` (réglages *Voice > Vocabulary*) associe un nom aux façons dont il est entendu (`libutils = lib utils, lib you tils`). Les noms de vos sessions, les projets de la carte de votre ordinateur et vos propres noms sont ajoutés tout seuls.

## Réponses à voix haute {#speech}

Les réponses de la session globale sont lues en résumé court, dans leur langue (`speak`, `summary_chars`). `F6` ou `Ctrl+C` l'arrête. `say_engine` choisit le moteur (`piper` local, `kokoro`, ou `elevenlabs` / `cartesia` en offre gratuite ; un moteur en échec retombe sur Piper) et `say_voices` la voix par langue.

## Moteurs {#engines}

`listen_model` choisit le reconnaisseur : `parakeet` local (français, anglais, mélangé), `whisper` (99 langues, plus lent), `kroko-fr` (français seul, en direct, le plus rapide), ou les API gratuites `groq` et `deepgram` (clé stockée par `claude-cluster auth login groq`). `claude-cluster engines` liste chaque moteur avec ses limites, son usage gratuit et s'il est prêt.

## La page web et le téléphone {#web}

La page web utilise le micro du navigateur (https obligatoire) : *tap to talk* ou *hands-free*, chaque phrase reconnue par le modèle de parole de l'ordinateur. Voir [Interfaces](page:interfaces).

## Phrase de réveil fenêtre fermée {#service}

`claude-cluster service enable` installe une unité systemd utilisateur dont le détecteur léger entend la phrase de réveil et ouvre claude-cluster avec la voix active. Rien de ce qui est entendu n'est gardé : l'audio passe par un anneau de 0,5 s écrasé sur place. Voir [Réglages](page:settings#service).
