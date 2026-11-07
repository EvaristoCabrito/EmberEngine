"""Give a model's parts a real tiling photograph, as a NEW version (the user, 2026-11-06: "eu não quero nenhum objeto sem textura ...
se não tem textura, se não tem onde colocar, inventa"; the dead trees of O Vau / the Ashen Forest read as plain brown plastic: their
baked texture is a uniform grain with no bark in it).

The chosen parts (all, or --materials a,b) get:
  - the colour, the OpenGL normal and the ARM (AO / roughness / metal) of a photo set: a Poly Haven id (content/textures/polyhaven/<id>/
    <id>_{diff,nor_gl,arm}_2k.jpg) or an ambientCG one (content/textures/ambientcg/<id>/<id>_{color,normalgl,roughness}_2k.jpg);
  - new UVs by a box projection per triangle (the axis its face looks along; the sides keep the photo upright, so bark grain runs up
    the trunk), the photo repeating every --tile model units, or every --tile-frac of the model's height;
  - factor white (metal 0 unless --metal), vertex colour and old tangents dropped (the cooker makes tangents from the new UVs).
The other parts, the images they use and everything else are kept; the binary chunk is rebuilt without what nothing uses any more.

  python -I tools/scripts/models/retexture_box.py <in.glb> <out.glb> --set <id> (--tile T | --tile-frac F) [--materials a,b] [--metal] [--tint r,g,b]

--tint: the factor over the photo (a dark cast iron from a bright metal photo).
"""
import io
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
CT = {5126: ('<f4', 4), 5121: ('u1', 1), 5123: ('<u2', 2), 5125: ('<u4', 4)}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


def photo_set(sid):
    ph = ROOT / 'content/textures/polyhaven' / sid
    if ph.exists():
        return {'diff': ph / f'{sid}_diff_2k.jpg', 'nor': ph / f'{sid}_nor_gl_2k.jpg', 'arm': ph / f'{sid}_arm_2k.jpg'}
    ac = ROOT / 'content/textures/ambientcg' / sid
    if ac.exists():
        return {'diff': ac / f'{sid}_color_2k.jpg', 'nor': ac / f'{sid}_normalgl_2k.jpg', 'rough': ac / f'{sid}_roughness_2k.jpg'}
    raise SystemExit('no photo set ' + sid)


def jpg_bytes(path, arm_from_rough=False):
    img = Image.open(path)
    if arm_from_rough:  # ambientCG: roughness alone -> ARM (AO 1, roughness, metal 0)
        r = np.asarray(img.convert('L'))
        img = Image.fromarray(np.dstack([np.full_like(r, 255), r, np.zeros_like(r)]), 'RGB')
    img = img.convert('RGB')
    if max(img.size) > 2048:
        img = img.resize((2048, 2048), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, 'JPEG', quality=92)
    return buf.getvalue()


def read(j, binb, i):
    a = j['accessors'][i]
    bv = j['bufferViews'][a['bufferView']]
    dt, sz = CT[a['componentType']]
    n = NC[a['type']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', sz * n)
    raw = np.frombuffer(binb, dtype=np.uint8, count=stride * (a['count'] - 1) + sz * n, offset=off)
    rows = np.lib.stride_tricks.as_strided(raw, shape=(a['count'], sz * n), strides=(stride, 1)).copy()
    return rows.view(dt).reshape(a['count'], n)


def node_matrices(j):
    def local(nd):
        if 'matrix' in nd:
            return np.array(nd['matrix'], dtype=np.float64).reshape(4, 4).T
        t = np.array(nd.get('translation', [0, 0, 0]), dtype=np.float64)
        x, y, z, w = nd.get('rotation', [0, 0, 0, 1])
        s = np.array(nd.get('scale', [1, 1, 1]), dtype=np.float64)
        r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                      [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                      [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        m = np.eye(4)
        m[:3, :3] = r * s
        m[:3, 3] = t
        return m
    world = {}
    parent = {c: i for i, nd in enumerate(j.get('nodes', [])) for c in nd.get('children', [])}

    def get(i):
        if i not in world:
            world[i] = (get(parent[i]) if i in parent else np.eye(4)) @ local(j['nodes'][i])
        return world[i]
    mesh_mat = {}
    for i, nd in enumerate(j.get('nodes', [])):
        if 'mesh' in nd:
            mesh_mat.setdefault(nd['mesh'], get(i))
    return mesh_mat


def main():
    a = sys.argv[1:]
    src, dst = Path(a[0]), Path(a[1])
    opt = lambda k, d=None: a[a.index(k) + 1] if k in a else d
    sid = opt('--set')
    tile = float(opt('--tile', '0'))
    frac = float(opt('--tile-frac', '0'))
    only = set(opt('--materials', '').split(',')) - {''}
    metal = '--metal' in a
    tint = [float(x) for x in opt('--tint', '1,1,1').split(',')]
    if dst.exists():
        raise SystemExit('refusing to overwrite ' + str(dst))
    b = src.read_bytes()
    n = struct.unpack_from('<I', b, 12)[0]
    j = json.loads(b[20:20 + n])
    o = 20 + n
    bl = struct.unpack_from('<I', b, o)[0]
    binb = b[o + 8:o + 8 + bl]
    mats = j.get('materials', [])
    chosen = {i for i, m in enumerate(mats) if not only or m.get('name') in only}
    mesh_mat = node_matrices(j)
    # the model's height (world, up = y) for --tile-frac
    lo, hi = np.full(3, 1e9), np.full(3, -1e9)
    for mi, me in enumerate(j['meshes']):
        M = mesh_mat.get(mi, np.eye(4))
        for p in me['primitives']:
            pos = read(j, binb, p['attributes']['POSITION']).astype(np.float64)
            w = pos @ M[:3, :3].T + M[:3, 3]
            lo, hi = np.minimum(lo, w.min(0)), np.maximum(hi, w.max(0))
    if frac:
        tile = (hi[1] - lo[1]) * frac
    if tile <= 0:
        raise SystemExit('--tile or --tile-frac')
    new_views = []  # (bytes) appended after the kept ones; accessor / image entries point at ('new', k)
    def add_view(data, target=None):
        new_views.append((data, target))
        return ('new', len(new_views) - 1)
    def add_accessor(arr, ctype, typ, target, minmax=False):
        acc = {'bufferView': add_view(arr.tobytes(), target), 'componentType': ctype, 'count': int(arr.shape[0]), 'type': typ}
        if minmax:
            acc['min'] = arr.min(0).tolist()
            acc['max'] = arr.max(0).tolist()
        j['accessors'].append(acc)
        return len(j['accessors']) - 1
    for mi, me in enumerate(j['meshes']):
        M = mesh_mat.get(mi, np.eye(4))
        for p in me['primitives']:
            if p.get('material') not in chosen:
                continue
            at = p['attributes']
            pos = read(j, binb, at['POSITION']).astype(np.float32)
            nor = read(j, binb, at['NORMAL']).astype(np.float32) if 'NORMAL' in at else None
            idx = read(j, binb, p['indices']).reshape(-1).astype(np.int64) if 'indices' in p else np.arange(len(pos))
            tri = idx.reshape(-1, 3)
            P = pos[tri]                                           # (t, 3, 3) mesh space
            W = P.astype(np.float64) @ M[:3, :3].T + M[:3, 3]      # world, for the projection
            fn = np.cross(W[:, 1] - W[:, 0], W[:, 2] - W[:, 0])
            axis = np.argmax(np.abs(fn), axis=1)                   # 0 x, 1 y (up), 2 z
            u = np.where(axis[:, None] == 0, W[..., 2], W[..., 0])
            v = np.where(axis[:, None] == 1, W[..., 2], -W[..., 1])
            uv = (np.stack([u, v], -1) / tile).astype(np.float32).reshape(-1, 2)
            corners = P.reshape(-1, 3)
            attrs = {'POSITION': add_accessor(corners, 5126, 'VEC3', 34962, minmax=True),
                     'TEXCOORD_0': add_accessor(uv, 5126, 'VEC2', 34962)}
            if nor is not None:
                attrs['NORMAL'] = add_accessor(nor[tri].reshape(-1, 3), 5126, 'VEC3', 34962)
            p['attributes'] = attrs
            p['indices'] = add_accessor(np.arange(len(corners), dtype=np.uint32), 5125, 'SCALAR', 34963)
    # the photo set's images
    ps = photo_set(sid)
    j.setdefault('images', [])
    j.setdefault('textures', [])
    j.setdefault('samplers', [])
    j['samplers'].append({'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497})
    samp = len(j['samplers']) - 1
    def add_tex(data, name):
        j['images'].append({'bufferView': add_view(data), 'mimeType': 'image/jpeg', 'name': name})
        j['textures'].append({'sampler': samp, 'source': len(j['images']) - 1})
        return len(j['textures']) - 1
    t_diff = add_tex(jpg_bytes(ps['diff']), f'{sid}_diff')
    t_nor = add_tex(jpg_bytes(ps['nor']), f'{sid}_nor_gl')
    t_arm = add_tex(jpg_bytes(ps['arm']) if 'arm' in ps else jpg_bytes(ps['rough'], arm_from_rough=True), f'{sid}_arm')
    for i in chosen:
        m = mats[i]
        m['pbrMetallicRoughness'] = {'baseColorTexture': {'index': t_diff}, 'metallicRoughnessTexture': {'index': t_arm},
                                     'baseColorFactor': tint + [1], 'metallicFactor': 1.0 if metal else 0.0, 'roughnessFactor': 1.0}
        m['normalTexture'] = {'index': t_nor}
        m['occlusionTexture'] = {'index': t_arm}
        m.pop('emissiveTexture', None)
    # compact: keep the buffer views still used, then the new ones
    used = set()
    for acc in j['accessors']:
        pass
    live_acc = set()
    for me in j['meshes']:
        for p in me['primitives']:
            live_acc.update(p['attributes'].values())
            if 'indices' in p:
                live_acc.add(p['indices'])
            for t in p.get('targets', []):
                live_acc.update(t.values())
    for sk in j.get('skins', []):
        if 'inverseBindMatrices' in sk:
            live_acc.add(sk['inverseBindMatrices'])
    for an in j.get('animations', []):
        for s in an.get('samplers', []):
            live_acc.update([s['input'], s['output']])
    live_img = set()
    for t in j['textures']:
        if 'source' in t:
            live_img.add(t['source'])
    # accessors renumbered (dropped ones removed), images kept (unused images dropped from their bytes only)
    acc_map, new_accs = {}, []
    for i, acc in enumerate(j['accessors']):
        if i in live_acc:
            acc_map[i] = len(new_accs)
            new_accs.append(acc)
    out = bytearray()
    views = []
    def put(data, target=None, stride=None):
        while len(out) % 4:
            out.append(0)
        v = {'buffer': 0, 'byteOffset': len(out), 'byteLength': len(data)}
        if target:
            v['target'] = target
        if stride:
            v['byteStride'] = stride
        out.extend(data)
        views.append(v)
        return len(views) - 1
    view_map = {}
    def map_view(ref):
        if ref in view_map:
            return view_map[ref]
        if isinstance(ref, tuple):
            data, target = new_views[ref[1]]
            view_map[ref] = put(data, target)
        else:
            bv = j['bufferViews'][ref]
            data = binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]
            view_map[ref] = put(data, bv.get('target'), bv.get('byteStride'))
        return view_map[ref]
    for acc in new_accs:
        acc['bufferView'] = map_view(acc['bufferView'])
    for i, im in enumerate(j['images']):
        if 'bufferView' in im:
            if i in live_img:
                im['bufferView'] = map_view(im['bufferView'])
            else:
                im['bufferView'] = map_view(add_view(b''))
    j['accessors'] = new_accs
    for me in j['meshes']:
        for p in me['primitives']:
            p['attributes'] = {k: acc_map[v] for k, v in p['attributes'].items()}
            if 'indices' in p:
                p['indices'] = acc_map[p['indices']]
            if 'targets' in p:
                p['targets'] = [{k: acc_map[v] for k, v in t.items()} for t in p['targets']]
    for sk in j.get('skins', []):
        if 'inverseBindMatrices' in sk:
            sk['inverseBindMatrices'] = acc_map[sk['inverseBindMatrices']]
    for an in j.get('animations', []):
        for s in an.get('samplers', []):
            s['input'], s['output'] = acc_map[s['input']], acc_map[s['output']]
    while len(out) % 4:
        out.append(0)
    j['bufferViews'] = views
    j['buffers'] = [{'byteLength': len(out)}]
    j.setdefault('extensionsUsed', [])
    js = json.dumps(j, separators=(',', ':')).encode('utf-8')
    js += b' ' * ((4 - len(js) % 4) % 4)
    glb = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(out))
    glb += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(out), 0x004E4942) + bytes(out)
    dst.write_bytes(glb)
    print('%s: %s on %d of %d parts, tile %.2f (model height %.2f), %.1f MB' % (dst.name, sid, len(chosen), len(mats), tile,
                                                                                  hi[1] - lo[1], len(glb) / 1048576))


if __name__ == '__main__':
    main()
