+++
title = "Édition du prompt"
summary = "Échap pour reprendre ou revenir en arrière, la réserve, l'historique, les touches de l'éditeur de ligne, le mode vim, le texte collé et les modes du prompt."
section = "concepts"
order = 130
keywords = "prompt édition échap rewind reprendre réserve stash historique vim éditeur de ligne touches coller pastilles commande shell commandes vi file"
+++

## Échap : reprendre ou revenir en arrière {#esc}

Une seule règle, choisie selon ce qui se passe (`[keys] take_back`) :

- Un prompt **en file** revient dans l'éditeur, retiré de la file.
- Un tour **en cours** (ou en attente d'une permission) : `Échap` `Échap` (le premier `Échap` le signale) l'interrompt et seul ce prompt est repris - jamais `↑`, qui ne reprend qu'un prompt en file : il quitte la conversation (claude repart du message précédent) et son texte revient dans l'éditeur pour être modifié et renvoyé (`prompt taken back`).
- **Rien en cours, prompt vide** : le **choix de retour en arrière** liste les prompts de la conversation, du plus récent, chacun avec son heure, sa première ligne et la taille de sa réponse ; `↑` / `↓` choisissent, Entrée revient juste avant (le prompt revient dans l'éditeur, les messages suivants quittent la conversation d'où l'agent reprend), la ligne `Cancel` ou Échap ne change rien. Une ligne montre ce qui sera abandonné. `/rewind` l'ouvre aussi. Jamais pendant qu'un tour tourne.
- **Rien en cours, un prompt en cours d'écriture** : `Échap` `Échap` le vide.

Ce qu'un retour a retiré est gardé dans `rewind/<session>-<heure>.json` du dossier de données (la transcription propre de claude n'est jamais touchée, donc l'ancienne conversation peut être reprise à son ancienne feuille) ; avec `[ui] stash_rewound = true` les prompts abandonnés vont aussi dans la réserve.

## Commandes et skills du même nom {#same-name}

La complétion de `/` liste chaque commande et skill avec sa source (`local`, `built-in`, un plugin, un lien importé). Quand deux portent le même nom - le `/context` de claude-cluster et celui de l'agent, votre skill `export` et le `/export` de claude-cluster -, les deux sont listés, et celui choisi garde un identifiant caché après son nom : des caractères « tag » invisibles que le curseur et Retour arrière traitent comme un seul bloc avec la dernière lettre. Le cœur lance alors la commande de claude-cluster ou envoie la commande de l'agent / le skill à l'agent ; l'identifiant est retiré avant que quoi que ce soit n'atteigne l'agent ou le transcript. Un `/nom` tapé à la main reste la commande de claude-cluster.

## La réserve {#stash}

`Ctrl+S` met le prompt de côté et vide l'éditeur, `Ctrl+P` reprend le dernier, `Ctrl+L` ouvre la **liste** : du plus récent, avec l'heure, la première ligne et la session où il a été écrit. Entrée en charge un dans la session au premier plan et le retire de la réserve (avec un prompt déjà écrit : *Replace* - celui en cours d'écriture part dans la réserve -, *Append* ou *Cancel*) ; `Suppr` / `x` en retire un (`u` annule) ; `X` vide tout en deux étapes. La réserve est globale (`stash.json` du dossier de données, conservée entre les redémarrages) : au plus `[ui] stash_max` (10) ; pleine, `Ctrl+S` refuse et rien n'est perdu (`stash_full = drop_oldest` fait de la place à la place). Limites : 256 Ko par prompt, 4 Mo au total.

## Historique {#history}

`↑` / `↓` se déplacent par **ligne visuelle** dans un prompt replié, en gardant la colonne ; sur la première ou dernière ligne l'**historique** de la session s'affiche, jamais en boucle ; ce que vous écriviez est gardé pendant la navigation. `Ctrl+R` ouvre la liste de l'historique (tapez pour filtrer, `Tab` : toutes les sessions). Il garde les 1000 derniers prompts (`history_limit`) avec leurs images et textes collés.

## Touches de l'éditeur de ligne {#keys}

Le prompt et chaque fenêtre qui prend du texte (la réponse *Other*, le chat d'une question, les formulaires, les valeurs de réglages, les filtres, renommer, décrire, cd...) utilisent le même éditeur de ligne :

| Touches | Action |
|---|---|
| `←` `→`, `Ctrl+←` `Ctrl+→` / `Alt+b` `Alt+f` | un caractère (un emoji, un accent : une cellule), un mot |
| `Home` `End` / `Ctrl+A` `Ctrl+E`, `Ctrl+Home` `Ctrl+End` | la ligne, le texte |
| `Retour arrière` `Suppr`, `Ctrl+Retour arrière` / `Alt+Retour arrière` / `Ctrl+W`, `Ctrl+Suppr` / `Alt+d` | un caractère, le mot avant, le mot suivant |
| `Ctrl+U` / `Ctrl+K`, `Ctrl+Y` | jusqu'au début / la fin de la ligne (gardé), le remettre |
| `Ctrl+Z` / `Ctrl+Shift+Z` | annuler / rétablir |
| `Shift` (+ `Ctrl`) + flèches / `Home` / `End` | sélectionner ; taper remplace la sélection |
| `Ctrl+Shift+C` / `Ctrl+X` / `Ctrl+Shift+V` (`Ctrl+V`) | copier / couper / coller (collage bracketed aussi) |
| souris | un clic place le curseur, glisser sélectionne, double clic prend un mot |

Les mots et le kill ring fonctionnent dans le prompt là où aucune action de `[keys]` n'utilise la touche ; un mot de passe se tape à l'aveugle. `Ctrl+E` édite tout le prompt dans votre éditeur (`[ui] editor`, sinon nvim, vim, vi, `$VISUAL`, `$EDITOR`, emacs -nw, nano).

## Mode vim {#vim}

`/vim`, la palette ou `[ui] vim = true` : le prompt démarre en Insert, `Échap` passe en Normal (`h j k l`, `w b e`, `0 ^ $`, `gg G`, `f t F T`, `x X s S D C`, `dd cc yy`, opérateurs avec mouvements, `p P`, `u` / `Ctrl+R`, compteurs, `v` / `V` visuel). En Normal, Entrée envoie.

## Texte collé et pastilles {#paste}

Un collage de plus de `[ui] paste_lines` (3) lignes ou `paste_chars` (800) caractères est un bloc `[Pasted text #N +L lines]`, envoyé, mis en réserve et édité (`Ctrl+E`) comme son texte complet. Les images collées du presse-papiers sont `[Image #N]`, enregistrées par contenu donc la même image est une seule référence. Une pastille par image ou texte collé se tient au-dessus du prompt : le survol le montre, un clic le remet, un clic droit ouvre une image dans la visionneuse du bureau. `Retour arrière` sur un bloc le retire en entier. Chaque collage (un bloc ou un texte court) est **signalé à l'agent** : il le reçoit entre `<pasted>` et `</pasted>`, tandis que vous voyez le texte simple dans le prompt, le transcript et l'historique (deux marques invisibles). Une commande collée au début d'un prompt vide (`!`, `/`, `:`) garde son mode.

## Modes du prompt {#modes}

Un premier caractère change ce que fait le prompt : `!<commande>` lance votre shell dans le dossier de la session (la sortie va dans la conversation et est donnée à l'agent avec votre prompt suivant si vous êtes d'accord : *this time / always / not this time / never* ; une commande de plus de 8 s passe en arrière-plan comme un shell `b1`, listé par `/shells`) ; `:s/old/new/` réécrit votre dernier prompt ; `:r <fichier>` / `:r !<commande>` lit un fichier ou une sortie dans le prompt ; `:w`, `:e`, `:q`, `:qa` ; `:set clé=valeur` ; `:g/texte/` cherche. `@` complète les fichiers, `/` commandes et skills, `::` les sessions. Depuis la page web et l'application `!` est refusé sauf `[web] shell = true`.
