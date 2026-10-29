"""Catalogue whole kits from their GLBs: for every content/models/<kit>/<name>-vNN.glb named in content/models/names.json
(the Portuguese names + overrides), read the glTF bounds (accessor min/max of every POSITION; the exports carry identity
node transforms), and write / replace the kit's entries in content/models/models.json (ids ember-<kit>-<name>, authored
height / width, footprint from the width: one <= 2.0, pair <= 4.2, else triple). Then run catalog_models.py.

  python -I tools/scripts/models/add_kits_to_catalog.py <kit> [<kit> ...]
"""
import glob
import json
import os
import struct
import sys

ROOT = 'content/models'


def glb_bounds(path):
    with open(path, 'rb') as f:
        magic, version, length = struct.unpack('<III', f.read(12))
        assert magic == 0x46546C67, path
        chunk_len, chunk_type = struct.unpack('<II', f.read(8))
        doc = json.loads(f.read(chunk_len).decode('utf-8'))
    lo = [1e9, 1e9, 1e9]
    hi = [-1e9, -1e9, -1e9]
    for mesh in doc.get('meshes', []):
        for prim in mesh.get('primitives', []):
            acc = doc['accessors'][prim['attributes']['POSITION']]
            for i in range(3):
                lo[i] = min(lo[i], acc['min'][i])
                hi[i] = max(hi[i], acc['max'][i])
    tris = 0
    for mesh in doc.get('meshes', []):
        for prim in mesh.get('primitives', []):
            if 'indices' in prim:
                tris += doc['accessors'][prim['indices']]['count'] // 3
    return lo, hi, tris


def main(*kits):
    with open(f'{ROOT}/names.json', encoding='utf-8') as f:
        names = json.load(f)
    with open(f'{ROOT}/models.json', encoding='utf-8') as f:
        models = json.load(f)
    for kit in kits:
        table = names[kit]
        section = table.pop('_section', 'Objetos') if '_section' in table else 'Objetos'
        models['models'] = [m for m in models['models'] if not m['id'].startswith(f'ember-{kit}-')]
        added = 0
        for glb in sorted(glob.glob(f'{ROOT}/{kit}/*.glb')):
            stem = os.path.basename(glb)[:-4]
            name, _, version = stem.rpartition('-v')
            if name not in table:
                print(f'  {kit}/{stem}: not in names.json, skipped')
                continue
            entry = table[name]
            pt, over = (entry, {}) if isinstance(entry, str) else (entry[0], entry[1] if len(entry) > 1 else {})
            lo, hi, tris = glb_bounds(glb)
            height = round(hi[1] - lo[1], 3)
            width = round(max(hi[0] - lo[0], hi[2] - lo[2]), 3)
            foot = over.get('footprint', 'one' if width <= 2.0 else 'pair' if width <= 4.2 else 'triple')
            models['models'].append({
                'id': f'ember-{kit}-{name}', 'name': pt, 'kit': kit, 'model': f'ember-{name}', 'file': f'/game/ember/{kit}/{stem}.glb',
                'height': height, 'width': width, 'footprint': foot, 'section': over.get('section', section),
                'blocks': over.get('blocks', height >= 0.5), 'triangles': tris,
                **({'light': over['light']} if 'light' in over else {}),  # a prop with its own light (models.json "light", like the hearth)
            })
            added += 1
        print(f'{kit}: {added} models catalogued')
        table['_section'] = section
    with open(f'{ROOT}/models.json', 'w', encoding='utf-8') as f:
        json.dump(models, f, ensure_ascii=False, indent=2)
    print(f'{len(models["models"])} models in {ROOT}/models.json')


if __name__ == '__main__':
    main(*sys.argv[1:])
