"""EmberEngine's own 3D props (content/models/models.json) merged into the Map Editor's catalog
(content/engine2/editor-catalog.json), as decorations of the kind export_scene.ts calls a model: the scene builder
places them (SceneBuilder: `model`, centroid of the footprint, uniform fit to `height`) and the editor's 3D palette
lists them (decoMeta.props3d, architectureOrder). Idempotent: every `ember-*` entry is rebuilt from models.json;
Engine2's own entries are untouched. Run it again after export_editor_catalog.ts regenerates the catalog.

  python -I tools/scripts/models/catalog_models.py [content/models/models.json] [content/engine2/editor-catalog.json]
"""
import json
import sys

FOOTPRINTS = {
    'one': [{'dx': 0, 'dy': 0}],
    'pair': [{'dx': 0, 'dy': 0}, {'dx': 1, 'dy': 0}],
    'triple': [{'dx': -1, 'dy': 0}, {'dx': 0, 'dy': 0}, {'dx': 1, 'dy': 0}],
    'quad': [{'dx': -1, 'dy': 0}, {'dx': 0, 'dy': 0}, {'dx': 1, 'dy': 0}, {'dx': 2, 'dy': 0}],
}


def main(models_path='content/models/models.json', catalog_path='content/engine2/editor-catalog.json',
         architecture_path='content/models/architecture.json'):
    with open(models_path, encoding='utf-8') as f:
        models = json.load(f)
    with open(catalog_path, encoding='utf-8') as f:
        catalog = json.load(f)
    editor = catalog['editor']
    editor.setdefault('decoMeta', {})
    editor.setdefault('architectureOrder', [])
    editor.setdefault('architectureDefs', {})
    # drop the previous run's entries
    for table in (catalog['decorations'], editor['decoMeta'], editor['decorationInfo'], editor['architectureDefs']):
        for k in [k for k in table if k.startswith('ember-')]:
            del table[k]
    editor['architectureOrder'] = [k for k in editor['architectureOrder'] if not k.startswith('ember-')]
    # EmberEngine's own architecture (content/models/architecture.json): walls, windows, doorways built by world/Architecture.cpp,
    # placed like Engine2's wall pieces (architecture: true), listed first in the editor's 3D Walls palette.
    with open(architecture_path, encoding='utf-8') as f:
        architecture = json.load(f)
    for p in architecture['pieces']:
        pid = p['id']
        if not pid.startswith('ember-arch-'):
            raise SystemExit(f'{pid}: EmberEngine architecture ids start with ember-arch-')
        span = p.get('architectureSpan')
        footprint = FOOTPRINTS[p.get('footprint', 'one')]
        catalog['decorations'][pid] = {
            'name': p['name'], 'footprint': footprint, 'blockingFootprint': [] if p['model3d'] == 'doorway' else None,
            'architectureSpan': bool(span), 'architecture': True, 'model3d': p['model3d'], 'model': None,
            'mirrorAlternate': False, 'exitKind': None, 'size': {'w': 1.86 * len(footprint), 'h': 1.98}, 'light': None, 'standOn': False,
            'lockYaw': False, 'nudge': 0,
        }
        editor['decoMeta'][pid] = {'model3d': p['model3d'], 'exitKind': None, 'props3d': False, 'rockStyle': False, 'thickWall': False,
                                   'doorStyle': p.get('doorStyle'), 'wallTexture': p['wallTexture'], 'solidHouse': False, 'solidCart': False,
                                   'solidRock': False, 'chest': False, 'manual': True, 'art': ''}
        editor['decorationInfo'][pid] = {'section': 'Ruínas e construções', 'tile': None, 'thumb': '', 'blocksByDefault': p['model3d'] != 'doorway'}
        editor['architectureDefs'][pid] = {
            'model3d': p['model3d'], 'heightScale': p.get('heightScale', 1), 'wallThicknessScale': p.get('wallThicknessScale', 1),
            'architectureSpan': span, 'doorStyle': None, 'rockStyle': None, 'templeStyle': None, 'castleStyle': None, 'thickWall': False,
            'dungeonReference': False, 'wallTexture': p['wallTexture'], 'connects': p.get('connects', True),
            'emberStyle': p['emberStyle'], 'textureRepeat': p.get('textureRepeat', 3.6),
        }
    editor['architectureOrder'] = [p['id'] for p in architecture['pieces']] + editor['architectureOrder']
    # the kit's photographed materials (world/Architecture.h emberMaterialName -> texture URL)
    editor['architectureMaterials'] = architecture.get('materials', {})
    # the editor's Abrir porta / Fechar passagem pairs (decoMeta.doorStyle -> doorVariants[style])
    editor.setdefault('doorVariants', {})
    for style in [s for s, v in editor['doorVariants'].items() if str(v.get('open', '')).startswith('ember-')]:
        del editor['doorVariants'][style]
    editor['doorVariants'].update(architecture.get('doorVariants', {}))
    for m in models['models']:
        mid = m['id']
        if not mid.startswith('ember-'):
            raise SystemExit(f'{mid}: EmberEngine ids start with ember-')
        catalog['decorations'][mid] = {
            'name': m['name'], 'footprint': FOOTPRINTS[m.get('footprint', 'one')], 'blockingFootprint': None, 'architectureSpan': False,
            'architecture': False, 'model3d': 'prop',
            'model': {'model': m['model'], 'file': m['file'], 'height': m['height']},
            'mirrorAlternate': False, 'exitKind': None, 'size': {'w': m['width'], 'h': m['height']}, 'light': None, 'standOn': False,
            'lockYaw': False, 'nudge': 0,
        }
        editor['decoMeta'][mid] = {'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False, 'doorStyle': None,
                                   'wallTexture': None, 'solidHouse': False, 'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True,
                                   'art': ''}
        editor['decorationInfo'][mid] = {'section': m.get('section', 'Objetos'), 'tile': None, 'thumb': '', 'blocksByDefault': bool(m.get('blocks', True))}
        editor['architectureOrder'].append(mid)
    with open(catalog_path, 'w', encoding='utf-8') as f:
        json.dump(catalog, f, ensure_ascii=False, separators=(',', ':'))
    print(f'{len(models["models"])} EmberEngine models + {len(architecture["pieces"])} architecture pieces in {catalog_path} '
          f'({len(catalog["decorations"])} decorations)')


if __name__ == '__main__':
    main(*sys.argv[1:])
