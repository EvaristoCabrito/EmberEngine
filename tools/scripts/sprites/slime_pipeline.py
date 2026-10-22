"""The Corrosive Slime from its matted frames to the game files (the user, 2026-10-22): the full cut (cut.json), its world size set so
the Small Slime's idle body is as wide as a hex (`--width`, world units; the Large one is the roster's copy at x2), the medium copy
(cut-media.json: every clip at 0.555 of the video's size), the roster / manifest (apply_sprite_overrides.py) and the Map Editor's class
list (catalog_ember_classes.py). Then cook battle.pak.

    python -I tools/scripts/sprites/slime_pipeline.py [--width 1.75] [--dir corrosive-slime]

Any other video monster with the same files (cut.json + cut-media.json in content/sprites/<dir>) goes through it too: the Giant Spider
is `--dir giant-spider --width 3.5`.
"""
import argparse
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HERE = os.path.join(ROOT, "content", "sprites", "corrosive-slime")
PY = [sys.executable, "-I"]


def run(*args):
    print(">", " ".join(args[2:]) if len(args) > 2 else " ".join(args))
    subprocess.run(list(args), cwd=ROOT, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=float, default=1.75, help="the Small Slime's idle body width in world units (a hex is ~1.73)")
    ap.add_argument("--dir", default="corrosive-slime", help="the monster's folder in content/sprites")
    ap.add_argument("--cut-only", action="store_true",
                    help="only the two cuts: several new monsters are cut first, then apply_sprite_overrides.py + catalog_ember_classes.py "
                         "run once (the overrides list every set, so each one's cut must exist)")
    a = ap.parse_args()
    HERE = os.path.join(ROOT, "content", "sprites", a.dir)
    spec_path = os.path.join(HERE, "cut.json")
    spec = json.load(open(spec_path, encoding="utf-8"))
    # 1. the full cut at the current figureUnits, then the scale that makes the idle's body (its opaque box) `width` wide
    run(*PY, "tools/scripts/sprites/video_sprite.py", spec_path)
    result = json.load(open(os.path.join(HERE, "cut-result.json"), encoding="utf-8"))["clips"]
    base = result["base"]
    units_px = base["box"]["w"] / base["nat"][0]
    body = base["crop"][2] * units_px
    figure = spec["figureUnits"] * a.width / body
    print(f"idle body {body:.3f} units at figureUnits {spec['figureUnits']} -> figureUnits {figure:.4f} for {a.width}")
    if abs(figure - spec["figureUnits"]) > 1e-4:
        spec["figureUnits"] = round(figure, 4)
        spec["figureNote"] = f"set so the idle body (its opaque box) is {a.width} units wide"
        open(spec_path, "w", encoding="utf-8").write(json.dumps(spec, indent=1, ensure_ascii=False) + "\n")
        run(*PY, "tools/scripts/sprites/video_sprite.py", spec_path)
    # 2. the medium copy: every clip of the full cut at 0.555 of the video's size, same world size
    media_path = os.path.join(HERE, "cut-media.json")
    media = json.load(open(media_path, encoding="utf-8"))
    media["figureUnits"] = spec["figureUnits"]
    if "figurePx" in spec:
        media["figurePx"] = spec["figurePx"]
    media["clips"] = [dict(c, scaleBy=0.555) for c in spec["clips"]]
    for c in media["clips"]:
        c.pop("version", None)
    open(media_path, "w", encoding="utf-8").write(json.dumps(media, indent=1, ensure_ascii=False) + "\n")
    run(*PY, "tools/scripts/sprites/video_sprite.py", media_path)
    if a.cut_only:
        return
    # 3. roster + manifest, then the editor's class list
    run(*PY, "tools/scripts/sprites/apply_sprite_overrides.py")
    run(*PY, "tools/scripts/sprites/catalog_ember_classes.py")
    walk = json.load(open(os.path.join(HERE, "cut-result.json"), encoding="utf-8"))["clips"].get("move", {})
    print("walk body drift (px, + = the body moved right in the video):", walk.get("bodyDrift"))


if __name__ == "__main__":
    main()
