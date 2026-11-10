"""Put the 3D stake barricade in the game in place of every 2D barricade.

- content/models/models.json and the editor catalogue gain `ember-defenses-stake-barricade` (section Barricada).
- The 2D barricade ids keep existing (maps place them and the engine drops `barricade` on every barricade tile), but
  each now carries the 3D model, so SceneBuilder places the mesh instead of the card. No map file changes.
Idempotent.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'

ENTRY = {'id': 'ember-defenses-stake-barricade', 'name': 'Barricada de estacas 3D', 'kit': 'defenses',
         'model': 'ember-stake-barricade', 'file': '/game/ember/defenses/stake-barricade-v01.glb',
         'height': 2.227, 'width': 1.8, 'footprint': 'one', 'section': 'Barricada', 'blocks': True, 'triangles': 8940}
FLAT = ('barricade', 'city-spike-barricade', 'city-spike-barricade-large', 'city-spike-barricade-low',
        'city-wooden-barricade', 'city-banner-barricade')


def main():
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    models['models'] = [m for m in models['models'] if m['id'] != ENTRY['id']] + [ENTRY]
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    model = {'model': ENTRY['model'], 'file': ENTRY['file'], 'height': ENTRY['height']}
    size = {'w': ENTRY['width'], 'h': ENTRY['height']}

    mid = ENTRY['id']
    catalog['decorations'][mid] = {
        'name': ENTRY['name'], 'footprint': [{'dx': 0, 'dy': 0}], 'blockingFootprint': None, 'architectureSpan': False,
        'architecture': False, 'model3d': 'prop', 'model': model, 'mirrorAlternate': False, 'exitKind': None,
        'size': size, 'light': None, 'standOn': False, 'lockYaw': False, 'nudge': 0,
    }
    editor['decoMeta'][mid] = {
        'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False, 'doorStyle': None,
        'wallTexture': None, 'solidHouse': False, 'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True,
        'art': '',
    }
    editor['decorationInfo'][mid] = {'section': ENTRY['section'], 'tile': None, 'thumb': '', 'blocksByDefault': True}
    if mid not in editor['architectureOrder']:
        editor['architectureOrder'].append(mid)

    changed = []
    for fid in FLAT:
        deco = catalog['decorations'].get(fid)
        if deco is None:
            continue
        deco['model3d'], deco['model'], deco['size'] = 'prop', dict(model), dict(size)
        meta = editor['decoMeta'].setdefault(fid, {})
        meta['model3d'], meta['props3d'], meta['art'] = 'prop', True, ''
        info = editor['decorationInfo'].setdefault(fid, {})
        info['thumb'] = ''
        # one 3D barricade in the editor's palette is enough: the old ids stay valid in maps, out of the lists
        editor['decorationOrder'] = [k for k in editor['decorationOrder'] if k != fid]
        changed.append(fid)

    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(f'{mid} registered; 2D barricades now 3D: {", ".join(changed)}')


if __name__ == '__main__':
    main()
