"""Create a cleaner Wisp Crossing revision without replacing earlier maps."""
import copy
import json
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/wisp-forest-crossing016.json'
OUT = ROOT / 'content/maps/ember/wisp-forest-crossing017.json'

# Keep a few older silhouettes around the outer lanes; the nine larger v02
# trees remain the visual anchors for the forest.
KEEP_COORDS = {
    'wilds-dead-oak': {(40, 6), (2, 19), (41, 25), (39, 32), (24, 7)},
    'dead-tree': {(22, 1), (39, 10), (37, 21), (3, 30), (29, 14)},
    'wilds-twisted-tree': {(4, 16), (32, 19), (39, 19), (31, 31), (20, 1)},
    'wilds-hollow-stump': {(6, 1), (38, 11), (28, 25), (11, 32)},
    'wilds-root-tangle': {(12, 10), (16, 16), (9, 21)},
    'wilds-tree-stump': {(8, 27), (34, 14), (24, 22)},
    'wilds-exposed-roots': {(22, 19), (36, 20), (13, 28)},
    'wilds-mossy-log': {(12, 5), (34, 19), (27, 11)},
    'wilds-mossy-stones': {(10, 21), (35, 18), (12, 15)},
    'wilds-shelf-mushrooms': {(18, 12), (33, 14), (7, 25)},
    'wilds-bare-branch': {(14, 16), (27, 28)},
    'wilds-wisp-mushrooms': {(8, 11), (36, 17), (30, 10)},
    'wilds-mushroom-cluster': {(16, 17), (34, 15)},
    'wilds-dry-twig-pile': {(6, 24), (35, 20)},
}
REMOVE_IDS = {'city-crate-stack', 'city-log-stack', 'city-barrels'}


def main():
    if OUT.exists():
        raise SystemExit(f'preserving existing output; choose a new revision: {OUT}')

    data = copy.deepcopy(json.loads(SOURCE.read_text(encoding='utf-8')))
    draft = data['draft']
    original = json.loads(SOURCE.read_text(encoding='utf-8'))['draft']
    before_counts = Counter(item['id'] for item in draft['decorations'])

    for item_id, coords in KEEP_COORDS.items():
        available = {(item['x'], item['y']) for item in draft['decorations'] if item['id'] == item_id}
        missing = coords - available
        if missing:
            raise SystemExit(f'{item_id} keep coordinates are missing: {sorted(missing)}')
        if len(coords) > before_counts[item_id]:
            raise SystemExit(f'{item_id} keep count exceeds source count')

    kept = []
    removed = Counter()
    for item in draft['decorations']:
        item_id = item['id']
        point = (item['x'], item['y'])
        if item_id in REMOVE_IDS:
            removed[item_id] += 1
            continue
        if item_id in KEEP_COORDS and point not in KEEP_COORDS[item_id]:
            removed[item_id] += 1
            continue
        kept.append(item)

    draft['decorations'] = kept
    # Restore the original Crossing visibility behavior from revision 015.
    draft['fog'] = True

    for key in ('tiles', 'tileVariants', 'playerSpawns', 'enemySpawns', 'neutralSpawns'):
        if draft.get(key) != original.get(key):
            raise SystemExit(f'unexpected change to preserved map data: {key}')
    if sum(len(draft.get(key, [])) for key in ('playerSpawns', 'enemySpawns', 'neutralSpawns')) != 33:
        raise SystemExit('expected all 33 existing unit spawns to remain')
    if sum(1 for item in kept if item['id'].startswith('ember-wisp-revisited-') and item['id'].endswith('-v02')) != 9:
        raise SystemExit('expected all nine large Revisited trees to remain')

    data['serial'] = int(data.get('serial', 0)) + 1
    data['savedAt'] = int(time.time() * 1000)
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    after_counts = Counter(item['id'] for item in kept)
    print(f'created {OUT.relative_to(ROOT)} from {SOURCE.name}')
    print(f'decorations: {len(draft["decorations"])} kept of {sum(before_counts.values())}; removed {sum(removed.values())}')
    print('removed by id:', dict(sorted(removed.items())))
    print('kept units: 33; kept Revisited trees: 9; fog:', draft['fog'])
    print('new decoration totals by id:', dict(sorted(after_counts.items())))


if __name__ == '__main__':
    main()
