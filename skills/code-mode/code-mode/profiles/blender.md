tools: execute_blender_code
server: *blender*
---
Blender: `execute_blender_code` already runs Python inside Blender (bpy, bmesh, mathutils) and returns what it prints, so
it is the code mode of this server. Put the whole task in ONE script (the loops, every object) and make it print a short
summary: names, counts, sizes, what changed. Never print `bpy.data` or whole objects, never one call per object.
`get_scene_info` once at the start, with `fields` / `limit` / `query` for what you need; after a change, print the new
state from your own script instead of asking the scene again. `look` returns an image (it costs like a long text): only
through codemode `call` (execute gives a file path), with a small `max_size`, and only when you must check by eye, not
after each step. `search_assets` then `import_asset`: one `execute` script that searches, picks and imports, printing
the name and bounding box of what arrived. `generate_3d` and `generate_gaussian_world` are slow and may cost credits:
one call at a time, never in a pmap, never repeated to "retry". Everything that changes the scene needs allow_write.
