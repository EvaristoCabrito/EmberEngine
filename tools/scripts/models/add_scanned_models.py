"""The scanned models of 2026-10-30 (Smithsonian 3D, ambientCG) -> content/models/models.json, at real size.

  python -I tools/scripts/models/add_scanned_models.py

Each model folder (content/models/<source>/<name>/) holds <name>-2k-v01.glb, -1k-v01.glb and -512-v01.glb (the user: "2K / 1K and
512 nothing else"); the game cooks the 2K one (staged as ember/<source>/<name>-v01.glb) and takes 1K / 512 from its mips
(Opções > Gráficos > Texturas 3D). Size = the GLB's bounds in metres x 1.56 units per metre (a 1.70 m human = 2.65 units).
Replaces its own previous entries; then run catalog_models.py.
"""
import json
import os
import struct

UNITS_PER_METRE = 2.65 / 1.70
MODELS = {
    'smithsonian': {
        'cast-iron-cauldron': 'Caldeirão de ferro fundido 3D',
        'garden-fork': 'Forcado de jardim 3D',
        'bridge-ward-lock-key': 'Chave antiga 3D',
        'cuatro': 'Cuatro (viola) 3D',
        'gorilla-cranium': 'Crânio de gorila 3D',
        'orangutan-cranium': 'Crânio de orangotango 3D',
        'ruffed-lemur-cranium': 'Crânio de lêmure 3D',
        'kloss-gibbon-cranium': 'Crânio de gibão 3D',
        'homo-floresiensis-cranium': 'Crânio de Homo floresiensis 3D',
        'black-crested-gibbon-mandible': 'Mandíbula de gibão 3D',
        'chimpanzee-fibula-right': 'Fíbula de chimpanzé 3D',
        'chimpanzee-metatarsal5-right': 'Metatarso de chimpanzé 3D',
        'mountain-gorilla-ulna-right': 'Ulna de gorila 3D',
        'mountain-gorilla-metacarpal2-left': 'Metacarpo de gorila 3D',
    },
    'ambientcg': {
        'bread-006': 'Pão 3D',
        'apple-002': 'Maçã 3D',
        'stick-001': 'Graveto 3D',
    },
}


def node_matrix(node):
    if 'matrix' in node:
        m = node['matrix']
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    tx, ty, tz = node.get('translation', [0, 0, 0])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[i][0] * sx, r[i][1] * sy, r[i][2] * sz, (tx, ty, tz)[i]] for i in range(3)] + [[0, 0, 0, 1]]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def bounds(path):
    """The model's size in its file's units (glTF y up: [x, height, z]), every node's transform applied (the corners of each
    primitive's box), and its triangle count."""
    data = open(path, 'rb').read()
    n = struct.unpack('<I', data[12:16])[0]
    gltf = json.loads(data[20:20 + n])
    lo, hi = [1e9] * 3, [-1e9] * 3
    tris = 0

    def visit(index, parent):
        nonlocal tris
        node = gltf['nodes'][index]
        world = mul(parent, node_matrix(node))
        for prim in gltf['meshes'][node['mesh']]['primitives'] if 'mesh' in node else []:
            a = gltf['accessors'][prim['attributes']['POSITION']]
            tris += gltf['accessors'][prim['indices']]['count'] // 3 if 'indices' in prim else a['count'] // 3
            for corner in range(8):
                p = [(a['max'] if corner >> i & 1 else a['min'])[i] for i in range(3)] + [1]
                w = [sum(world[r][k] * p[k] for k in range(4)) for r in range(3)]
                for i in range(3):
                    lo[i], hi[i] = min(lo[i], w[i]), max(hi[i], w[i])
        for child in node.get('children', []):
            visit(child, world)

    identity = [[float(i == j) for j in range(4)] for i in range(4)]
    for root in gltf['scenes'][gltf.get('scene', 0)]['nodes']:
        visit(root, identity)
    size = [hi[i] - lo[i] for i in range(3)]
    # the museum's bone and skull scans are in millimetres (a gorilla's skull 324 long): nothing here is 10 m long
    if max(size) > 10.0:
        size = [s / 1000.0 for s in size]
    return size, tris


def main():
    path = 'content/models/models.json'
    with open(path, encoding='utf-8') as f:
        models = json.load(f)
    kits = set(MODELS)
    models['models'] = [m for m in models['models'] if m.get('kit') not in kits]
    for kit, names in MODELS.items():
        for name, label in names.items():
            glb = f'content/models/{kit}/{name}/{name}-2k-v01.glb'
            size, tris = bounds(glb)
            height = round(size[1] * UNITS_PER_METRE, 3)
            width = round(max(size[0], size[2]) * UNITS_PER_METRE, 3)
            models['models'].append({
                'id': f'ember-{kit}-{name}', 'name': label, 'kit': kit, 'model': f'{kit}-{name}-v01',
                'file': f'/game/ember/{kit}/{name}-v01.glb', 'height': height, 'width': width,
                'footprint': 'one' if width <= 1.9 else 'pair', 'section': 'Objetos', 'blocks': False, 'triangles': tris,
            })
            print(f'{kit}/{name}: {height} x {width} units, {tris} triangles')
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(models, f, ensure_ascii=False, indent=2)
        f.write('\n')


if __name__ == '__main__':
    main()
