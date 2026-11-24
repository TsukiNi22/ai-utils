tools: execute_luau, run_code
server: *roblox*
---
Roblox Studio: `execute_luau` (built-in server; legacy server: `run_code`) runs Luau inside Studio, so it is the code
mode of this server. Put the whole task in ONE script (the loops over instances, the edits) and return / print a short
summary: counts by ClassName, a few paths, what changed. Never a `GetDescendants()` dump, never one call per instance.
Built-in server: every tool takes `studio_id` (get it once with `list_roblox_studios`, keep it in a variable: variables
persist between executes) and `execute_luau` takes `datamodel_type` (Edit, Client or Server). Read with the narrow
tools: `script_grep` (50 matches at most) or `script_search` (10) before `script_read`, and `script_read` with a line
range rather than a whole script; `search_game_tree` with its filters and a depth limit, `inspect_instance` for one
instance. Several edits of scripts: one `multi_edit`. `get_console_output` returns the whole output log: get it inside
`execute` and print only the errors and warnings since your action. A play test: `start_stop_play`, then poll
`get_studio_state` / `get_console_output` in one `execute` loop with a sleep, not call after call. `screen_capture`
returns an image: only through codemode `call`, when you must check by eye. The `generate_*` tools and `upload_image`
are slow jobs (`wait_job_finished`): one at a time, never in a pmap. Check the exact arguments with search detail=true.
Everything that changes the place needs allow_write; `save_place` only when the user asks.
