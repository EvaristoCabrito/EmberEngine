"""How wide each decoration card's art really is, for the fog of war's line of sight (MapStage::addOccluders).

A card's `w` is its whole image box; a tree's trunk fills a third of it. For every PNG in Engine2's
public/game/decorations this writes the median share of the image's width that is opaque (alpha > 128), over the
rows between 20% and 80% of the art's own height, to content/engine2/card-sight.json:
  {"/game/decorations/<name>.png": share, ...}
A card blocks sight only where its drawn width (w x share) reaches.

  python -I tools/scripts/engine2/card_sight.py content/game/public content/engine2/card-sight.json
"""
import json
import sys
from pathlib import Path

from PIL import Image


def opaque_share(path):
    alpha = Image.open(path).convert("RGBA").getchannel("A")
    width, _ = alpha.size
    box = alpha.point(lambda v: 255 if v > 128 else 0).getbbox()
    if not box:
        return 0.0
    x0, y0, x1, y1 = box
    px = alpha.load()
    rows = range(y0 + (y1 - y0) // 5, max(y0 + (y1 - y0) // 5 + 1, y0 + 4 * (y1 - y0) // 5), 2)
    fills = sorted(sum(1 for x in range(x0, x1) if px[x, y] > 128) for y in rows)
    return fills[len(fills) // 2] / width


def main():
    public, out = Path(sys.argv[1]), Path(sys.argv[2])
    table = {}
    for png in sorted((public / "game" / "decorations").glob("*.png")):
        table["/game/decorations/" + png.name] = round(opaque_share(png), 4)
    out.write_text(json.dumps(table, indent=0, sort_keys=True) + "\n", encoding="utf-8")
    print(f"{len(table)} decoration images -> {out}")


if __name__ == "__main__":
    main()
