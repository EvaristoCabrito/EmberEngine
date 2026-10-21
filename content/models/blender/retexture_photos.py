"""The user's own material photographs (Attachments/watchtower, 2026-10-21) on existing props, as NEW versions (the old ones stay as
they were, and every map that uses them keeps them): the same builders, the materials named in the plan swapped for the photos
(content/models/textures/wt-*.jpg, square crops). Two sets:

  torre      the Torre de Vigia's props: dark oak (madeiraescura), pale pine (madeiraclara), iron (metal), burlap (serrarilepia),
             the pillars (pilares)                                               -> <name>-v02
  cemiterio  the cemetery's tombs: headstones (gravestone), tombs and sarcophagi (limestone)   -> the next free version

  blender -b --python content/models/blender/retexture_photos.py [-- name ...]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emberlib
from emberlib import *  # noqa
import kit_props, kit_treasure, kit_interior, kit_tavern, kit_dungeon, kit_crypt, kit_extra

WOOD_DARK = dict(color='wt-dark-oak', roughness=0.78)
WOOD_PALE = dict(color='wt-pine', roughness=0.82)
IRON = dict(color='wt-iron', roughness=0.62, metallic=0.35)
BURLAP = dict(color='wt-burlap', roughness=1.0)
TORRE = {'planks-dark': WOOD_DARK, 'planks-iron': WOOD_DARK, 'planks': WOOD_PALE, 'planks-worn': WOOD_PALE,
         'iron': IRON, 'iron-rust': IRON, 'burlap': BURLAP, 'masonry-crypt': dict(color='wt-pillar', roughness=0.9)}
HEADSTONE = {'masonry-crypt': dict(color='wt-gravestone', roughness=0.9), 'masonry-mossy': dict(color='wt-gravestone', roughness=0.9)}
TOMB = {'masonry-crypt': dict(color='wt-limestone', roughness=0.9), 'masonry-mossy': dict(color='wt-limestone', roughness=0.9)}

# (kit folder, asset name, builder, material overrides)
PLAN = [
    ('props', 'barrel', kit_props.barrel, TORRE),
    ('props', 'crate', kit_props.crate, TORRE),
    ('props', 'sacks', kit_props.sacks, TORRE),
    ('treasure', 'weapon-rack', kit_treasure.weapon_rack, TORRE),
    ('treasure', 'shield-stand', kit_treasure.shield_stand, TORRE),
    ('treasure', 'workbench', kit_treasure.workbench, TORRE),
    ('interior', 'long-bench', kit_interior.long_bench, TORRE),
    ('interior', 'barrel-rack', kit_interior.barrel_rack, TORRE),
    ('interior', 'simple-bed', kit_interior.simple_bed, TORRE),
    ('tavern', 'table', kit_tavern.table, TORRE),
    ('tavern', 'chair', kit_tavern.chair, TORRE),
    ('dungeon', 'pillar', kit_dungeon.pillar, TORRE),
    ('crypt', 'ancient-tombstone', kit_crypt.ancient_tombstone, HEADSTONE),
    ('extra', 'grave-marker', lambda: kit_extra.grave_marker(mound=False), HEADSTONE),
    ('crypt', 'stone-tomb', kit_crypt.stone_tomb, TOMB),
    ('crypt', 'sarcophagus', kit_crypt.sarcophagus, TOMB),
    ('crypt', 'sarcophagus-broken-lid', kit_crypt.sarcophagus_broken_lid, TOMB),
]

_material = emberlib.material
_overrides = {}


def material(key, **kw):
    """emberlib.M's materials, with this asset's photographs in place of the named ones (a new key: nothing cached is reused)."""
    if key in _overrides:
        o = _overrides[key]
        return _material(key + '-photo', color=o['color'], roughness=o.get('roughness', 0.85), metallic=o.get('metallic', 0.0))
    return _material(key, **kw)


emberlib.material = material


def next_version(kit, name):
    v = 2
    while os.path.exists(f'{ROOT}/{kit}/{name}-v{v:02d}.glb'):
        v += 1
    return f'v{v:02d}'


argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
reports, errors = [], []
for kit, name, build, overrides in PLAN:
    if argv and name not in argv:
        continue
    try:
        reset()
        _overrides.clear()
        _overrides.update(overrides)
        version = next_version(kit, name)
        r = export(build(), kit, name, version=version)
        r['version'] = version
        reports.append(r)
        print(f'[ok] {kit}/{name}-{version}: {r["triangles"]} tris, {r["size"]}')
    except Exception as e:
        import traceback
        traceback.print_exc()
        errors.append({'name': name, 'error': str(e)})
print('JSON_BEGIN' + json.dumps({'assets': reports, 'errors': errors}) + 'JSON_END')
