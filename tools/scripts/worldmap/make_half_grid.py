"""The Overworld's board at half the hex size (a test the user asked for, 2026-10-29: "scale down ... the movement grid by half").

Reads content/data/overworld.json (left untouched) and writes content/data/overworld-half.json: the same painting, locations,
missions and tables, the cells on a grid of hexes half as wide (hexSize 2.5, about four cells for every old one), every step 3
hours (`stepHours`; the old step was 6). An old hex (col, row) has its centre on the new hex (2 col + (row & 1), 2 row), so the
start, the Inn, the Wisp Forest, every location and the early road keep their places; a new hex exists when its centre lies in
an old cell and takes that cell's region and encounters. Overworld.h `kOverworldData` picks which file the game loads.

Usage: python -I tools/scripts/worldmap/make_half_grid.py
"""
import json
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "content" / "data" / "overworld.json"
OUT = ROOT / "content" / "data" / "overworld-half.json"
OLD_SIZE, NEW_SIZE = 5.0, 2.5
SQ3 = math.sqrt(3.0)


def to_world(col, row, size):  # Overworld::hexToWorld (odd-r, pointy-top)
    q = col - (row - (row & 1)) // 2
    return size * SQ3 * (q + row / 2), size * 1.5 * row


def from_world(x, y, size):  # the hex whose centre is nearest (cube rounding), as odd-r col / row
    r = y / (1.5 * size)
    q = x / (SQ3 * size) - r / 2
    s = -q - r
    rq, rr, rs = round(q), round(r), round(s)
    dq, dr, ds = abs(rq - q), abs(rr - r), abs(rs - s)
    if dq > dr and dq > ds:
        rq = -rr - rs
    elif dr > ds:
        rr = -rq - rs
    return rq + (rr - (rr & 1)) // 2, rr


def half(h):
    return {"col": 2 * h["col"] + (h["row"] & 1), "row": 2 * h["row"]}


def main():
    data = json.loads(SRC.read_text(encoding="utf-8"))
    board = data["board"]
    assert board.get("hexSize", 5) == OLD_SIZE
    old = {(c["col"], c["row"]): c for c in board["cells"]}
    rows = [r for _, r in old]
    cols = [c for c, _ in old]
    cells = []
    for row in range(2 * min(rows) - 2, 2 * max(rows) + 3):
        for col in range(2 * min(cols) - 3, 2 * max(cols) + 4):
            x, y = to_world(col, row, NEW_SIZE)
            src = old.get(from_world(x, y, OLD_SIZE))
            if src is None:
                continue
            cell = {k: v for k, v in src.items() if k not in ("col", "row", "x", "y", "hours")}
            cells.append({"col": col, "row": row, "hours": 3, **cell, "x": x, "y": y})
    for key in ("startHex", "ashenBlocked", "innHex", "wispHex"):
        if isinstance(board.get(key), dict):
            board[key] = half(board[key])
    board["westEdgeCol"] = 2 * board["westEdgeCol"]
    board["hexSize"] = NEW_SIZE
    board["stepHours"] = 3
    board["cells"] = cells
    keys = {(c["col"], c["row"]) for c in cells}
    for loc in data["locations"]:
        # The hex under the pin (an old hex's centre can be a whole small hex away from its pin): clicking the pin is clicking the
        # location's hex. The early road (Overworld::canStep) reaches the Stone Bridge's hex wherever it lies.
        near = min(cells, key=lambda c: (c["x"] - loc["x"]) ** 2 + (c["y"] - loc["y"]) ** 2)
        loc["col"], loc["row"] = near["col"], near["row"]
        assert (loc["col"], loc["row"]) in keys, loc["id"]
    for key in ("startHex", "innHex"):
        assert (board[key]["col"], board[key]["row"]) in keys, key
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{len(old)} cells -> {len(cells)} cells; {OUT}")


if __name__ == "__main__":
    main()
