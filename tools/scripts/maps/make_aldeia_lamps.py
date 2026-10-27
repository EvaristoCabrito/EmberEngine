"""aldeia019 = aldeia017 with its four lantern-post cards (lamppost, city-lantern-post) replaced by the 3D street lamp v02 on the
same hexes (the user, 2026-10-27: "pode botar na aldeia"). Each lamp turned by quarter turns so its lantern hangs toward the
board's centre (rot r puts the lantern along +x, +z, -x, -z for r = 0..3: SceneBuilder's rotY = -r pi / 2). Never overwrites.

  python -I tools/scripts/maps/make_aldeia_lamps.py
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SRC = os.path.join(ROOT, 'content/maps/ember/aldeia017.json')
OUT = os.path.join(ROOT, 'content/maps/ember/aldeia019.json')
CARDS = {'lamppost', 'city-lantern-post'}
LAMP = 'ember-city-street-lamp-v02'


def main():
    if os.path.exists(OUT):
        print(f'{OUT} exists: make the next version instead', file=sys.stderr)
        return 1
    with open(SRC, encoding='utf-8') as f:
        doc = json.load(f)
    d = doc['draft']
    cx, cy = (d['cols'] - 1) / 2, (d['rows'] - 1) / 2
    swapped = 0
    for p in d['decorations']:
        if p['id'] not in CARDS:
            continue
        dx, dy = cx - p['x'], cy - p['y']
        angle = math.atan2(dy, dx)  # from +x toward +y (rows grow along +z)
        p['id'] = LAMP
        p['rot'] = int(round(angle / (math.pi / 2))) % 4
        swapped += 1
        print(f"  ({p['x']}, {p['y']}) -> {LAMP} rot {p['rot']}")
    if swapped != 4:
        print(f'expected 4 lantern posts, found {swapped}', file=sys.stderr)
        return 1
    doc['serial'] = 19
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(doc, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print(f'wrote {OUT}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
