"""Create a non-destructive Farmlands Revisited map draft from the current Farmlands layout."""
import copy
import json
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/farmlands002.json'
OUTPUT = ROOT / 'content/maps/ember/farmlands-revisited001.json'


def main():
    if OUTPUT.exists():
        raise SystemExit(f'{OUTPUT.relative_to(ROOT)} already exists; preserve it and choose a new revision.')
    source = json.loads(SOURCE.read_text(encoding='utf-8'))
    doc = copy.deepcopy(source)
    draft = doc['draft']
    cols, rows = draft['cols'], draft['rows']
    valid = { (x, y) for y in range(rows) for x in range(cols)
              if y * cols + x < len(draft['tiles']) and draft['tiles'][y * cols + x] != 'void' }
    houses = [(d['x'], d['y']) for d in draft['decorations'] if d.get('id', '').endswith('-house')]
    connectors = [(d['x'], d['y']) for d in draft['decorations'] if d.get('id') == 'floor-connector']
    trees = [(d['x'], d['y']) for d in draft['decorations'] if 'forest-photographic' in d.get('id', '')]
    occupied = {(d.get('x'), d.get('y')) for d in draft['decorations']}

    # Five separated crop blocks follow the open farm edges while leaving a broad central village and walkable lanes.
    zones = [
        (range(3, 10), range(4, 11), 1),
        (range(19, 29), range(3, 9), 2),
        (range(43, 52), range(5, 14), 3),
        (range(3, 12), range(23, 32), 4),
        (range(34, 43), range(31, 39), 5),
    ]
    wheat = set()
    for xs, ys, phase in zones:
        for y in ys:
            for x in xs:
                # Rounded field corners and narrow soil breaks keep the cultivated blocks organic.
                if ((x * 7 + y * 11 + phase * 5) % 13 == 0) or (abs(x - xs.start) <= 1 and abs(y - ys.start) <= 1 and phase % 2 == 0):
                    continue
                if (x, y) not in valid or (x, y) in occupied:
                    continue
                if any(max(abs(x - hx), abs(y - hy)) <= 2 for hx, hy in houses):
                    continue
                if any(max(abs(x - cx), abs(y - cy)) <= 1 for cx, cy in connectors + trees):
                    continue
                wheat.add((x, y))

    placed = []
    for x, y in sorted(wheat, key=lambda p: (p[1], p[0])):
        placed.append({'id': 'ember-farmlands-wheat-tile', 'x': x, 'y': y})

    # Low, scattered meadow clumps soften field boundaries without covering the central routes.
    meadow = set()
    for x, y in wheat:
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            p = x + dx, y + dy
            if p in valid and p not in wheat and p not in occupied and (p[0] * 5 + p[1] * 3) % 4 == 0:
                if not any(max(abs(p[0] - hx), abs(p[1] - hy)) <= 2 for hx, hy in houses):
                    meadow.add(p)
    for x, y in sorted(meadow, key=lambda p: (p[1], p[0])):
        placed.append({'id': 'ember-farmlands-tall-grass', 'x': x, 'y': y})

    for x, y in ((16, 13), (30, 13), (41, 21), (18, 35)):
        if (x, y) in valid and all(max(abs(x - hx), abs(y - hy)) > 2 for hx, hy in houses):
            placed.append({'id': 'ember-farmlands-hay-bales', 'x': x, 'y': y})

    draft['title'] = 'Farmlands Revisited'
    draft['place'] = 'Campos cultivados e pequenas casas'
    draft['briefing'] = 'Campos extensos de trigo e pasto cercam o povoado. Trilhas de terra ligam a ferraria, a botica, o armazém, a pousada e a casa dos moradores.'
    draft['objective'] = 'Atravesse os campos e visite as casas do povoado.'
    draft['decorations'].extend(placed)
    doc['serial'] = 3
    doc['savedAt'] = int(time.time() * 1000)
    OUTPUT.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'created {OUTPUT.relative_to(ROOT)}: {cols}x{rows}, {len(wheat)} wheat tiles, {len(meadow)} tall-grass tiles, '
          f'{sum(p["id"] == "ember-farmlands-hay-bales" for p in placed)} hay stacks; preserved {len(draft["decorations"]) - len(placed)} existing decorations')


if __name__ == '__main__':
    main()
