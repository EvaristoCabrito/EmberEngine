"""Create Bosque Morto colored tree variants from the existing dead-tree GLBs and register them."""
import json
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/models/wisp-forest-revisited'
KIT = ROOT / 'content/models/bosque-morto-v01'
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/engine2/editor-catalog.json'
ASSETS = [
    ('giant-dead-oak', 'Carvalho morto gigante · Bosque Morto', 'giant-dead-oak-v02.glb'),
    ('hollow-snag', 'Árvore morta oca · Bosque Morto', 'hollow-snag-v02.glb'),
    ('lightning-split', 'Árvore morta partida · Bosque Morto', 'lightning-split-v02.glb'),
    ('weeping-willow', 'Salgueiro morto pendente · Bosque Morto', 'weeping-willow-v02.glb'),
    ('uprooted-leaning', 'Árvore morta inclinada · Bosque Morto', 'uprooted-leaning-v02.glb'),
]
COLOR = [0.48, 0.30, 0.20, 1.0]


def glb_document(data):
    if data[:4] != b'glTF' or struct.unpack_from('<I', data, 4)[0] != 2:
        raise ValueError('expected a glTF 2.0 binary')
    size = struct.unpack_from('<I', data, 12)[0]
    if struct.unpack_from('<I', data, 16)[0] != 0x4E4F534A:
        raise ValueError('first GLB chunk is not JSON')
    return json.loads(data[20:20 + size].decode('utf-8')), data[20 + size:]


def encode_glb(doc, tail):
    chunk = json.dumps(doc, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    chunk += b' ' * ((-len(chunk)) % 4)
    return b'glTF' + struct.pack('<II', 2, 12 + 8 + len(chunk) + len(tail)) + struct.pack('<II', len(chunk), 0x4E4F534A) + chunk + tail


def metrics(path):
    doc, _ = glb_document(path.read_bytes())
    positions = [a for a in doc.get('accessors', []) if a.get('type') == 'VEC3' and 'min' in a and 'max' in a]
    low = [min(a['min'][i] for a in positions) for i in range(3)]
    high = [max(a['max'][i] for a in positions) for i in range(3)]
    dims = [high[i] - low[i] for i in range(3)]
    tris = sum((doc['accessors'][p['indices']]['count'] // 3) if 'indices' in p else
               next((doc['accessors'][a]['count'] - 2 for a in p.get('attributes', {}).values()
                     if doc['accessors'][a].get('type') == 'VEC3'), 0)
               for mesh in doc.get('meshes', []) for p in mesh.get('primitives', []) if p.get('mode', 4) == 4)
    return max(dims[0], dims[2]), dims[1], tris


def append_or_check(items, entry, key):
    current = next((x for x in items if x[key] == entry[key]), None)
    if current is None:
        items.append(entry)
    elif current != entry:
        raise ValueError(f'conflicting existing {key}: {entry[key]}')


def main():
    if KIT.exists():
        raise SystemExit(f'preserving existing output directory: {KIT}')
    KIT.mkdir(parents=True)
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    editor.setdefault('decoMeta', {})
    editor.setdefault('decorationInfo', {})
    editor.setdefault('architectureOrder', [])

    for asset, title, source_name in ASSETS:
        source = SOURCE / source_name
        output_name = f'{asset}-v01.glb'
        output = KIT / output_name
        doc, tail = glb_document(source.read_bytes())
        for material in doc.get('materials', []):
            pbr = material.setdefault('pbrMetallicRoughness', {})
            if 'baseColorTexture' in pbr:
                pbr['baseColorFactor'] = COLOR
        output.write_bytes(encode_glb(doc, tail))

        width, height, triangles = metrics(output)
        mid = f'ember-bosque-morto-{asset}'
        file = f'/game/ember/bosque-morto-v01/{output_name}'
        model = {'id': mid, 'name': title, 'kit': 'bosque-morto-v01', 'model': asset,
                 'file': file, 'height': round(height, 3), 'width': round(width, 3),
                 'footprint': 'one', 'section': 'Natureza · Bosque Morto',
                 'blocks': True, 'triangles': triangles}
        append_or_check(models['models'], model, 'id')
        decoration = {'name': title, 'footprint': [{'dx': 0, 'dy': 0}], 'blockingFootprint': None,
                      'architectureSpan': False, 'architecture': False, 'model3d': 'prop',
                      'model': {'model': asset, 'file': file, 'height': round(height, 3)},
                      'mirrorAlternate': False, 'exitKind': None,
                      'size': {'w': round(width, 3), 'h': round(height, 3)}, 'light': None,
                      'standOn': False, 'lockYaw': False, 'nudge': 0}
        existing = catalog['decorations'].get(mid)
        if existing is None:
            catalog['decorations'][mid] = decoration
        elif existing != decoration:
            raise ValueError(f'conflicting catalog decoration: {mid}')
        meta = {'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False,
                'thickWall': False, 'doorStyle': None, 'wallTexture': None, 'solidHouse': False,
                'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True, 'art': ''}
        info = {'section': 'Natureza · Bosque Morto', 'tile': None, 'thumb': '', 'blocksByDefault': True}
        for key, entry in (('decoMeta', meta), ('decorationInfo', info)):
            existing = editor[key].get(mid)
            if existing is None:
                editor[key][mid] = entry
            elif existing != entry:
                raise ValueError(f'conflicting catalog {key}: {mid}')
        if mid not in editor['architectureOrder']:
            editor['architectureOrder'].append(mid)

    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temp = CATALOG.with_name(CATALOG.name + '.bosque-morto-v01.tmp')
    temp.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    os.replace(temp, CATALOG)
    print(f'[ok] created and registered {len(ASSETS)} dark-bark Bosque Morto tree models in {KIT}')


if __name__ == '__main__':
    main()
