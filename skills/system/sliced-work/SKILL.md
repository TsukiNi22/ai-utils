---
name: sliced-work
description: Run any work that takes several minutes (render, generation, conversion, encoding, simulation bake, batch of AI calls or files) in slices - piece by piece (frame ranges, image tiles, lines of a list, chunks of data), each slice written atomically, so that after a crash, a kill, a reboot or a stop on purpose the work resumes where it stopped instead of from the start, then the slices merged into the final result exactly as one pass would give it (same seed and settings per slice, neighbourhood effects like denoise, glare or the video encode done once after the merge, a check against a full render of a small sample). slices.py plans, runs, resumes, reports and merges (frames, cat, ffmpeg concat, image tiles, or a command); blender_tiles.py renders one tile of a Blender frame. Use whenever a job will take more than a few minutes or could be cut by a crash, the user says "en slicing", "par morceaux", "reprenable", "qu'on puisse reprendre", or a long job was lost and must not be lost again.
metadata:
  summary: 'Long jobs in resumable slices (frames, tiles, items, chunks), written atomically, resumed after a crash, merged into the same result as one pass.'
---

# Sliced work

A job of several minutes is cut in **slices**: each one is done or not (never half done), a crash costs one slice, and
the merge gives the result of a single pass. `SKILL_DIR` = the directory of this file, `slices.py` =
`python3 SKILL_DIR/scripts/slices.py`. Heavy slices run limited (`memory-guard`).

## 1. Cut
| Work | Slice | Merge |
|---|---|---|
| animation, frames | `--range 1-240 --size 10` (Blender `-s {start} -e {end} -a`) | `frames` |
| one big image | `--tiles 4x4 --image WxH` (`blender_tiles.py`) | `tiles` |
| a list (prompts, files, URLs, AI calls) | `--items list.txt` (`{item}`) | `cat` or `frames` |
| video encode, audio | the frames lossless first, the encode once at the end; or segments with closed GOP | `concat` (`-c copy`) |
| data, logs, datasets | chunks by line / size / key, one output each | `cat` or a command |

A slice: 1 to 5 minutes. Outputs on disk (`~/.cache/<task>/`), not in tmpfs `/tmp`.

## 2. Same result as one pass
- **Per-item work is identical**: same file, settings, seed and version for every slice (Cycles: the scene seed,
  no animated seed when the frames must match a full run).
- **Neighbourhood effects once, after the merge**: denoise, glare / bloom, blur, sharpen, the video encode (rate
  control, GOP). `blender_tiles.py` turns the Cycles denoiser off; denoise the merged image.
- **Prove it on a sample**: a small frame or a few items both ways, then
  `magick compare -metric AE full.png merged.png null:` (0 = identical). Measured: a 203x117 Cycles frame, 3x2 uneven
  tiles, killed after 2 tiles and resumed -> 0 pixels differ.

## 3. Run, resume, merge
```bash
slices.py init ~/.cache/logo-anim --range 1-240 --size 10 --merge frames --final frames --max 4G \
    --cmd 'blender -b /abs/scene.blend -o {out}/frame_#### -s {start} -e {end} -a'
slices.py run ~/.cache/logo-anim       # background; again after a crash: the done slices are skipped
slices.py status ~/.cache/logo-anim    # done / pending / failed, time left
slices.py merge ~/.cache/logo-anim     # every slice done -> the final result
```
One tile of a Blender frame (`--info` gives the `WxH` for `init`):
```bash
slices.py init ~/.cache/poster --tiles 3x3 --image 3840x2160 --merge tiles --final poster.png --max 4G \
    --cmd 'blender -b /abs/poster.blend -P SKILL_DIR/scripts/blender_tiles.py -- --col {col} --row {row} --cols {cols} --rows {rows} --out {out}'
```
- Each slice writes into `{out}` (`parts/<id>.partial/`), renamed `parts/<id>/` on success; a `.partial` left by a
  crash is deleted and the slice runs again. Absolute paths in `--cmd`.
- A failed slice stops the run: fix it, `slices.py run` again; `slices.py reset DIR <id>` redoes one.
- Keep `DIR` until the user has the final result; say its path and size, then propose to delete the parts.
