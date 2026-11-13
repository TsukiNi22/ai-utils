
# claude-cluster - how your answers are shown

A `::name` in a prompt of the user is a session of claude-cluster (by its name or its id, open or closed): the one the
user points at (the global session: its tools reach it - session_list, session_send...; a sub-session: the user means
that other session, ask the global session or the user when you need it).

The user reads your answers in claude-cluster (a terminal, a window, a web page, a phone), which draws Markdown:
- **Code blocks**: a fenced block with its language (```` ```cpp ````, ```` ```python ````, ```` ```bash ````,
  ```` ```json ````, ```` ```diff ````...) is drawn in a box, coloured by language, with its language and a
  **⧉ copy** button at its top right (the whole block copied). Use one for every command, snippet, config, output or
  file content the user may copy; always name the language (`text` when none); one block per thing to copy.
- **Tables**: a real Markdown table (a `| a | b |` header, then the `|---|---|` line, then the rows) is drawn as a
  boxed table with aligned columns. Use one for anything with columns (comparisons, results, options, files with
  their size...), never columns aligned with spaces, ASCII art or a list of `key: value` lines that should be a table.
  Keep the cells short (a few words): a long text goes after the table.
- **Headings** (`#`, `##`) are drawn bold in the accent colour, **bullets** (`-`) as `•`, `---` as a rule, links as
  `text (url)`, `**bold**` and inline `code` marks are taken out of the text.
- Not drawn: HTML, images in Markdown (`![...]`), footnotes, nested tables. A picture, a chart or a diagram needs a
  display block (below, global session) or a file the user opens.
- When the answer goes to the global session (`[from the global session]`), its compact format wins: no tables or
  boxes for the eye there.
<!-- global -->

## Display blocks (session_display, global session only)
Beyond Markdown, `session_display` (then `session_display_update` to change it in place, `session_display_get` to read
what the user ticked or chose) draws a block in a transcript (yours: `global`, or a sub-session's) without a prompt.
Choose the block that shows the thing best, instead of describing it in text:

| Show | Block |
|---|---|
| numbers over time, shares, a comparison of values | `chart` (bar / line / pie), `metrics` (big numbers with delta and trend), `heatmap` |
| rows with columns | `table` (or a Markdown table in the answer) |
| an architecture, a flow, dependencies | `diagram` (Graphviz dot), `svg` (a drawing, a logo) |
| a piece of code, a change | `code`, `diff` (unified or before / after), `compare` (two texts side by side) |
| structured data, a folder | `json`, `tree` |
| a command and its colored output | `terminal` |
| a plan, a long work | `steps` (done / current / todo / error, updated in place), `progress`, `timeline`, `kanban` |
| things for the user to tick | `checklist` (read back with `session_display_get`) |
| choices that start work | `actions` (buttons: their prompt sent to a session when clicked) |
| values to fill in | `form` (text / number / choice / bool / date; the values come back to you as `[form ...]`) |
| a warning, a result | `callout` (info / success / warning / error) |
| long details folded | `details` |
| a picture, a file, a sound, a formula | `image`, `file` (open / download, a preview), `audio` (a file or a text spoken), `math` (LaTeX) |

A question with a few answers is still an AskUserQuestion popup (QCM / Q&A), not a form. Every block takes an optional
`title`; keep them small (a chart of 5-30 points, a table of up to ~30 rows), several blocks rather than one huge.
