"""The watchtower's walls in EmberEngine's own kit (content/models/architecture.json), on a saved draft:

  python -I tools/scripts/maps/watchtower_walls.py <in draft.json> <out draft.json> [--windows] [--torches] [--door]

Every `wall-3d-tower` becomes `ember-arch-watchtower-wall` and the flat watchtower entrance becomes the kit's portal
(`ember-arch-watchtower-portal`, same hexes; with --door the closed two-leaf door `ember-arch-watchtower-door`, the user's
"size 2 door for the main entrance"). With --windows, the outer ring's straight runs get openings: every third piece of a run (not
at a corner or a junction), alternating a lancet window and an arrow slit. With --torches, every `light-wall-torch` card standing
beside a plain wall piece moves onto it: the piece becomes `ember-arch-watchtower-wall-torch` (a 3D bracket, torch and flame on the
room's side, its own light) and the card is removed. Nothing else in the draft changes; the serial is the caller's.

The outer ring: a wall hex from which a straight walk away from the board's centre (perpendicular to its run) leaves the board
without meeting another wall.
"""
import json
import sys

WALL_OLD, WALL_NEW = 'wall-3d-tower', 'ember-arch-watchtower-wall'
DOOR_OLD, PORTAL, DOOR = 'watchtower-stone-open-door-2hex', 'ember-arch-watchtower-portal', 'ember-arch-watchtower-door'
GATE = 'ember-arch-watchtower-gate'
WINDOW, SLIT, TORCH = 'ember-arch-watchtower-window', 'ember-arch-watchtower-slit', 'ember-arch-watchtower-wall-torch'
TORCH_CARD = 'light-wall-torch'
WALL_IDS = {WALL_OLD, WALL_NEW, WINDOW, SLIT, TORCH}


def main(src, dst, windows=False, torches=False, door=False, gate=False):
    with open(src, encoding='utf-8') as f:
        file = json.load(f)
    draft = file['draft']
    cols, rows = draft['cols'], draft['rows']
    decos = draft['decorations']
    swapped = doors = 0
    for d in decos:
        if d['id'] == WALL_OLD:
            d['id'] = WALL_NEW
            swapped += 1
        elif d['id'] in (DOOR_OLD, PORTAL, DOOR, GATE):
            d['id'] = GATE if gate else DOOR if door else PORTAL
            doors += 1
    if gate:
        # the gate is four hexes wide (anchor - 1 .. anchor + 2): the walls on its two extra cells go
        gates = [(d['x'], d['y']) for d in decos if d['id'] == GATE]
        taken = {(gx + dx, gy) for (gx, gy) in gates for dx in (-1, 0, 1, 2)}
        before = len(decos)
        decos[:] = [d for d in decos if not (d['id'] in WALL_IDS and (d['x'], d['y']) in taken)]
        print(f'{before - len(decos)} wall pieces removed for the gate')
    walls = {(d['x'], d['y']): d for d in decos if d['id'] in WALL_IDS}
    cx, cy = (cols - 1) / 2, (rows - 1) / 2

    def outer(x, y, dx, dy):
        x, y = x + dx, y + dy
        while 0 <= x < cols and 0 <= y < rows:
            if (x, y) in walls:
                return False
            x, y = x + dx, y + dy
        return True

    def junction(x, y, horizontal):
        return ((x, y - 1) in walls or (x, y + 1) in walls) if horizontal else ((x - 1, y) in walls or (x + 1, y) in walls)

    opened = 0
    if windows:
        # straight runs: maximal lines of wall hexes along a row (horizontal) or a column (vertical), at least 3 long
        runs = []
        for horizontal in (True, False):
            for (x, y) in sorted(walls):
                before = (x - 1, y) if horizontal else (x, y - 1)
                if before in walls:
                    continue
                run = []
                px, py = x, y
                while (px, py) in walls:
                    run.append((px, py))
                    px, py = (px + 1, py) if horizontal else (px, py + 1)
                if len(run) >= 3:
                    runs.append((horizontal, run))
        kind = 0
        for horizontal, run in runs:
            # away from the centre: up / down for a horizontal run, left / right for a vertical one
            x0, y0 = run[0]
            dx, dy = (0, -1 if y0 < cy else 1) if horizontal else (-1 if x0 < cx else 1, 0)
            for k, (x, y) in enumerate(run):
                if k == 0 or k == len(run) - 1 or (k % 3) != 1:
                    continue
                if junction(x, y, horizontal) or not outer(x, y, dx, dy) or walls[(x, y)]['id'] != WALL_NEW:
                    continue
                walls[(x, y)]['id'] = WINDOW if kind % 2 == 0 else SLIT
                kind += 1
                opened += 1
    moved = 0
    if torches:
        keep = []
        for d in decos:
            if d['id'] != TORCH_CARD:
                keep.append(d)
                continue
            x, y = d['x'], d['y']
            # the plain wall beside the card (orthogonal first, then diagonal), not a corner, not already given something
            target = None
            for (nx, ny) in ((x, y - 1), (x, y + 1), (x - 1, y), (x + 1, y), (x - 1, y - 1), (x + 1, y - 1), (x - 1, y + 1), (x + 1, y + 1)):
                w = walls.get((nx, ny))
                if not w or w['id'] != WALL_NEW:
                    continue
                horizontal = (nx - 1, ny) in walls or (nx + 1, ny) in walls
                if junction(nx, ny, horizontal) and ((nx, ny - 1) in walls or (nx, ny + 1) in walls) and ((nx - 1, ny) in walls or (nx + 1, ny) in walls):
                    continue  # a corner / junction
                target = w
                break
            if target is None:
                keep.append(d)
                continue
            target['id'] = TORCH
            moved += 1
        draft['decorations'] = keep
    with open(dst, 'w', encoding='utf-8') as f:
        json.dump(file, f, ensure_ascii=False, indent=2)
    print(f'{dst}: {swapped} walls, {doors} {"gates" if gate else "doors" if door else "portals"}, {opened} openings, {moved} torches onto walls')


if __name__ == '__main__':
    flags = sys.argv[3:]
    main(sys.argv[1], sys.argv[2], '--windows' in flags, '--torches' in flags, '--door' in flags, '--gate' in flags)
