"""Create the additive Wisp Forest 3D revision from the latest saved map draft."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/wisp-forest007.json'
OUT = ROOT / 'content/maps/ember/wisp-forest008.json'

TREES = [
    ('ember-wisp-revisited-weeping-willow', 3, 1),
    ('ember-wisp-revisited-giant-dead-oak', 10, 1),
    ('ember-wisp-revisited-hollow-snag', 17, 1),
    ('ember-wisp-revisited-uprooted-leaning', 2, 6),
    ('ember-wisp-revisited-lightning-split', 18, 5),
    ('ember-wisp-revisited-spiral-twisted', 3, 16),
    ('ember-wisp-revisited-tall-burnt-pine', 17, 15),
    ('ember-wisp-revisited-broken-crown', 7, 18),
    ('ember-wisp-revisited-root-arch', 10, 19),
]
PROPS = [
    ('ember-wisp-revisited-broken-stump', 4, 3),
    ('ember-wisp-revisited-rotted-hollow-log', 5, 14),
    ('ember-wisp-revisited-roots-and-fungi', 14, 15),
]


def main():
    if OUT.exists():
        raise SystemExit(f'preserving existing output; choose a new revision: {OUT}')
    original = json.loads(SOURCE.read_text(encoding='utf-8'))
    data = copy.deepcopy(original)
    d = data['draft']
    cols, rows = d['cols'], d['rows']
    if (cols, rows) != (20, 20):
        raise SystemExit(f'unexpected source dimensions: {cols}x{rows}')
    d['id'] = 'wisp-forest-revisited'
    d['title'] = 'Wisp Forest Revisited'
    d['briefing'] = ('A luz desaparece sob as árvores antigas. Troncos mortos e raízes expostas '
                     'cercam uma clareira de cinza e folhas, enquanto wisps rubros permanecem no '
                     'caminho para o santuário soterrado.')

    # Preserve the source outline and every existing spawn/object. Retheme only the playable cells.
    valid = set()
    for i, tile in enumerate(d['tiles']):
        if tile == 'void':
            d['tileVariants'][i] = 0
            continue
        x, y = i % cols, i // cols
        d['tiles'][i] = 'woods'
        # The irregular ashen clearing sits off-centre so the path still feels authored.
        if (7 <= x <= 12 and 7 <= y <= 12 and ((x + 2*y) % 5 != 0)):
            d['tileVariants'][i] = 64
        else:
            d['tileVariants'][i] = 62
        valid.add((x, y))
    d['baseTile'] = 'woods'
    d['baseVariant'] = 62

    existing = {(z['x'], z['y']) for z in d['decorations']}
    additions = []
    for model, x, y in TREES + PROPS:
        if (x, y) not in valid:
            raise SystemExit(f'placement is outside the irregular playable outline: {model} at {x},{y}')
        if (x, y) in existing:
            raise SystemExit(f'placement overlaps existing authored content: {model} at {x},{y}')
        additions.append({'id': model, 'x': x, 'y': y})
    d['decorations'].extend(additions)
    # Root-textured cells hug the new trunks and existing shrine, retaining the leaf-litter clearing.
    root_centres = [(x, y) for _, x, y in TREES] + [(16, 12)]
    for y in range(rows):
        for x in range(cols):
            i = y*cols+x
            if d['tiles'][i] != 'woods':
                continue
            if any(abs(x-cx) + abs(y-cy) <= 2 for cx, cy in root_centres):
                d['tileVariants'][i] = 63

    data['serial'] = int(original.get('serial', 0)) + 1
    data['savedAt'] = '2026-10-24T00:00:00.000Z'
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'created {OUT.relative_to(ROOT)}: {len(valid)} woods cells, {len(TREES)} new trees, {len(PROPS)} new props')


if __name__ == '__main__':
    main()
