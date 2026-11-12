"""Put the fire kit v01 (create_fire_kit_v01.py) in the game and give the 2D fire props their 3D model.

  python -I tools/scripts/models/register_fire_kit_v01.py

- content/models/models.json and the editor catalogue gain `ember-fire-<name>` (section Fogo 3D), with the measures the build wrote
  in build/models-tmp/fire/manifest.json.
- The 2D fire props keep their ids (the maps place them), but each now carries a 3D model and its flames ("fire": the points where
  the engine draws a volume of fire, in the model's own space, y up), so SceneBuilder places the mesh instead of the card. Their
  light keeps its strength, reach and flicker; it sits at the fire. No map file changes.
- The Meshy hearth gets its three tongues of flame the same way.
The catalogue as it was is kept beside it once (editor-catalog-20261112-before-fire-kit.json). Idempotent.
"""
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODELS = ROOT / 'content/models/models.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
BACKUP = ROOT / 'content/data/editor-catalog-20261112-before-fire-kit.json'
MANIFEST = ROOT / 'build/models-tmp/fire/manifest.json'
SECTION = 'Fogo 3D'
FLAME = [1, 0.7, 0.36]  # the one colour of every torch, brazier, lamp and candle (MapStage kFlameLightColor)

# 2D prop id -> the model that takes its place
FIRE_KIT = {
    'candle': ('light-candle', 'cave-candle-stand'),
    'torch-stand': ('light-wall-torch',),
    'brazier-tripod': ('wilds-brazier-tripod', 'torture-brazier'),
    'campfire': ('city-campfire', 'cave-basalt-004-campfire', 'cave-basalt-006-campfire'),
    # the second batch (create_fire_kit_v01_scenes.py): every lantern an open cage round its candle
    'lantern-pillar': ('wilds-cemetery-lantern',),
    'street-lamp': ('lamppost', 'city-lantern-post', 'ember-city-street-lamp-v02'),
    'lantern-post-wood': ('wilds-lantern-post',),
    'lantern-post-low': ('ember-farmhouse-oil-lantern',),
    'lantern-signpost': ('wilds-lantern-signpost',),
    'candle-menhir': ('wilds-candle-menhir',),
    'incense-burner': ('wilds-incense-burner',),
    'candle-shrine': ('city-shrineCandle',),
    'campfire-cauldron': ('wilds-campfire-cauldron',),
    'hand-lantern': ('cave-rusty-lantern',),
    'ice-lantern': ('ice-lantern-cage',),
    'chandelier': ('Chandelier',),
    'candle-sconce': ('city-inn-candle-sconce',),
    'forge': ('farmland-photographic-013-forge',),
    'smithy': ('city-forge',),
}
# ... models the game already had
SIGNAL_BRAZIER = {'model': 'watchtower-signal-brazier-v02', 'file': '/game/ember/watchtower-v02/watchtower-signal-brazier-v02.glb', 'height': 1.52,
                  'width': 1.12, 'fire': [{'at': [0.0, 1.3, 0.0], 'height': 0.85, 'radius': 0.3, 'turbulence': 1.0}]}
SIGNAL_BRAZIER_IDS = ('light-brazier-bowl', 'city-brazier', 'Brazier3', 'cave-basalt-004-brazier', 'cave-basalt-005-brazier', 'cave-basalt-006-brazier',
                      'cave-wall-brazier')
# the 2D fireplaces: the Meshy hearth, at their size
FIREPLACES = ('inn-fireplace', 'light-fireplace')
FIREPLACE_HEIGHT = 3.0
# 2D cards that keep their picture: where each painted flame stands on it (u across, v up, 0..1 of the image; measured from the
# pictures' own flame pixels), its height and radius in world units
CARD_FIRE = {
    'burning-house': [{'uv': [0.56, 0.56], 'height': 2.3, 'radius': 0.7, 'turbulence': 1.0}, {'uv': [0.497, 0.244], 'height': 0.7, 'radius': 0.22, 'turbulence': 1.0}],
    'burnt-house-ruins': [{'uv': [0.83, 0.388], 'height': 1.0, 'radius': 0.28, 'turbulence': 1.0}],
    'burning-hamlet': [{'uv': [0.81, 0.471], 'height': 1.7, 'radius': 0.45, 'turbulence': 1.0}, {'uv': [0.2, 0.556], 'height': 1.6, 'radius': 0.45, 'turbulence': 1.0}],
}
HEARTH = 'ember-tavern-stone-hearth'
HEARTH_FIRE = [{'at': [-0.27, 0.25, 0.05], 'height': 0.62, 'radius': 0.2, 'turbulence': 1.0},
               {'at': [0.0, 0.25, 0.0], 'height': 0.86, 'radius': 0.25, 'turbulence': 1.0},
               {'at': [0.28, 0.25, 0.08], 'height': 0.66, 'radius': 0.2, 'turbulence': 1.0}]


def engine_fire(m):
    """Blender's (x, y, z up) -> the model's space in the game (glTF: x, z up -> y, -y -> z), about the model's own bounds."""
    return [{'at': [round(f['at'][0], 4), round(f['at'][2] - m['base'], 4), round(-f['at'][1], 4)], 'height': f['height'], 'radius': f['radius'],
             'turbulence': f['turbulence']} for f in m['fire']]


def light_at(light, fire, centre=(0.0, 0.0)):
    """The prop's own light, moved to its first flame: a third of the way up it, over the model's middle."""
    first = fire[0]
    out = dict(light)
    out['color'] = FLAME
    out['minHeight'] = round(first['at'][1] + first['height'] * 0.35, 3)
    out['offset'] = [round(first['at'][0] - centre[0], 3), round(first['at'][2] - centre[1], 3)]
    return out


def main():
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    models = json.loads(MODELS.read_text(encoding='utf-8'))
    if not BACKUP.exists():
        shutil.copyfile(CATALOG, BACKUP)
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor, decorations = catalog['editor'], catalog['decorations']
    by_id = {m['id']: m for m in models['models']}

    def as_model(fid, model, size, fire, light):
        deco = decorations.get(fid)
        if deco is None:
            return False
        deco['model3d'], deco['model'], deco['size'] = 'prop', dict(model), dict(size)
        deco['nudge'], deco['lockYaw'] = 0, False
        if fire:
            deco['fire'] = fire
        if light:
            deco['light'] = light
        meta = editor['decoMeta'].setdefault(fid, {})
        meta['model3d'], meta['props3d'], meta['art'] = 'prop', True, ''
        return True

    changed = []
    for key, m in manifest.items():
        mid = f'ember-fire-{key}'
        fire = engine_fire(m)
        # the model's middle in the game's space: the instance is centred on its bounds, the light is given from the anchor
        centre = (m['centre'][0], -m['centre'][1])
        entry = {'id': mid, 'name': m['name'], 'kit': 'fire', 'model': mid, 'file': f"/game/ember/fire/{m['file']}", 'height': m['height'],
                 'width': m['width'], 'footprint': 'one', 'section': SECTION, 'blocks': False, 'triangles': m['triangles']}
        models['models'] = [x for x in models['models'] if x['id'] != mid] + [entry]
        model = {'model': entry['model'], 'file': entry['file'], 'height': entry['height']}
        size = {'w': entry['width'], 'h': entry['height']}
        first = decorations.get(FIRE_KIT.get(key, ('',))[0], {}).get('light') or {'color': FLAME, 'intensity': 6, 'radius': 4, 'flicker': 1, 'minHeight': 1}
        decorations[mid] = {
            'name': entry['name'], 'footprint': [{'dx': 0, 'dy': 0}], 'blockingFootprint': None, 'architectureSpan': False, 'architecture': False,
            'model3d': 'prop', 'model': model, 'mirrorAlternate': False, 'exitKind': None, 'size': size, 'light': light_at(first, fire, centre),
            'standOn': False, 'lockYaw': False, 'nudge': 0, 'fire': fire,
        }
        editor['decoMeta'][mid] = {
            'model3d': 'prop', 'exitKind': None, 'props3d': True, 'rockStyle': False, 'thickWall': False, 'doorStyle': None, 'wallTexture': None,
            'solidHouse': False, 'solidCart': False, 'solidRock': False, 'chest': False, 'manual': True, 'art': '',
        }
        editor['decorationInfo'][mid] = {'section': SECTION, 'tile': None, 'thumb': '', 'blocksByDefault': False}
        if mid not in editor['architectureOrder']:
            editor['architectureOrder'].append(mid)
        for fid in FIRE_KIT.get(key, ()):
            own = decorations.get(fid, {}).get('light')
            if as_model(fid, model, size, fire, light_at(own, fire, centre) if own else None):
                changed.append(f'{fid} -> {mid}')

    brazier = {k: SIGNAL_BRAZIER[k] for k in ('model', 'file', 'height')}
    for fid in SIGNAL_BRAZIER_IDS:
        own = decorations.get(fid, {}).get('light')
        if as_model(fid, brazier, {'w': SIGNAL_BRAZIER['width'], 'h': SIGNAL_BRAZIER['height']}, SIGNAL_BRAZIER['fire'],
                    light_at(own, SIGNAL_BRAZIER['fire']) if own else None):
            changed.append(f'{fid} -> signal brazier v02')

    hearth = by_id.get(HEARTH)
    for fid in FIREPLACES:
        if hearth is None:
            break
        k = FIREPLACE_HEIGHT / hearth['height']
        light = dict(decorations.get(fid, {}).get('light') or hearth['light'])
        light['minHeight'] = round(hearth['light']['minHeight'] * k, 3)
        if as_model(fid, {'model': hearth['model'], 'file': hearth['file'], 'height': FIREPLACE_HEIGHT},
                    {'w': round(hearth['width'] * k, 3), 'h': FIREPLACE_HEIGHT}, HEARTH_FIRE, light):
            changed.append(f'{fid} -> {HEARTH}')

    # the burning houses stay 2D cards (the user, 2026-11-12): a volume of fire over each painted one, its foot on the picture
    for fid, fires in CARD_FIRE.items():
        if fid in decorations:
            decorations[fid]['fire'] = fires
            changed.append(f'{fid}: {len(fires)} flames over its card')

    if HEARTH in decorations:
        decorations[HEARTH]['fire'] = HEARTH_FIRE
        changed.append(f'{HEARTH}: three tongues of flame')

    MODELS.write_text(json.dumps(models, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print('\n'.join(changed))


if __name__ == '__main__':
    main()
