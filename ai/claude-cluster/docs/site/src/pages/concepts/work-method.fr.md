+++
title = "Méthode de travail"
summary = "Dire à claude-cluster comment vous aimez organiser le travail : préréglages, votre texte, et la proposition issue de vos habitudes."
section = "concepts"
order = 90
keywords = "méthode de travail préréglage list-review one-task plan-first custom détection work_method proposition"
+++

**Réglages > Agent > Work method** (`[agent] work_method`, `work_method_text`) dit comment vous aimez organiser le travail. La session globale découpe ce qu'elle répartit en conséquence. Vous seul la changez : le réglage est protégé.

## Préréglages {#presets}

| Préréglage | Sens |
|---|---|
| `free` | aucune méthode (défaut) |
| `list-review` | une liste de tâches, puis une revue, répétées jusqu'à satisfaction : la session globale découpe une demande en liste numérotée (ou un `session_fanout`), demande une revue à une session, renvoie les défauts en nouvelle liste, jusqu'à ce qu'une revue ne trouve plus rien |
| `one-task` | une tâche à la fois : une tâche par prompt, sa réponse attendue avant la suivante |
| `plan-first` | un plan d'abord (sans modification), vous est montré, puis le feu vert |
| `custom` | seulement votre propre texte |

Votre **texte libre** (400 caractères au plus) s'ajoute au préréglage. La méthode est injectée, brève, dans l'ajout au prompt système des sous-sessions et dans les consignes de routage de la session globale ; chacune la prend à son prochain démarrage. L'outil MCP `work_method` permet à la session globale de la lire.

## Propositions issues de vos habitudes {#detect}

Avec `work_method_detect = true` (désactivé par défaut) la session globale peut **proposer** une méthode d'après la façon dont vous formulez vos demandes. Son outil reçoit un petit échantillon lu dans les transcriptions **sans appel de modèle** : comptes de prompts avec listes de tâches, demandes de revue, demandes de plan, prompts d'une ligne, et une dizaine de vos prompts récents, 220 caractères chacun. Cela n'arrive que si la détection est active, une fois toutes les `work_method_every` sessions qu'elle a lancées (20), ou quand vous le demandez.

Une proposition est une question sur toutes les interfaces (une fenêtre : **Allow** / **Deny**, avec une notification). Seul Allow écrit le réglage ; Deny est définitif pour cette proposition et est dit à la session globale. Jamais appliquée en silence. État : `method.json` dans le dossier de données.
