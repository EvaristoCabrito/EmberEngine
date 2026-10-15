"""Draws where enclosed never-seen holes appear (Sandbox writes captures/fog/holes.txt during --scout runs).

    python -I tools/scripts/draw_fog_holes.py captures/fog/holes.txt <scene.json> <out.png>

One panel per moment logged: hexes coloured by fog-of-war state (dark = never seen, grey = remembered,
pale = in sight), holes in red, the scout as a yellow dot, and what stands on the board from the imported
scene (3D models as brown boxes, standing cards at least 1.2 tall and 1.2 wide as dark green dots: the things
that block sight).
"""
import json
import math
import sys

from PIL import Image, ImageDraw

SQ3 = math.sqrt(3.0)
COLORS = {".": (25, 25, 30), "r": (95, 95, 105), "v": (205, 215, 200), "H": (230, 40, 40), "S": (205, 215, 200)}


def read_moments(path):
    moments, cur = [], None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("time"):
            parts = line.split()
            cur = {"time": float(parts[1]), "scout": (int(parts[3]), int(parts[4])), "holes": int(parts[6]), "rows": []}
            moments.append(cur)
        elif cur is not None and line:
            cur["rows"].append(line)
    return moments


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    moments = read_moments(sys.argv[1])
    scene = json.load(open(sys.argv[2], encoding="utf-8"))
    rows = len(moments[0]["rows"])
    cols = len(moments[0]["rows"][0])
    ox, oz = SQ3 * (cols - 0.5) / 2, 0.75 * (rows - 1)
    k = 22  # pixels per world unit
    pw, ph = int((SQ3 * (cols + 1)) * k), int((1.5 * rows + 1) * k) + 30
    sheet = Image.new("RGB", (pw * len(moments), ph), (0, 0, 0))
    for m_i, m in enumerate(moments):
        img = Image.new("RGB", (pw, ph), (0, 0, 0))
        d = ImageDraw.Draw(img)

        def to_px(x, z):
            return ((x + ox + SQ3 * 0.5) * k + 4, (z + oz + 1.0) * k + 30)

        for r, row in enumerate(m["rows"]):
            for c, ch in enumerate(row):
                cx, cz = SQ3 * (c + 0.5 * (r & 1)) - ox, 1.5 * r - oz
                pts = [to_px(cx + math.sin(a * math.pi / 3), cz + math.cos(a * math.pi / 3)) for a in range(6)]
                d.polygon(pts, fill=COLORS.get(ch, (255, 0, 255)), outline=(0, 0, 0))
                if ch == "S":
                    x, y = to_px(cx, cz)
                    d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(255, 220, 0))
        for item in scene.get("decor", []):
            x, y = to_px(item["x"], item["z"])
            if item.get("kind") == "model":
                d.rectangle([x - 7, y - 7, x + 7, y + 7], outline=(170, 110, 50), width=3)
            elif not item.get("onFloor") and item.get("h", 0) >= 1.2 and item.get("w", 0) >= 1.2:
                d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(40, 120, 50))
        d.text((8, 8), f"t = {m['time']:.1f} s, scout at {m['scout']}, {m['holes']} hole(s) in red", fill=(255, 255, 255))
        sheet.paste(img, (m_i * pw, 0))
    sheet.save(sys.argv[3])
    print(f"{len(moments)} moments -> {sys.argv[3]}")


if __name__ == "__main__":
    main()
