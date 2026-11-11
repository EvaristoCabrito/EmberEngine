"""Put the Farmlands houses' kit v01 (create_farmhouse_kit_v01.py) in the game's lists.

  python -I tools/scripts/models/register_farmhouse_kit_v01.py

content/models/models.json and content/data/editor-catalog.json gain `ember-farmhouse-<name>` (section Fazenda), with the measures
the build wrote in build/models-tmp/farmhouse/manifest.json. Only these ids are touched. Idempotent.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
MANIFEST = ROOT / 'build/models-tmp/farmhouse/manifest.json'
FOOTPRINTS = {'one': [{'dx': 0, 'dy': 0}], 'pair': [{'dx': 0, 'dy': 0}, {'dx': 1, 'dy': 0}]}
SECTION = 'Fazenda'


def main():
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    for key, m in manifest.items():
        mid = f'ember-farmhouse-{key}'
        entry = {'id': mid, 'name': m['name'], 'kit': 'farmhouse', 'model': f'ember-farmhouse-{key}',
                 'file': f"/game/ember/farmhouse/{m['file']}", 'height': m['height'], 'width': m['width'], 'footprint': m['footprint'],
                 'section': SECTION, 'blocks': m['blocks'], 'triangles': m['triangles']}
        if m.get('light'):
            entry['light'] = m['light']
        models['models'] = [x for x in models['models'] if x['id'] != mid] + [entry]
        catalog['decorations'][mid] = {
            'name': entry['name'], 'footprint': FOOTPRINTS[m['footprint']], 'blockingFootprint': None, 'architectureSpan': False,
            'architecture': False, 'model3d': 'prop', 'model': {'model': entry['model'], 'file': entry['file'], 'height': entry['height']},
            'mirrorAlternate': False, 'exitKind': None, 'size': {'w': entry['width'], 'h': entry['height']}, 'light': m.get('light'),
            'standOn': False, 'lockYaw': False, 'nudge': 0,
        }
        editor['decoMeta'][mid] = {
            'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False, 'doorStyle': None,
            'wallTexture': None, 'solidHouse': False, 'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True, 'art': '',
        }
        editor['decorationInfo'][mid] = {'section': SECTION, 'tile': None, 'thumb': '', 'blocksByDefault': m['blocks']}
        if mid not in editor['architectureOrder']:
            editor['architectureOrder'].append(mid)
        print('registered', mid, entry['height'], entry['width'], m['footprint'])
    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')


if __name__ == '__main__':
    main()
