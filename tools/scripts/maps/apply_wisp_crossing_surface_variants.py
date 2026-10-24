"""Apply the forest floor materials to a new, preserved Wisp Crossing revision."""
import copy
import json
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/wisp-forest-crossing014.json'
OUT = ROOT / 'content/maps/ember/wisp-forest-crossing015.json'


def axial(point):
    x, y = point
    return x - y // 2, y


def distance(a, b):
    aq, ar = axial(a)
    bq, br = axial(b)
    return max(abs(aq - bq), abs(ar - br), abs(aq + ar - bq - br))


def main():
    if OUT.exists():
        raise SystemExit(f'preserving existing output; choose a new revision: {OUT}')
    data = copy.deepcopy(json.loads(SOURCE.read_text(encoding='utf-8')))
    draft = data['draft']
    cols, rows = draft['cols'], draft['rows']
    forest_cells = [(i % cols, i // cols) for i, t in enumerate(draft['tiles']) if t in ('plains', 'woods')]
    water_cells = [(i % cols, i // cols) for i, t in enumerate(draft['tiles']) if t == 'water']
    tree_points = [(d['x'], d['y']) for d in draft['decorations'] if d['id'].startswith('ember-wisp-revisited-') and d['id'].endswith('-v02')]
    if len(tree_points) != 9 or not water_cells:
        raise SystemExit(f'expected nine Revisited trees and water cells, got {len(tree_points)} trees and {len(water_cells)} water cells')

    variants = list(draft['tileVariants'])
    for x, y in forest_cells:
        variants[y * cols + x] = 62
    for x, y in forest_cells:
        if min(distance((x, y), p) for p in tree_points) <= 2:
            variants[y * cols + x] = 63
    for x, y in forest_cells:
        if min(distance((x, y), p) for p in water_cells) <= 1 and variants[y * cols + x] != 63:
            variants[y * cols + x] = 64
    draft['tileVariants'] = variants
    data['serial'] = int(data.get('serial', 0)) + 1
    data['savedAt'] = int(time.time() * 1000)
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    counts = Counter((draft['tiles'][i], variant) for i, variant in enumerate(variants) if draft['tiles'][i] in ('plains', 'woods'))
    print(f'created {OUT.relative_to(ROOT)} from {SOURCE.name}; no objects, spawns or terrain types changed')
    print('forest-floor variants by terrain/index:', dict(sorted(counts.items())))


if __name__ == '__main__':
    main()
