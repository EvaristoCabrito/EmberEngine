"""A Estalagem's furniture in 3D (the user, 2026-10-25, with the Inn's word: "remover as mesas e cadeiras e outros objetos 2d e colocar as
novas mesas e cadeiras e outros objetos de bar 3D", "o mesmo no segundo andar temos camas 3D", "os 2D que permanecerem no INN continuem
la", "a não ser que tenha mais mesas e cadeiras", "uma lareira 3D que fiz no meshy para experimentar no lugar da atual").

Only the 2D tables, chairs and stools become the game's own cooked 3D pieces (every other 2D piece stays where it is); the salão's
fireplace becomes the Meshy hearth; the bar's two workbenches beside Brue become the tavern counter; Andar II's bedrooms get the
Torre's photographed beds. New versions only: estalagem007 (from 006 as committed) and estalagem-andar-2006 (from 004, the version
content/maps/active.json plays); every older file untouched.

The script refuses (and writes nothing) if a piece lands on a wall, a door, a stair, an NPC, the party's spawn, another piece or off
the board, or if any NPC, the stairs or a door can no longer be reached on foot from the spawn.

    python -I tools/scripts/maps/make_inn_3d_furniture.py
"""
import json
import os
import subprocess
from collections import deque

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, "content", "maps", "ember")

TABLE, CHAIR, STOOL = "ember-tavern-table-v02", "ember-tavern-chair-v02", "ember-interior-stool"
COUNTER, HEARTH, BED = "ember-tavern-counter", "ember-tavern-stone-hearth", "ember-interior-simple-bed-v02"
PAIR = {TABLE, COUNTER, HEARTH, BED}  # two hexes: the anchor and its east neighbour (rot 0)
# a 3D chair's back is toward +z (the next row) at rot 0; each quarter turn: 1 back toward -x, 2 toward -z, 3 toward +x
FACE_EAST, FACE_WEST, FACE_NORTH = 1, 3, 0
WALLS = {"wall-3d-tavern"}
DOORS = {"door-3d-frame"}
STAIRS = {"inn-stairs-up", "inn-stairs-down", "floor-connector"}
PASSABLE = {"light-candle", "light-wall-torch", "floor-connector"}  # cards that never hold the path


def head_text(path):
    rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
    return subprocess.run(["git", "-C", ROOT, "show", f"HEAD:{rel}"], capture_output=True, check=True).stdout.decode("utf-8")


def cells_of(d):
    return [(d["x"], d["y"]), (d["x"] + 1, d["y"])] if d["id"] in PAIR else [(d["x"], d["y"])]


def neighbours(x, y):
    odd = y & 1
    for dx, dy in ((1, 0), (-1, 0), (odd, -1), (odd - 1, -1), (odd, 1), (odd - 1, 1)):
        yield x + dx, y + dy


def check(draft, name):
    cols, rows = draft["cols"], draft["rows"]
    owner = {}
    for d in draft["decorations"]:
        for c in cells_of(d):
            if not (0 <= c[0] < cols and 0 <= c[1] < rows):
                raise SystemExit(f"{name}: {d['id']} at {d['x']},{d['y']} goes off the board")
            if d["id"] == "floor-connector":
                continue  # it shares the stairs' hex
            if c in owner and d["id"] not in PASSABLE and owner[c] not in PASSABLE:
                raise SystemExit(f"{name}: {d['id']} at {d['x']},{d['y']} lands on {owner[c]} at {c}")
            owner.setdefault(c, d["id"])
    people = {(s["x"], s["y"]): s.get("name", "?") for k in ("playerSpawns", "neutralSpawns", "enemySpawns") for s in draft.get(k, [])}
    for c, who in people.items():
        if c in owner and owner[c] not in PASSABLE:
            raise SystemExit(f"{name}: {who} at {c} stands on {owner[c]}")
    blocked = {c for c, i in owner.items() if i not in PASSABLE and i not in DOORS and i not in STAIRS}
    start = (draft["playerSpawns"][0]["x"], draft["playerSpawns"][0]["y"])
    seen, todo = {start}, deque([start])
    while todo:
        x, y = todo.popleft()
        for n in neighbours(x, y):
            if 0 <= n[0] < cols and 0 <= n[1] < rows and n not in seen and n not in blocked and n not in people:
                seen.add(n)
                todo.append(n)
    for c, who in people.items():
        if c != start and not any(n in seen for n in neighbours(*c)):
            raise SystemExit(f"{name}: nobody can walk beside {who} at {c}")
    for d in draft["decorations"]:
        if d["id"] in DOORS | STAIRS and (d["x"], d["y"]) not in seen:
            raise SystemExit(f"{name}: {d['id']} at {d['x']},{d['y']} cannot be reached")
    return len(seen)


def place(d, ident, x=None, y=None, rot=None):
    out = {"id": ident, "x": d["x"] if x is None else x, "y": d["y"] if y is None else y}
    if rot:
        out["rot"] = rot
    return out


def salao():
    src = os.path.join(MAPS, "estalagem006.json")
    m = json.loads(head_text(src))  # the committed 006 (the working copy carries pieces the game does not know)
    draft = m["draft"]
    out = []
    for d in draft["decorations"]:
        at = (d["id"], d["x"], d["y"])
        if at == ("city-workbench", 2, 4) or at == ("city-workbench", 4, 4):
            out.append(place(d, COUNTER))  # the bar beside Brue: two counters, 2..5 on row 4
        elif d["id"] == "city-workbench" and (d["x"], d["y"]) == (13, 7):
            out.append(place(d, TABLE, x=12))  # 12-13: its stool stays at 14
        elif d["id"] == "city-workbench":
            out.append(place(d, TABLE))
        elif at == ("city-stool", 6, 9):
            out.append(place(d, CHAIR, x=7, rot=FACE_WEST))  # the table now takes 5-6
        elif at == ("city-stool", 9, 7):
            out.append(place(d, CHAIR, rot=FACE_EAST))
        elif at == ("city-stool", 14, 7):
            out.append(place(d, CHAIR, rot=FACE_WEST))
        elif at == ("city-stool", 16, 2):
            out.append(place(d, CHAIR, rot=FACE_NORTH))  # by the fire, facing it
        elif d["id"] == "city-stool":
            out.append(place(d, CHAIR))
        elif d["id"] == "city-broken-chair":
            out.append(place(d, CHAIR))
        elif d["id"] == "inn-fireplace":
            out.append(place(d, HEARTH))
        else:
            out.append(d)
    # the long table at 18-19 gets a chair at each end
    out.append({"id": CHAIR, "x": 17, "y": 8, "rot": FACE_EAST})
    out.append({"id": CHAIR, "x": 20, "y": 8, "rot": FACE_WEST})
    draft["decorations"] = out
    m["serial"] = 7
    reach = check(draft, "estalagem007")
    return m, os.path.join(MAPS, "estalagem007.json"), reach


def rooms_of(draft):
    # each hex's room: flood fill over the hexes that are no wall and no doorway
    cols, rows = draft["cols"], draft["rows"]
    edge = {(d["x"], d["y"]) for d in draft["decorations"] if d["id"] in WALLS | DOORS}
    edge |= {(x, y) for y in range(rows) for x in range(cols) if x in (0, cols - 1) or y in (0, rows - 1)}  # the rim outside the walls
    room, n = {}, 0
    for y in range(rows):
        for x in range(cols):
            if (x, y) in edge or (x, y) in room:
                continue
            n += 1
            room[(x, y)] = n
            todo = deque([(x, y)])
            while todo:
                c = todo.popleft()
                for m in neighbours(*c):
                    if 0 <= m[0] < cols and 0 <= m[1] < rows and m not in edge and m not in room:
                        room[m] = n
                        todo.append(m)
    return room


def free_pair(x0, y0, owner, people, cols, rows, room):
    # the nearest free two-hex pair (same row) in the hex's own room: its own row first, then the rows beside it
    for dy in (0, 1, -1):
        for dx in (0, -1, 1, -2, 2):
            x, y = x0 + dx, y0 + dy
            pair = ((x, y), (x + 1, y))
            if not (0 <= x and x + 1 < cols and 0 <= y < rows):
                continue
            if all(c not in owner and c not in people and room.get(c) == room.get((x0, y0)) for c in pair):
                return x, y
    return None


def andar():
    src = os.path.join(MAPS, "estalagem-andar-2004.json")
    m = json.load(open(src, encoding="utf-8"))
    draft = m["draft"]
    cols, rows = draft["cols"], draft["rows"]
    room = rooms_of(draft)
    people = {(s["x"], s["y"]) for k in ("playerSpawns", "neutralSpawns") for s in draft.get(k, [])}
    keep = [d for d in draft["decorations"] if d["id"] != "city-workbench"]
    owner = {}
    for d in keep:
        for c in cells_of(d):
            owner.setdefault(c, d["id"])
    out = []
    for d in keep:
        out.append(place(d, STOOL) if d["id"] == "city-stool" else d)
    # a bed in every bedroom (two in the wide room above the corridor's east end); the stairs room keeps none
    for x, y in ((2, 1), (12, 1), (17, 1), (27, 1), (30, 1), (2, 13), (14, 19), (22, 19)):
        bed = {"id": BED, "x": x, "y": y}
        for c in cells_of(bed):
            if c in owner or c in people:
                raise SystemExit(f"estalagem-andar-2006: the bed at {x},{y} lands on {owner.get(c, 'someone')}")
            owner[c] = BED
        out.append(bed)
    for d in draft["decorations"]:
        if d["id"] != "city-workbench":
            continue
        at = free_pair(d["x"], d["y"], owner, people, cols, rows, room)
        if at is None:
            raise SystemExit(f"estalagem-andar-2006: no free pair for the table at {d['x']},{d['y']}")
        t = place(d, TABLE, x=at[0], y=at[1])
        for c in cells_of(t):
            owner[c] = TABLE
        out.append(t)
    draft["decorations"] = out
    m["serial"] = 6
    reach = check(draft, "estalagem-andar-2006")
    return m, os.path.join(MAPS, "estalagem-andar-2006.json"), reach


def main():
    made = [salao(), andar()]
    for m, path, reach in made:
        if os.path.exists(path):
            raise SystemExit(f"{os.path.basename(path)} already exists: a change is a new version, never an overwrite")
    for m, path, reach in made:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(m, f, ensure_ascii=False)
        print(f"wrote {os.path.relpath(path, ROOT)} ({reach} hexes reachable on foot)")


if __name__ == "__main__":
    main()
