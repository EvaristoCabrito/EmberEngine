"""The Underworld's hex board over the Atlas do Submundo (content/worldmap/underworld-atlas-v02.png).

python -I tools/scripts/maps/make_underworld_board.py [--overlay out.png]

Writes content/data/underworld.json: the cells (col, row, x, y in percent of the atlas), the start hex and the locations.
The hexes are the world map's own (pointy-top, odd-r, 5 % of the image's width a side, so they look the same size as the
Overworld's), laid so that Temple Descend's hex centre is the user's white mark (Attachments/UnderworldAMarked.jpg: 51.04 %,
9.89 %, the mouth of the shaft under the temple). A cell is kept when the painting under it is the underground (not the parchment
border, not the surface above the cave), and only if it is connected to Temple Descend.
"""
import json
import math
import sys
from collections import deque

from PIL import Image

ATLAS = "content/worldmap/underworld-atlas-v02.png"
OUT = "content/data/underworld.json"
MARK = (51.04265402843602, 9.89268947015426)  # the white mark, percent of the atlas
START_ROW = 1  # Temple Descend's row: row 0 lies on the surface above the cave


def main():
    overlay = sys.argv[sys.argv.index("--overlay") + 1] if "--overlay" in sys.argv else None
    im = Image.open(ATLAS).convert("RGB")
    w, h = im.size
    px = im.load()
    side_x = 5.0  # percent of the width
    step_x = side_x * math.sqrt(3.0)  # a hex's width, percent of the width
    step_y = 1.5 * side_x * w / h  # a row, percent of the height (the same size on screen as along x)
    oy = MARK[1] - START_ROW * step_y
    start_col = int(round((MARK[0] - (START_ROW & 1) * step_x / 2) / step_x))
    ox = MARK[0] - (start_col + 0.5 * (START_ROW & 1)) * step_x

    def centre(c, r):
        return ox + (c + 0.5 * (r & 1)) * step_x, oy + r * step_y

    # The parchment = the pale, colourless painting joined to the image's edge (flood fill from the edge over it, on a quarter-size
    # blurred copy): the pale ground inside the cave (the mushroom caps, the ruins' floor) is enclosed by the dark rim and stays in.
    from PIL import ImageFilter
    sw, sh = w // 4, h // 4
    small = im.resize((sw, sh), Image.BOX).filter(ImageFilter.GaussianBlur(1.5))
    sp = small.load()

    def pale(X, Y):
        r, g, b = sp[X, Y]
        return (r + g + b) / 3 > 140 and max(r, g, b) - min(r, g, b) < 70

    parchment = [[False] * sw for _ in range(sh)]
    todo = deque((X, Y) for X in range(sw) for Y in (0, sh - 1)) + deque((X, Y) for Y in range(sh) for X in (0, sw - 1))
    while todo:
        X, Y = todo.popleft()
        if not (0 <= X < sw and 0 <= Y < sh) or parchment[Y][X] or not pale(X, Y):
            continue
        parchment[Y][X] = True
        todo.extend(((X + 1, Y), (X - 1, Y), (X, Y + 1), (X, Y - 1)))

    def underground(x, y):
        # The painting around the centre (a disc of 40 % of the hex's half width): mostly not parchment.
        cx, cy = x / 100 * sw, y / 100 * sh
        rad = 0.4 * step_x / 2 / 100 * sw
        out = total = 0
        for dy in range(-int(rad), int(rad) + 1):
            for dx in range(-int(rad), int(rad) + 1):
                if dx * dx + dy * dy > rad * rad:
                    continue
                X, Y = int(cx + dx), int(cy + dy)
                total += 1
                if not (0 <= X < sw and 0 <= Y < sh) or parchment[Y][X]:
                    out += 1
        return total > 0 and out / total < 0.35

    rows = int((100 - oy) / step_y) + 1
    cols = int(100 / step_x) + 1
    cells = {}
    for r in range(START_ROW, rows):
        for c in range(-1, cols + 1):
            x, y = centre(c, r)
            if not (2 <= x <= 98 and 2 <= y <= 98):
                continue
            if (c, r) == (start_col, START_ROW) or (r > START_ROW and underground(x, y)):
                cells[(c, r)] = (x, y)

    def neighbours(c, r):  # odd-r, the world map's own order (Overworld::neighbors)
        d = [(1, 0), (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1)] if r % 2 == 0 else [(1, 0), (1, -1), (0, -1), (-1, 0), (0, 1), (1, 1)]
        return [(c + a, r + b) for a, b in d]

    seen = {(start_col, START_ROW)}
    todo = deque(seen)
    while todo:
        cur = todo.popleft()
        for n in neighbours(*cur):
            if n in cells and n not in seen:
                seen.add(n)
                todo.append(n)
    kept = sorted(seen, key=lambda k: (k[1], k[0]))
    data = {
        "atlas": ATLAS,
        "hexSide": side_x,
        "hexWidth": step_x,
        "rowStep": step_y,
        "origin": {"x": ox, "y": oy},  # hex (0, 0)'s centre
        "startHex": {"col": start_col, "row": START_ROW},
        "cells": [{"col": c, "row": r, "x": round(cells[(c, r)][0], 4), "y": round(cells[(c, r)][1], 4)} for c, r in kept],
        "locations": [{"id": "temple-descend", "name": "Temple Descend", "col": start_col, "row": START_ROW,
                       "x": round(MARK[0], 4), "y": round(MARK[1], 4)}],
    }
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print(f"{OUT}: {len(kept)} cells (of {len(cells)} under the cave), start ({start_col},{START_ROW})")
    for r in range(START_ROW, rows):
        line = "".join("#" if (c, r) in seen else "." for c in range(-1, cols + 1))
        print(f"{r:2d} {' ' if r % 2 else ''}{' '.join(line)}")
    if overlay:
        from PIL import ImageDraw
        ov = im.copy()
        d = ImageDraw.Draw(ov)
        for c, r in kept:
            x, y = cells[(c, r)]
            X, Y = x / 100 * w, y / 100 * h
            hw, hh = step_x / 2 / 100 * w, side_x / 100 * w
            pts = [(X, Y - hh), (X + hw, Y - hh / 2), (X + hw, Y + hh / 2), (X, Y + hh), (X - hw, Y + hh / 2), (X - hw, Y - hh / 2)]
            d.polygon(pts, outline=(255, 60, 60) if (c, r) != (start_col, START_ROW) else (255, 255, 0))
        ov.save(overlay)


if __name__ == "__main__":
    main()
