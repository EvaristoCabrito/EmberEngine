"""Engine2's effect art (public/game/fx, public/game/effects) as sprite clips, and the battle pack's manifest.

Writes content/fx.sprites.json: one set "fx", one clip per image. A "...-4x4" sheet becomes 16 frames (its cells,
row by row); any other image one frame. Art painted on black (no alpha channel) is cooked with "!black" (alpha =
its brightest channel, as Engine2's deriveAlphaFromBlack). Clip name = file name without extension.

Then merges content/engine2.sprites.json + content/decor.sprites.json + content/fx.sprites.json into
build/sprites/battle.sprites.json (the one pack the game mounts: two packs would both define sprites/roster).

  python -I tools/scripts/fx_sprites.py [C:/Engine2/public/game]
Engine2 is only read.
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = sys.argv[1] if len(sys.argv) > 1 else "C:/Engine2/public/game"


def clips():
    out = []
    for folder in ("fx", "effects"):
        for name in sorted(os.listdir(os.path.join(GAME, folder))):
            stem, ext = os.path.splitext(name)
            if ext.lower() not in (".png", ".jpg", ".jpeg", ".webp"):
                continue
            rel = f"{folder}/{name}"
            with Image.open(os.path.join(GAME, folder, name)) as im:
                black = "A" not in im.getbands()
            suffix = "!black" if black else ""
            if "4x4" in stem:
                frames = {"list": [f"{rel}#4x4:{i}{suffix}" for i in range(16)]}
            else:
                frames = {"file": rel + suffix}
            out.append({"name": stem, "pack": "fx", "mode": "once", "fps": 24.0, "frames": frames})
    return out


def main():
    fx = {"version": 1, "sets": [{"name": "fx", "clips": clips()}]}
    with open(os.path.join(ROOT, "content", "fx.sprites.json"), "w", encoding="utf-8") as f:
        json.dump(fx, f, indent=1, ensure_ascii=False)
        f.write("\n")
    sets = []
    for part in ("engine2", "decor", "fx"):
        with open(os.path.join(ROOT, "content", f"{part}.sprites.json"), encoding="utf-8") as f:
            sets += json.load(f)["sets"]
    os.makedirs(os.path.join(ROOT, "build", "sprites"), exist_ok=True)
    with open(os.path.join(ROOT, "build", "sprites", "battle.sprites.json"), "w", encoding="utf-8") as f:
        json.dump({"version": 1, "sets": sets}, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"fx: {len(fx['sets'][0]['clips'])} clips; battle manifest: {len(sets)} sets")


if __name__ == "__main__":
    main()
