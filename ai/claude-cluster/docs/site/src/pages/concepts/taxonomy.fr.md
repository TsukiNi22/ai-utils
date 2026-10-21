+++
title = "Catégories, tags et taxonomie"
summary = "Étiqueter les sessions, imbriquer les étiquettes dans un arbre, et le lire en liste, liste groupée et graphe hub."
section = "concepts"
order = 60
keywords = "catégorie tag taxonomie parent hub graphe arbre group_by_category étiquette session_describe taxonomy_edit descendants"
+++

## Catégories et tags {#labels}

Chaque session a une **catégorie** (un mot : `bugfix`, `docs`...) et un nombre quelconque de **tags**. Ils sont posés par la session globale (`session_describe` : `category`, `tags`, `add_tags`, `remove_tags`) ou par vous : `/category <mot>`, `/tags a b` (ou `+a -b`), la palette *Category* / *Tags*, `claude-cluster label <s> --category c --tag a,b`, un clic dessus dans la page web. Aucun modèle n'est appelé. Ils suivent la session (corbeille, restauration, `/clear`) et s'affichent en pastilles colorées sous la description.

`session_list` filtre par `category`, `tag` (plusieurs : tous requis), `state` (`open`, `archived` = la corbeille, `all`, ou un état) et `under` (un nœud de la taxonomie). `search` et `claude-cluster search` trouvent `#tag` (étiquettes seulement) ou un texte dans une étiquette ; la palette ouvre une session par ses étiquettes.

## La taxonomie {#taxonomy}

Catégories et tags forment une **forêt** : chaque nœud a un **parent** optionnel (une autre catégorie ou un tag) ; aucun signifie qu'il pend sous la racine `global`. Elle est stockée dans `taxonomy.json` du dossier de données et envoyée à toutes les interfaces. Une session pend sous son étiquette **principale** (sa catégorie, sinon son premier tag, sinon `global`).

- Les filtres `category` / `tag` de `session_list` trouvent aussi les sessions **sous** les étiquettes situées en dessous (`descendants: false` : l'étiquette seule) ; `under` prend n'importe quel nœud.
- Modifiez-la avec le graphe (*Set parent*, *Rename*, *Merge into*, *Delete*, *Add node*), la palette (*Taxonomy: ...*), `claude-cluster taxonomy [set_parent <nœud> [parent] | rename <nœud> <nouveau> | merge <nœud> <dans> | delete <nœud> [strip]]`, ou l'outil MCP `taxonomy` (`list`, `set_parent`, `rename`, `merge`, `delete`). Un cycle est refusé et supprimer un nœud déplace ses enfants vers son parent. L'outil est refusé tant que `[agent] taxonomy_edit` est désactivé.

## Liste groupée {#grouped}

`[ui] group_by_category` (réglages, la palette *Group by category*, `/group`, un bouton de la page web) groupe la liste des sessions par catégorie, **imbriquée par la taxonomie** : un en-tête par niveau du chemin du nœud, dans le terminal, la fenêtre, la page web et l'application.

## Vues graphe {#graph}

Le **Hub** (page web, fenêtre, application : clic droit ou la barre) et le graphe du terminal (`Alt+G`) montrent le cluster sous forme de graphe.

- **Hub en arbre** : `global` à la racine, nœuds de catégories et de tags colorés comme leurs pastilles avec le nombre de sessions en dessous, sessions en feuilles colorées selon l'état (vert au travail, orange en attente de permission, rouge en erreur, gris au repos). Les nœuds se replient. *Focus* ne montre que les descendants d'un nœud, *By every tag* met une session sous chacune de ses étiquettes, les sessions archivées sont grisées, un clic sur un nœud filtre la liste de sessions sur ce qui est en dessous. Les libellés ne se chevauchent jamais : placés autour de leur nœud, l'état supprimé d'abord, puis le titre raccourci, puis un point. La vue en anneau reste à un clic.
- **Graphe du terminal** (`Alt+G`, lecture seule) : la session globale au milieu, les sessions autour, un lien plein pour « au travail » et en pointillé pour « au repos ». Déplacement avec les flèches ou `Z Q S D`, zoom avec la molette ou `E` / `A`, `0` cadre tout, `F` centre la sélection, `Tab` sélectionne, `Entrée` ou un double clic ouvre la session, `L` la légende, `M` une mini-carte, `Échap` quitte. De loin (zoom sous 0,8) les noms sont raccourcis, sous 0,45 les nœuds sont des points. Les touches sont les `graph_*` de la [référence des touches](page:keys-reference).

Un graphe de votre **code** (fichiers, classes, appels) est autre chose : c'est le skill `html-doc` du dépôt, pas claude-cluster.
