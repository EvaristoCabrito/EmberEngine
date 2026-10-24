"""Add the new Farmlands GLBs to EmberEngine's and the map editor's model catalogues."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/engine2/editor-catalog.json'

ENTRIES = [
    {'id': 'ember-farmlands-tall-grass', 'name': 'Capim alto de pasto 3D', 'kit': 'farmlands',
     'model': 'tall-grass', 'file': '/game/ember/farmlands/tall-grass-v02.glb',
     'height': 0.884, 'width': 1.84, 'footprint': 'one', 'section': 'Natureza', 'blocks': False, 'triangles': 450},
    {'id': 'ember-farmlands-wheat-tile', 'name': 'Tile hexagonal de trigo 3D', 'kit': 'farmlands',
     'model': 'wheat-tile', 'file': '/game/ember/farmlands/wheat-tile-v03.glb',
     'height': 1.277, 'width': 1.84, 'footprint': 'one', 'section': 'Cultivos', 'blocks': False, 'triangles': 3030},
    {'id': 'ember-farmlands-hay-bales', 'name': 'Fardos de palha 3D', 'kit': 'farmlands',
     'model': 'hay-bales', 'file': '/game/ember/farmlands/hay-bales-v02.glb',
     'height': 1.33, 'width': 1.84, 'footprint': 'one', 'section': 'Objetos rurais', 'blocks': True, 'triangles': 704},
]


def main():
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    model_ids = {m['id'] for m in models['models']}
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    for entry in ENTRIES:
        mid = entry['id']
        if mid not in model_ids:
            models['models'].append(entry)
        else:
            index = next(i for i, existing in enumerate(models['models']) if existing['id'] == mid)
            models['models'][index] = entry
    for m in ENTRIES:
        mid = m['id']
        fp = [{'dx': 0, 'dy': 0}]
        catalog['decorations'][mid] = {
            'name': m['name'], 'footprint': fp, 'blockingFootprint': None, 'architectureSpan': False,
            'architecture': False, 'model3d': 'prop',
            'model': {'model': m['model'], 'file': m['file'], 'height': m['height']},
            'mirrorAlternate': False, 'exitKind': None, 'size': {'w': m['width'], 'h': m['height']},
            'light': None, 'standOn': False, 'lockYaw': False, 'nudge': 0,
        }
        editor['decoMeta'][mid] = {
            'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False,
            'doorStyle': None, 'wallTexture': None, 'solidHouse': False, 'solidCart': False,
            'solidRock': False, 'chest': False, 'manual': True, 'art': '',
        }
        editor['decorationInfo'][mid] = {
            'section': m['section'], 'tile': None, 'thumb': '', 'blocksByDefault': m['blocks'],
        }
        editor['architectureOrder'].append(mid)

    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(f'added {len(ENTRIES)} Farmlands model definitions without removing existing catalogue entries')


if __name__ == '__main__':
    main()
