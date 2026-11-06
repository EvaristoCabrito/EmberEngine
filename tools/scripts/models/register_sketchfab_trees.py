"""The user's Sketchfab trees (content/models/sketchfab/<pack>/, one GLB per element at 512 / 1k / 2k, elements.json beside them)
into content/models/models.json (the user, 2026-11-05: "tem milhões de árvores foda que eu adicionei, que não aparece lá", then
"Sim, coloca todas"). The 2k file is the source (the mesh pack keeps Alta 2K / Média 1K / Leve 512 from one file).

Size: the GLB's world bounds with its node transforms applied (the exports carry rotations and scales), so the engine's uniform
fit to `height` is 1:1. Trees stand on one hex (the trunk) and block it, like the Wisp Forest's trees.

  python -I tools/scripts/models/register_sketchfab_trees.py      then   python -I tools/scripts/models/catalog_models.py
Idempotent: every ember-sketchfab-* entry is rebuilt, every other entry is left as it is.
"""
import json
import math
import re
import struct
import sys

PACKS = {
    # pack folder: (id prefix, numbered)
    'pine-forest-at-sunset': ('ember-sketchfab-pine', True),
    'white-oak-trees': ('ember-sketchfab-white-oak', True),
    'swamp-shack': ('ember-sketchfab-swamp', True),
    'tree-trunk-scan': ('ember-sketchfab-tree-trunk-scan', False),
    'old-tree-stump': ('ember-sketchfab-old-tree-stump', False),
}
# Each element by its scene name: (Portuguese name, editor section), or None = left out. The packs are whole Sketchfab scenes: their
# terrain, water and grass sheets were exported as elements too and are not props.
KINDS = [
    (r'^(Terrain|Landscape|Water|GrassInstances|Foliage_DeadGrass|grass)', None),
    (r'^Grass', None),
    (r'^Ponderosa Pine seedling', ('Pinheiro jovem 3D', 'Natureza')),
    (r'^tree_copy', ('Pinheiro 3D', 'Natureza')),
    (r'^Log', ('Tronco caído 3D', 'Natureza')),
    (r'^Rock', ('Rocha 3D', 'Pedras e relevo')),
    (r'^Dead_Tree', ('Árvore morta do pântano 3D', 'Natureza')),
    (r'^FlorasynthTree', None),  # named by pack below
    (r'.*', ('?', 'Natureza')),
]
PACK_TREE = {'pine-forest-at-sunset': 'Pinheiro grande 3D', 'white-oak-trees': 'Carvalho branco 3D'}
SINGLE = {'tree-trunk-scan': 'Tronco escaneado 3D', 'old-tree-stump': 'Toco velho 3D'}
ROOT = 'content/models/sketchfab'
SECTION = 'Natureza'


def glb_doc(path):
    with open(path, 'rb') as f:
        f.read(12)
        n, _ = struct.unpack('<II', f.read(8))
        return json.loads(f.read(n))


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def node_matrix(n):
    if 'matrix' in n:
        m = n['matrix']  # column-major
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = n.get('translation', [0, 0, 0])
    x, y, z, w = n.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = n.get('scale', [1, 1, 1])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[0][0] * sx, r[0][1] * sy, r[0][2] * sz, tx],
            [r[1][0] * sx, r[1][1] * sy, r[1][2] * sz, ty],
            [r[2][0] * sx, r[2][1] * sy, r[2][2] * sz, tz],
            [0, 0, 0, 1]]


def world_bounds(path):
    d = glb_doc(path)
    nodes = d.get('nodes', [])
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get('children', []):
            parent[c] = i

    def world(i):
        m = node_matrix(nodes[i])
        while i in parent:
            i = parent[i]
            m = mat_mul(node_matrix(nodes[i]), m)
        return m

    lo, hi, tris = [math.inf] * 3, [-math.inf] * 3, 0
    for i, n in enumerate(nodes):
        if 'mesh' not in n:
            continue
        m = world(i)
        for prim in d['meshes'][n['mesh']]['primitives']:
            a = d['accessors'][prim['attributes']['POSITION']]
            if 'indices' in prim:
                tris += d['accessors'][prim['indices']]['count'] // 3
            for cx in (a['min'][0], a['max'][0]):
                for cy in (a['min'][1], a['max'][1]):
                    for cz in (a['min'][2], a['max'][2]):
                        p = [m[r][0] * cx + m[r][1] * cy + m[r][2] * cz + m[r][3] for r in range(3)]
                        lo = [min(u, v) for u, v in zip(lo, p)]
                        hi = [max(u, v) for u, v in zip(hi, p)]
    return lo, hi, tris


def main(models_path='content/models/models.json'):
    with open(models_path, encoding='utf-8') as f:
        models = json.load(f)
    entries = []
    left = []
    counters = {}
    for pack, (prefix, numbered) in PACKS.items():
        elements = json.load(open(f'{ROOT}/{pack}/elements.json', encoding='utf-8'))
        for e in elements:
            kind = None
            for pattern, k in KINDS:
                if re.search(pattern, e['element'], re.I):
                    kind = k if not pattern.startswith('^FlorasynthTree') else (PACK_TREE[pack], 'Natureza')
                    break
            if pack in SINGLE:
                kind = (SINGLE[pack], 'Natureza')
            if kind is None:
                left.append(f"{pack}/{e['element']}")
                continue
            name, section = kind
            file = e['file'].replace('{level}', '2k')
            stem = file[:-len('-2k-v01.glb')]
            slug = stem[len(pack) + 1:] if stem.startswith(pack + '-') else stem
            slug = re.sub(r'^florasynthtree-?', 'tree-', slug).strip('-') or 'tree'
            eid = f'{prefix}-{slug}' if numbered else prefix
            counters[name] = counters.get(name, 0) + 1
            lo, hi, tris = world_bounds(f'{ROOT}/{pack}/{file}')
            height = round(hi[1] - lo[1], 3)
            width = round(max(hi[0] - lo[0], hi[2] - lo[2]), 3)
            rock = section == 'Pedras e relevo'
            foot = ('one' if width <= 2.0 else 'pair' if width <= 4.2 else 'triple') if rock else 'one'
            entries.append({
                'id': eid, 'name': f'{name} · {counters[name]:02}' if numbered else name, 'kit': f'sketchfab/{pack}',
                'model': eid[len('ember-'):], 'file': f'/game/ember/sketchfab/{pack}/{file}',
                'height': height, 'width': width, 'footprint': foot, 'section': section, 'blocks': True,
                'triangles': tris, 'source': f'Sketchfab {pack}',
            })
    print('left out (scene ground / water / grass):', ', '.join(left))
    ids = {e['id'] for e in entries}
    assert len(ids) == len(entries), 'duplicate ids'
    assert not any(e['name'].startswith('?') for e in entries), [e['id'] for e in entries if e['name'].startswith('?')]
    models['models'] = [m for m in models['models'] if not m['id'].startswith('ember-sketchfab-')] + entries
    with open(models_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(models, f, ensure_ascii=False, indent=2)
        f.write('\n')
    for e in entries:
        print(f"{e['id']:55} h {e['height']:7.2f} w {e['width']:7.2f} tris {e['triangles']}")
    print(len(entries), 'trees registered')


if __name__ == '__main__':
    main(*sys.argv[1:])
