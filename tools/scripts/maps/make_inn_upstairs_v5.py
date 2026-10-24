"""The Inn's second floor, version 5 (2026-10-22; the user: "comece pelo segundo andar do INN, ele tem uma porção de detalhes
estranhos e nada a ver, deixa profissional"): estalagem-andar-2004 rebuilt on the Inn's own 3D kit (content/models/architecture.json
ember-arch-inn-*), with the light of the hour coming in through real windows.

  python -I tools/scripts/maps/make_inn_upstairs_v5.py [content/maps/ember/estalagem-andar-2004.json] [content/maps/ember/estalagem-andar-2005.json]

What changes against 004 (the board, its tiles, the rooms' walls' positions, the NPCs, the spawn and the stairs stay where they are):
  - every brick wall (wall-3d-tavern) becomes the Inn's plastered timber-framed wall;
  - an outer ring of the same walls on the board's rim (the void cells of row 0, row 20, column 0 and column 32), so the floor is a
    closed building: a casement window on it for every room (two for the wide ones) and one at each end of the corridor; the sun
    and the moon come in through them (MapStage "Luz das janelas", on by default for this map);
  - the rooms' door frames become the kit's one-hex door (animated: it swings into the room when someone stands beside it); the
    stairs room keeps an open doorway (the user, 2026-10-17: no lock there);
  - the seven torch cards standing on the void become candle lanterns on the corridor's walls (ember-arch-inn-sconce);
  - the bedrooms get what bedrooms have: a bed (the Torre's photographed simple bed), a stool, a trunk, a candle beside the bed, a rug
    or a bookshelf where the guest reads; the sack piles, workbenches, brazier, firewood and buckets go (004 keeps them);
  - the corridor: a bench against each wall, a barrel at either end.
"""
import json
import sys
import time

WALL, WINDOW, DOOR, DOORWAY, SCONCE = ('ember-arch-inn-wall', 'ember-arch-inn-window', 'ember-arch-inn-door', 'ember-arch-inn-doorway',
                                       'ember-arch-inn-sconce')
BED, STOOL, CRATE, BARREL, SHELF, BENCH, RUG, JUG = ('ember-interior-simple-bed-v02', 'ember-interior-stool', 'ember-props-crate-v02',
                                                    'ember-props-barrel-v02', 'ember-interior-bookshelf', 'ember-interior-long-bench-v02',
                                                    'ember-interior-worn-rug', 'ember-interior-beer-jug')
CANDLE = 'light-candle'
KEEP = {'inn-stairs-down', 'floor-connector'}

# the ring's windows: (x, y); the rooms' doors: (x, y, rot) (rot 2 = the leaf swings north, into a room above the corridor)
WINDOWS = [(4, 0), (11, 0), (18, 0), (25, 0), (29, 0), (3, 20), (8, 20), (15, 20), (22, 20), (29, 20), (0, 4), (0, 10), (0, 16), (32, 4), (32, 10), (32, 16)]
DOORS = [(4, 8, 2), (10, 8, 2), (17, 8, 2), (26, 8, 2), (6, 12, 0), (15, 12, 0), (21, 12, 0)]
DOORWAYS = [(29, 12, 0)]
SCONCES = [(7, 8), (13, 8), (20, 8), (29, 8), (3, 12), (9, 12), (17, 12), (24, 12)]
# the rooms' furniture: (id, x, y); a pair piece (bed, bench, rug) takes (x, y) and (x + 1, y)
FURNITURE = [
    # R1 (x 1-7, Tomas o Febril): a sick man's room
    (BED, 2, 1), (STOOL, 4, 1), (CANDLE, 5, 1), (CRATE, 1, 2), (RUG, 2, 5),
    # R2 (x 9-14, Dorminhoco, Nara the cartographer)
    (BED, 10, 1), (STOOL, 12, 1), (CANDLE, 13, 1), (SHELF, 9, 2), (JUG, 13, 2),
    # R3 (x 16-21, Dona Anselma)
    (BED, 17, 1), (STOOL, 19, 1), (CANDLE, 20, 1), (CRATE, 16, 2),
    # R4 (x 23-31, Garrick o Caçador, Erasmo the scholar): two beds
    (BED, 24, 1), (BED, 29, 1), (STOOL, 26, 1), (CANDLE, 27, 1), (SHELF, 31, 2), (CRATE, 23, 2), (RUG, 26, 5),
    # R5 (x 1-10, Berta, Nessa): two beds
    (BED, 2, 19), (BED, 7, 19), (STOOL, 4, 19), (STOOL, 9, 19), (CANDLE, 1, 18), (CANDLE, 10, 18), (CRATE, 5, 18), (RUG, 4, 14),
    # R6 (x 12-18, O Peregrino, Ada)
    (BED, 13, 19), (STOOL, 15, 19), (CANDLE, 16, 19), (CRATE, 17, 18),
    # R7 (x 20-25, Homem de Capuz)
    (BED, 22, 19), (STOOL, 24, 19), (CANDLE, 21, 19), (CRATE, 20, 18),
    # R8 (x 27-31, the stairs): the inn's stores
    (BARREL, 27, 13), (BARREL, 28, 13), (CRATE, 31, 13),
    # the corridor
    (BARREL, 1, 9), (BARREL, 31, 11), (BENCH, 12, 9), (BENCH, 23, 11),
]
PAIRS = {BED, BENCH, RUG}


def main(src='content/maps/ember/estalagem-andar-2004.json', dst='content/maps/ember/estalagem-andar-2005.json'):
    with open(src, encoding='utf-8') as f:
        file = json.load(f)
    draft = file['draft']
    cols, rows = draft['cols'], draft['rows']
    old = draft['decorations']
    walls = {(d['x'], d['y']) for d in old if d['id'] == 'wall-3d-tavern'}
    doors_old = {(d['x'], d['y']) for d in old if d['id'] == 'door-3d-frame'}
    for x, y, _ in DOORS + DOORWAYS:
        if (x, y) not in doors_old:
            raise SystemExit(f'no door frame at {x},{y} in {src}')
    candles_old = [d for d in old if d['id'] == CANDLE]
    kept = [d for d in old if d['id'] in KEEP]
    # the outer ring on the rim
    ring = {(x, 0) for x in range(cols)} | {(x, rows - 1) for x in range(cols)} | {(0, y) for y in range(rows)} | {(cols - 1, y) for y in range(rows)}
    walls |= ring
    for x, y in WINDOWS:
        if (x, y) not in ring:
            raise SystemExit(f'window {x},{y} is not on the ring')
    for x, y in SCONCES:
        if (x, y) not in walls:
            raise SystemExit(f'sconce {x},{y} is not on a wall')
    taken = set()
    decos = []
    for (x, y) in sorted(walls):
        if (x, y) in doors_old:
            continue
        pid = WINDOW if (x, y) in WINDOWS else SCONCE if (x, y) in SCONCES else WALL
        vertical = (x, y - 1) in walls or (x, y + 1) in walls
        horizontal = (x - 1, y) in walls or (x + 1, y) in walls
        decos.append({'id': pid, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal' if horizontal or not vertical else 'vertical'})
        taken.add((x, y))
    for pid, places in ((DOOR, DOORS), (DOORWAY, DOORWAYS)):
        for x, y, rot in places:
            decos.append({'id': pid, 'x': x, 'y': y, 'rot': rot, 'wallOrientation': 'horizontal'})
            taken.add((x, y))
    for d in kept:
        decos.append(d)
        taken.add((d['x'], d['y']))
        if d['id'] == 'inn-stairs-down':
            taken.add((d['x'] + 1, d['y']))
    npcs = {(s['x'], s['y']) for s in draft['neutralSpawns']} | {(s['x'], s['y']) for s in draft['playerSpawns']}
    candle = dict(candles_old[0]) if candles_old else {'id': CANDLE}
    for pid, x, y in FURNITURE:
        cells = [(x, y), (x + 1, y)] if pid in PAIRS else [(x, y)]
        for c in cells:
            if c in taken:
                raise SystemExit(f'{pid} at {x},{y}: {c} already taken')
            if c in npcs and pid not in (RUG,):
                raise SystemExit(f'{pid} at {x},{y}: an NPC stands on {c}')
            if not (0 < c[0] < cols - 1 and 0 < c[1] < rows - 1):
                raise SystemExit(f'{pid} at {x},{y}: {c} is on the rim')
            taken.add(c)
        d = dict(candle) if pid == CANDLE else {'id': pid}
        d.update({'id': pid, 'x': x, 'y': y})
        decos.append(d)
    draft['decorations'] = decos
    file['serial'] = 5
    file['savedAt'] = int(time.time() * 1000)
    with open(dst, 'w', encoding='utf-8') as f:
        json.dump(file, f, ensure_ascii=False, indent=2)
    from collections import Counter
    print(f'{dst}: {len(decos)} decorations:', dict(Counter(d['id'] for d in decos)))


if __name__ == '__main__':
    main(*sys.argv[1:])
