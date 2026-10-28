"""Append the Wisp Forest Revisited Blender kit to EmberEngine model/editor catalogues."""
import json
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
KIT = ROOT / 'content/models/wisp-forest-revisited'
ASSETS = [
    ('weeping-willow', 'Salgueiro morto pendente 3D', 'Natureza · Wisp Forest', True),
    ('giant-dead-oak', 'Carvalho morto gigante 3D', 'Natureza · Wisp Forest', True),
    ('hollow-snag', 'Árvore morta oca 3D', 'Natureza · Wisp Forest', True),
    ('uprooted-leaning', 'Árvore morta inclinada e arrancada 3D', 'Natureza · Wisp Forest', True),
    ('lightning-split', 'Árvore morta partida por raio 3D', 'Natureza · Wisp Forest', True),
    ('spiral-twisted', 'Árvore morta espiralada 3D', 'Natureza · Wisp Forest', True),
    ('tall-burnt-pine', 'Pinheiro alto queimado 3D', 'Natureza · Wisp Forest', True),
    ('broken-crown', 'Árvore morta de copa quebrada 3D', 'Natureza · Wisp Forest', True),
    ('root-arch', 'Arco natural de raízes mortas 3D', 'Natureza · Wisp Forest', True),
    ('leaf-litter-tile', 'Tile 3D de folhas mortas · Wisp', 'Tiles do chão · Wisp Forest', False),
    ('rooted-soil-tile', 'Tile 3D de solo com raízes · Wisp', 'Tiles do chão · Wisp Forest', False),
    ('ash-leaf-tile', 'Tile 3D de cinza e folhas · Wisp', 'Tiles do chão · Wisp Forest', False),
    ('broken-stump', 'Toco partido e enraizado 3D · Wisp', 'Natureza · Wisp Forest', True),
    ('rotted-hollow-log', 'Tronco oco em decomposição 3D · Wisp', 'Natureza · Wisp Forest', True),
    ('roots-and-fungi', 'Raízes com fungos silvestres 3D · Wisp', 'Natureza · Wisp Forest', False),
]


def glb_info(path):
    raw = path.read_bytes()
    if raw[:4] != b'glTF':
        raise SystemExit(f'not a GLB asset: {path}')
    n = struct.unpack_from('<I', raw, 12)[0]
    doc = json.loads(raw[20:20+n].decode('utf-8').rstrip('\x00 '))
    accessors = doc.get('accessors', [])
    positions = [a for a in accessors if a.get('type') == 'VEC3' and 'min' in a and 'max' in a]
    if not positions:
        raise SystemExit(f'missing position bounds: {path}')
    low = [min(a['min'][i] for a in positions) for i in range(3)]
    high = [max(a['max'][i] for a in positions) for i in range(3)]
    dims = [high[i]-low[i] for i in range(3)]  # glTF is Y-up: dims[1] is height.
    tris = sum(p.get('indices', 0) and accessors[p['indices']]['count'] // 3
               or next((accessors[a]['count']-2 for a in p.get('attributes', {}).values()
                        if accessors[a].get('type') == 'VEC3'), 0)
               for mesh in doc.get('meshes', []) for p in mesh.get('primitives', []) if p.get('mode', 4) == 4)
    return max(dims[0], dims[2]), dims[1], tris


def append_if_absent(items, entry, key='id'):
    found = next((x for x in items if x[key] == entry[key]), None)
    if found is None:
        items.append(entry)
    elif found != entry:
        raise SystemExit(f'preserving conflicting existing {key}: {entry[key]}')


def main():
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    editor.setdefault('decoMeta', {})
    editor.setdefault('decorationInfo', {})
    editor.setdefault('architectureOrder', [])
    for asset, name, section, blocks in ASSETS:
        tree_version = 'v02' if asset in {a[0] for a in ASSETS[:9]} else 'v01'
        stem = f'{asset}-{tree_version}'
        glb = KIT / f'{stem}.glb'
        if not glb.is_file():
            raise SystemExit(f'missing exported asset: {glb}')
        width, height, tris = glb_info(glb)
        mid = f'ember-wisp-revisited-{asset}' + ('-v02' if tree_version == 'v02' else '')
        model = {'id': mid, 'name': name, 'kit': 'wisp-forest-revisited', 'model': asset,
                 'file': f'/game/ember/wisp-forest-revisited/{stem}.glb', 'height': round(height, 3),
                 'width': round(width, 3), 'footprint': 'one', 'section': section,
                 'blocks': blocks, 'triangles': tris}
        append_if_absent(models['models'], model)
        decoration = {'name': name, 'footprint': [{'dx': 0, 'dy': 0}], 'blockingFootprint': None,
                      'architectureSpan': False, 'architecture': False, 'model3d': 'prop',
                      'model': {'model': asset, 'file': model['file'], 'height': model['height']},
                      'mirrorAlternate': False, 'exitKind': None,
                      'size': {'w': model['width'], 'h': model['height']}, 'light': None,
                      'standOn': False, 'lockYaw': False, 'nudge': 0}
        existing = catalog['decorations'].get(mid)
        if existing is None:
            catalog['decorations'][mid] = decoration
        elif existing != decoration:
            raise SystemExit(f'preserving conflicting catalog decoration: {mid}')
        meta = {'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False,
                'thickWall': False, 'doorStyle': None, 'wallTexture': None, 'solidHouse': False,
                'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True, 'art': ''}
        info = {'section': section, 'tile': None, 'thumb': '', 'blocksByDefault': blocks}
        for key, entry in (('decoMeta', meta), ('decorationInfo', info)):
            existing = editor[key].get(mid)
            if existing is None:
                editor[key][mid] = entry
            elif existing != entry:
                raise SystemExit(f'preserving conflicting catalog {key}: {mid}')
        if mid not in editor['architectureOrder']:
            editor['architectureOrder'].append(mid)

    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temp = CATALOG.with_name(CATALOG.name + '.wisp-revisited.tmp')
    temp.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    os.replace(temp, CATALOG)
    print(f'registered {len(ASSETS)} Wisp Forest Revisited GLBs without removing existing entries')


if __name__ == '__main__':
    main()
