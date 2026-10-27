"""Create an additive Wisp Forest Crossing pass using the Revisited prop kit."""
import copy
import json
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/wisp-forest-crossing012.json'
OUT = ROOT / 'content/maps/ember/wisp-forest-crossing014.json'

TREE_POSITIONS = {
    'ember-wisp-revisited-weeping-willow-v02': (8, 8),
    'ember-wisp-revisited-giant-dead-oak-v02': (6, 28),
    'ember-wisp-revisited-hollow-snag-v02': (29, 27),
    'ember-wisp-revisited-uprooted-leaning-v02': (17, 4),
    'ember-wisp-revisited-lightning-split-v02': (8, 20),
    'ember-wisp-revisited-spiral-twisted-v02': (11, 2),
    'ember-wisp-revisited-tall-burnt-pine-v02': (11, 6),
    'ember-wisp-revisited-broken-crown-v02': (7, 12),
    'ember-wisp-revisited-root-arch-v02': (10, 16),
}
PROP_POSITIONS = {
    'ember-wisp-revisited-broken-stump': (7, 5),
    'ember-wisp-revisited-rotted-hollow-log-v02': (36, 22),
    'ember-wisp-revisited-roots-and-fungi': (9, 31),
}


def axial(point):
    x, y = point
    return x - y // 2, y


def hex_distance(a, b):
    aq, ar = axial(a)
    bq, br = axial(b)
    return max(abs(aq - bq), abs(ar - br), abs(aq + ar - bq - br))


def main():
    if OUT.exists():
        raise SystemExit(f'preserving existing output; choose a new revision: {OUT}')
    data = copy.deepcopy(json.loads(SOURCE.read_text(encoding='utf-8')))
    draft = data['draft']
    cols, rows = draft['cols'], draft['rows']
    if (cols, rows) != (44, 36):
        raise SystemExit(f'unexpected Crossing dimensions: {cols}x{rows}')

    occupied = {(d['x'], d['y']) for d in draft['decorations']}
    occupied.update((s['x'], s['y']) for key in ('playerSpawns', 'enemySpawns', 'neutralSpawns') for s in draft.get(key, []))
    positions = {**TREE_POSITIONS, **PROP_POSITIONS}
    if len(TREE_POSITIONS) != 9 or len(PROP_POSITIONS) != 3:
        raise SystemExit('expected exactly nine trees and three props')
    for asset_id, point in positions.items():
        x, y = point
        if not (0 <= x < cols and 0 <= y < rows):
            raise SystemExit(f'{asset_id} is outside the map at {point}')
        terrain = draft['tiles'][y * cols + x]
        if terrain not in ('plains', 'woods'):
            raise SystemExit(f'{asset_id} lands on {terrain} at {point}')
        if point in occupied:
            raise SystemExit(f'{asset_id} overlaps existing gameplay content at {point}')

    tree_points = list(TREE_POSITIONS.values())
    tree_separation = min(hex_distance(a, b) for i, a in enumerate(tree_points) for b in tree_points[:i])
    if tree_separation < 4:
        raise SystemExit(f'new tree spacing is too tight: {tree_separation} hexes')

    # Apply the forest variants only to cells already authored as woods.
    woods = [(i % cols, i // cols) for i, terrain in enumerate(draft['tiles']) if terrain == 'woods']
    variants = list(draft['tileVariants'])
    for x, y in woods:
        variants[y * cols + x] = 62
    for x, y in woods:
        if min(hex_distance((x, y), p) for p in tree_points) <= 2:
            variants[y * cols + x] = 63
    landmarks = [(i % cols, i // cols) for i, terrain in enumerate(draft['tiles']) if terrain in ('ruins', 'water')]
    ash_cells = [p for p in woods if min(hex_distance(p, q) for q in landmarks) <= 2 and variants[p[1] * cols + p[0]] != 63]
    for x, y in ash_cells:
        variants[y * cols + x] = 64
    draft['tileVariants'] = variants

    for asset_id, (x, y) in positions.items():
        draft['decorations'].append({'id': asset_id, 'x': x, 'y': y})

    data['serial'] = int(data.get('serial', 0)) + 1
    data['savedAt'] = int(time.time() * 1000)
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    counts = Counter(variants[i] for i, terrain in enumerate(draft['tiles']) if terrain == 'woods')
    print(f'created {OUT.relative_to(ROOT)} from active campaign map {SOURCE.name}')
    print(f'preserved {len(draft["decorations"]) - len(positions)} original decorations, all spawns and crossing markers')
    print(f'added {len(TREE_POSITIONS)} trees and {len(PROP_POSITIONS)} props; tree spacing >= {tree_separation} hexes')
    print(f'woods variants: {dict(sorted(counts.items()))}')


if __name__ == '__main__':
    main()
