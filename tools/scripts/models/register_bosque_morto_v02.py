"""The Bosque Morto's trees remade (the user, 2026-11-10: "refazer e apagar esse lixo"): the five trees of
make_dead_trees_v02.py go into the catalog and models.json, the old bosque-morto-v01 kit comes out of both, and the Bosque
Morto map gets a new version with every tree swapped for a new one (nothing else in the map changes).

  python -I tools/scripts/models/register_bosque_morto_v02.py
"""
import json
import os
import struct
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
KIT = ROOT / 'content/models/bosque-morto-v02'
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
MAPS = ROOT / 'content/maps/ember'
SECTION = 'Natureza · Bosque Morto'
NEW = [
    ('old-oak', 'Carvalho morto 3D · Bosque Morto'),
    ('tall-ash', 'Freixo morto alto 3D · Bosque Morto'),
    ('forked-elm', 'Olmo morto bifurcado 3D · Bosque Morto'),
    ('broken-snag', 'Tronco morto quebrado 3D · Bosque Morto'),
    ('wide-beech', 'Faia morta larga 3D · Bosque Morto'),
]
OLD = ['ember-bosque-morto-giant-dead-oak', 'ember-bosque-morto-hollow-snag', 'ember-bosque-morto-lightning-split',
       'ember-bosque-morto-weeping-willow', 'ember-bosque-morto-uprooted-leaning']
# every tree standing on the Bosque Morto map -> the new tree that takes its hex
SWAP = {
    'ember-bosque-morto-giant-dead-oak': 'old-oak',
    'ember-bosque-morto-hollow-snag': 'broken-snag',
    'ember-bosque-morto-lightning-split': 'forked-elm',
    'ember-bosque-morto-weeping-willow': 'tall-ash',
    'ember-bosque-morto-uprooted-leaning': 'wide-beech',
    'ember-wisp-revisited-lightning-split-v02': 'tall-ash',
    'ember-wisp-revisited-hollow-snag-v02': 'broken-snag',
    'ember-wisp-revisited-giant-dead-oak-v02': 'old-oak',
}


def new_id(asset):
    return f'ember-bosque-morto-v02-{asset}'


def metrics(path):
    data = path.read_bytes()
    size = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + size].decode('utf-8'))
    acc = doc['accessors']
    pos = [acc[p['attributes']['POSITION']] for m in doc['meshes'] for p in m['primitives']]
    dims = [max(a['max'][i] for a in pos) - min(a['min'][i] for a in pos) for i in range(3)]
    tris = sum(acc[p['indices']]['count'] // 3 for m in doc['meshes'] for p in m['primitives'])
    return round(max(dims[0], dims[2]), 3), round(dims[1], 3), tris


def scrub(v, gone):
    if isinstance(v, dict):
        return {k: scrub(x, gone) for k, x in v.items() if k not in gone}
    if isinstance(v, list):
        return [scrub(x, gone) for x in v if not (isinstance(x, str) and x in gone)]
    return v


def main():
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    gone = set(OLD)
    models['models'] = [m for m in models['models'] if m['id'] not in gone]
    catalog = scrub(catalog, gone)
    editor = catalog['editor']
    for asset, title in NEW:
        mid = new_id(asset)
        width, height, triangles = metrics(KIT / f'{asset}-v01.glb')
        file = f'/game/ember/bosque-morto-v02/{asset}-v01.glb'
        key = f'bosque-morto-v02-{asset}'
        models['models'] = [m for m in models['models'] if m['id'] != mid]
        models['models'].append({'id': mid, 'name': title, 'kit': 'bosque-morto-v02', 'model': key, 'file': file,
                                 'height': height, 'width': width, 'footprint': 'one', 'section': SECTION, 'blocks': True,
                                 'triangles': triangles})
        catalog['decorations'][mid] = {
            'name': title, 'footprint': [{'dx': 0, 'dy': 0}], 'blockingFootprint': None, 'architectureSpan': False,
            'architecture': False, 'model3d': 'prop', 'model': {'model': key, 'file': file, 'height': height},
            'mirrorAlternate': False, 'exitKind': None, 'size': {'w': width, 'h': height}, 'light': None, 'standOn': False,
            'lockYaw': False, 'nudge': 0}
        editor['decoMeta'][mid] = {
            'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False, 'doorStyle': None,
            'wallTexture': None, 'solidHouse': False, 'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True,
            'art': ''}
        editor['decorationInfo'][mid] = {'section': SECTION, 'tile': None, 'thumb': '', 'blocksByDefault': True}
        if mid not in editor['architectureOrder']:
            editor['architectureOrder'].append(mid)
        print(f'[ok] {mid}: {width} wide, {height} tall, {triangles} triangles')
    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temp = CATALOG.with_name(CATALOG.name + '.tmp')
    temp.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    os.replace(temp, CATALOG)

    # the map: its newest version, copied to the next number with only the trees' ids changed
    def number(p):
        digits = p.stem[len('bosque'):]
        return int(digits) if digits.isdigit() else -1
    latest = max(MAPS.glob('bosque[0-9]*.json'), key=number)
    target = MAPS / f'bosque{number(latest) + 1:03d}.json'
    if target.exists():
        raise SystemExit(f'preserving existing map: {target}')
    game_map = json.loads(latest.read_text(encoding='utf-8'))
    swapped = 0
    for deco in game_map['draft']['decorations']:
        if deco.get('id') in SWAP:
            deco['id'] = new_id(SWAP[deco['id']])
            swapped += 1
    game_map['serial'] = number(latest) + 1
    game_map['savedAt'] = int(time.time() * 1000)
    target.write_text(json.dumps(game_map, ensure_ascii=False), encoding='utf-8')
    print(f'[ok] {target.name}: {swapped} trees swapped (from {latest.name})')


if __name__ == '__main__':
    main()
