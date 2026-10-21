+++
title = "Premier démarrage et première session"
summary = "Lancer claude-cluster, répondre aux premières questions, parler à la session globale, ouvrir une session sur un projet."
section = "guide"
order = 20
keywords = "premier démarrage démarrage rapide spawn nouvelle session session globale carte ordinateur restaurer corbeille permissions"
+++

## Premier démarrage {#start}

```bash
claude-cluster
```

Dans un terminal cela ouvre l'interface terminal, sinon la fenêtre (`--tty` / `--gui` en imposent une). Au premier démarrage deux questions sont posées :

- **Restaurer l'exécution précédente ?** (`[ui] restore` : `ask`, `always`, `never`). Échap restaure.
- **Cartographier cet ordinateur ?** La carte (`machine.md`) est ce que la session globale lit pour connaître vos projets, outils et shell : votre système et matériel, les outils installés, les alias de shell dangereux, vos dépôts git. Elle est locale, montrée avant d'être lue, et reproposée quand elle a plus de `[ui] machine_days` (30) jours. Répondre non est mémorisé.

L'interface montre la liste des sessions à gauche, la conversation au milieu et des panneaux optionnels (tokens, contexte, coût, git...) à droite. `F1` liste toutes les touches, `Ctrl+K` ouvre la palette de commandes, `F9` les réglages.

## Parler à la session globale {#global}

La première session est la **session globale** (`Ctrl+G`). C'est une session Claude Code qui pilote toutes les autres avec ses propres outils : demandez-lui en langage courant, ou à la [voix](page:voice).

> Ouvre une session sur le projet libutils et demande-lui de lancer ses tests.

Elle retrouve le projet dans la carte de votre ordinateur, crée une sous-session dans ce dossier, envoie le prompt, et vous donne la réponse quand la sous-session a fini (sans interrogation). Voir [La session globale](page:global-session).

## Ouvrir une session soi-même {#session}

| Façon | Effet |
|---|---|
| `Ctrl+T`, `/new <dossier>`, palette *New session on a project* | une nouvelle session dans un dossier ou un projet de la carte (le nom le plus proche gagne, fautes de frappe tolérées) |
| `claude-cluster spawn <dossier> --name x --prompt "..."` | pareil depuis un shell, sur l'instance en cours |
| `/cd <dossier>` | déplacer la session courante vers un autre projet |

Tapez ensuite dans le prompt et appuyez sur `Entrée`. Pendant qu'un tour tourne, vos prompts suivants sont mis en file sous la transcription ; `Échap` reprend le dernier.

## Permissions {#permissions}

Une nouvelle session démarre en mode `auto` : le classificateur de Claude Code autorise les actions sûres. Quand quelque chose attend votre réponse, une fenêtre apparaît sur la session, une notification le dit, et vous répondez par `Alt+Y` (autoriser), `Alt+A` (toujours autoriser) ou `Alt+D` (refuser). `Shift+Tab` change de mode. Voir [Permissions et modèles](page:permissions-models).

## Fermer, restaurer, quitter {#close}

- `Ctrl+W` ou `/close` ferme une session vers la **corbeille** (restaurable `[sessions] trash_days` jours, 14 par défaut) : `Ctrl+Shift+T` / `Alt+T` en rouvre une ; `/delete` la supprime pour de bon après une question.
- `Ctrl+Q` quitte. Une session encore au travail demande s'il faut la laisser tourner en arrière-plan ou tout arrêter.
- `claude-cluster serve` lance le cœur sans interface ; les commandes ci-dessous et le [serveur MCP](page:callbacks-mcp) le pilotent.

```bash
claude-cluster list                      # les sessions et leur état
claude-cluster send s2 "lance les tests" # un prompt à une session
claude-cluster status s2                 # détails et derniers messages
```

Ensuite : [les interfaces](page:interfaces) et [les touches](page:keys).
