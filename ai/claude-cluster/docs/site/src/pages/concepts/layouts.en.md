+++
title = "Layouts"
summary = "List, grid, tabs and hub: how sessions are laid out in each interface and how to move between them."
section = "concepts"
order = 140
keywords = "layout list grid tabs hub board tree f2 layout panels grid_open navigation"
+++

`[ui] layout` (settings, `F2`, `--layout`, the palette) chooses how sessions are shown: `list`, `grid`, `tabs` or `hub`. The terminal shows `list` or `grid`; `tabs` is for the window and the web page.

## List {#list}

The session list on the left with the active conversation and its **panels** on the right: tokens (last prompt, current, total), context (used / window), cost (session, last turn, budget, 5 h / 7 d limits), usage (the vendor: today, 7 days, lifetime, the account windows), skills, git (branch, graph), diff, tools (timeline), files (modified). `[ui] panels` sets the default ones of a new session; each is toggled per session from the palette. The list scrolls (`↑ n more / ↓ n more`) and keeps the **Tasks** entry on top.

## Grid {#grid}

Every session is a cell with its state, model and folder; the cells keep a minimum size (38 x 8) and rows that do not fit scroll (`rows 1-3 of 5`). On an **empty prompt** the arrows move between cells (up / down keep the column, left / right turn inside the row); with a prompt being written they edit it and `Alt+arrows` always move. `Shift+Enter` or `Alt+O` (`grid_open`) opens the focused session in the list layout. `Ctrl+B` then an arrow, and `Alt+PageUp` / `PageDown`, `Alt+Home` / `End` also navigate.

## Tabs and hub {#tabs}

`tabs` (window, web page) puts one tab per session. In the web page the views are *Chat*, *Grid* (a card per session with a message field), *Board* (your dashboard of tiles moved by drag and drop, sized S / M / L, kept in the browser), *Hub* (a graph: see [the taxonomy](page:taxonomy#graph)) and *Tree* (sessions by project folder). On a phone the views are a tab bar.

## Navigation {#navigation}

`Ctrl+N` / `Alt+P` next / previous, `Alt+1`..`Alt+9` session N, `Alt+arrows` and `Ctrl+B`-arrows by side, `Alt+G` the graph. The list and the grid scroll to keep the session in view. See [Keys](page:keys-reference).
