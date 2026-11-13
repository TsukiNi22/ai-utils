+++
title = "Categories, tags and the taxonomy"
summary = "Label sessions, nest the labels in a tree, and read it as a list, a grouped list and the hub graph."
section = "concepts"
order = 60
keywords = "category tag taxonomy parent hub graph tree group_by_category label session_describe taxonomy_edit descendants"
+++

## Categories and tags {#labels}

Each session has one **category** (a word: `bugfix`, `docs`...) and any number of **tags**. They are set by the global session (`session_describe`: `category`, `tags`, `add_tags`, `remove_tags`) or by you: `/category <word>`, `/tags a b` (or `+a -b`), the palette *Category* / *Tags*, `claude-cluster label <s> --category c --tag a,b`, a click on them in the web page. No model is called. They are kept with the session (trash, restore, `/clear`) and shown as coloured chips under the description.

`session_list` filters by `category`, `tag` (several: all required), `state` (`open`, `archived` = the trash, `all`, or a state) and `under` (a node of the taxonomy). `search` and `claude-cluster search` find `#tag` (labels only) or text in a label; the palette opens a session by its labels.

## The taxonomy {#taxonomy}

Categories and tags form a **forest**: each node has an optional **parent** (another category or tag); none means it hangs under the root `global`. It is stored in `taxonomy.json` of the data folder and sent to every interface. A session hangs under its **primary** label (its category, else its first tag, else `global`).

- `session_list` filters `category` / `tag` also match the sessions **under** the labels below them (`descendants: false`: the label itself only); `under` takes any node.
- Edit it with the graph (*Set parent*, *Rename*, *Merge into*, *Delete*, *Add node*), the palette (*Taxonomy: ...*), `claude-cluster taxonomy [set_parent <node> [parent] | rename <node> <new> | merge <node> <into> | delete <node> [strip]]`, or the MCP tool `taxonomy` (`list`, `set_parent`, `rename`, `merge`, `delete`). A cycle is refused and deleting a node moves its children to its parent. The tool is refused while `[agent] taxonomy_edit` is off.

## Grouped list {#grouped}

`[ui] group_by_category` (settings, the palette *Group by category*, `/group`, a button in the web page) groups the session list by category, **nested by the taxonomy**: one head per level of the path of the node, in the terminal, the window, the web page and the app.

## Graph views {#graph}

The **Hub** (web page, window, app: right click or the bar) and the graph of the terminal (`Alt+G`) show the cluster as a graph.

- **Hub as a tree**: `global` at the root, category and tag nodes coloured like their chips with the number of sessions below, sessions as leaves coloured by state (green working, orange waiting for a permission, red error, grey idle). Nodes fold. *Focus* shows only a node's descendants, *By every tag* puts a session under each of its labels, archived sessions are greyed, a click on a node filters the session list to what is below it. Labels never overlap: they are placed around their node, the status dropped first, then the title shortened, then a dot. The ring view stays one click away.
- **Terminal graph** (`Alt+G`, read only): the global session in the middle, sessions around it, a solid link for working and dashed for idle. Pan with the arrows or `Z Q S D`, zoom with the wheel or `E` / `A`, `0` fits, `F` centres the selection, `Tab` selects, `Enter` or a double click opens the session, `L` the legend, `M` a minimap, `Esc` leaves. Far away (zoom under 0.8) names are shortened, under 0.45 nodes are dots. The keys are `graph_*` in [keys](page:keys-reference).

A graph of your **code** (files, classes, calls) is a different thing: that is the `html-doc` skill of the repository, not claude-cluster.
