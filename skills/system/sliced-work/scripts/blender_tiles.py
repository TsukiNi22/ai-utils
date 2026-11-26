"""
One tile of a Blender frame, for slices.py --tiles (run inside Blender).

Usage:
    blender -b scene.blend -P blender_tiles.py -- --info
    blender -b scene.blend -P blender_tiles.py -- --col C --row R --cols N --rows M --out DIR [--frame F] [--keep-denoise]

- --info: the size of the frame in pixels (resolution x percentage), for slices.py init --image WxH.
- a tile: the border of the tile rendered and cropped (use_border + use_crop_to_border), row 0 at the top, saved as
  DIR/tile.<ext> in the format of the scene (PNG 16 bits RGBA when the scene saves a movie). The seed and the samples
  of the scene are kept, so each pixel is the one of a full render. The denoiser is turned off (it reads the
  neighbours: seams between tiles) unless --keep-denoise; denoise the merged image once instead.
"""

import argparse
import sys
from pathlib import Path

import bpy

def main() -> int:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(prog="blender_tiles.py")
    parser.add_argument("--info", action="store_true")
    parser.add_argument("--col", type=int)
    parser.add_argument("--row", type=int)
    parser.add_argument("--cols", type=int)
    parser.add_argument("--rows", type=int)
    parser.add_argument("--out")
    parser.add_argument("--frame", type=int)
    parser.add_argument("--keep-denoise", action="store_true")
    args = parser.parse_args(argv)
    scene = bpy.context.scene
    render = scene.render
    width = render.resolution_x * render.resolution_percentage // 100
    height = render.resolution_y * render.resolution_percentage // 100
    if args.info:
        print(f"IMAGE {width}x{height}")
        return 0
    x0, x1 = width * args.col // args.cols, width * (args.col + 1) // args.cols
    y0, y1 = height * args.row // args.rows, height * (args.row + 1) // args.rows
    render.use_border = True
    render.use_crop_to_border = True
    render.border_min_x, render.border_max_x = x0 / width, x1 / width
    render.border_min_y, render.border_max_y = 1 - y1 / height, 1 - y0 / height  # Blender counts y from the bottom
    if scene.render.engine == "CYCLES" and not args.keep_denoise:
        scene.cycles.use_denoising = False
    if scene.use_nodes and not args.keep_denoise:
        sys.stderr.write("blender_tiles: the compositor is on: blur / glare / denoise nodes make seams\n")
    if render.image_settings.file_format in ("FFMPEG", "AVI_JPEG", "AVI_RAW"):
        render.image_settings.file_format = "PNG"
        render.image_settings.color_depth = "16"
        render.image_settings.color_mode = "RGBA"
    if args.frame is not None:
        scene.frame_set(args.frame)
    render.filepath = str(Path(args.out) / "tile")
    render.use_file_extension = True
    bpy.ops.render.render(write_still=True)
    return 0

if __name__ == "__main__":
    code = main()
    if code:
        sys.exit(code)
