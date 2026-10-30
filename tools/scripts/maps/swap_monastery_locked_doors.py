"""The Monastery's locked doors in 3D (2026-10-30; the user: "we have real 3D doors, stop using 2D props"): every 2D
`dungeon-stone-door` (a pair) on the newest serial of each Monastery map becomes the kit's `ember-arch-mosteiro-door-locked`
(the same pair of hexes, the same anchor; a lock the Gazua opens, shut until then), written as the NEXT serial of the map, the
user's own edits in that serial kept (nothing else changes). Never overwrites.

  python -I tools/scripts/maps/swap_monastery_locked_doors.py [map id ...]
"""
import json
import os
import sys

sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')
OLD, NEW = 'dungeon-stone-door', 'ember-arch-mosteiro-door-locked'
IDS = sys.argv[1:] or ['mosteiro', 'mosteiro-andar-2', 'mosteiro-andar-3', 'mosteiro-masmorras', 'mosteiro-laboratorio']


def newest(map_id):
    best, file = -1, None
    for f in os.listdir(MAPS):
        if f.startswith(map_id) and len(f) == len(map_id) + 8 and f[len(map_id):-5].isdigit() and int(f[len(map_id):-5]) > best:
            best, file = int(f[len(map_id):-5]), f
    return best, file


for map_id in IDS:
    serial, file = newest(map_id)
    if not file:
        raise SystemExit(f'no map file for {map_id}')
    path = os.path.join(MAPS, file)
    text = open(path, encoding='utf-8').read()
    data = json.loads(text)
    draft = data['draft']
    swapped = []
    for d in draft['decorations']:
        if d['id'] == OLD:
            d['id'] = NEW
            swapped.append((d['x'], d['y']))
    if not swapped:
        print(f'{file}: no {OLD}, left alone')
        continue
    out = os.path.join(MAPS, f'{map_id}{serial + 1:03d}.json')
    if os.path.exists(out):
        raise SystemExit(f'{out} exists')
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print(f'{file} -> {os.path.basename(out)}: {len(swapped)} door(s) at {swapped}')
