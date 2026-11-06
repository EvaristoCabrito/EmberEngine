"""Audit every 3D model the editor catalog places: what colour each part really draws with.

The game draws a mesh part's albedo as baseColorFactor x COLOR_0 (vertex colour) x baseColorTexture (shaders/resolve*.hlsl).
A part is reported when it has no texture, no UVs, or when its vertex colour / factor darkens a texture to near black
(the -photo trees of 2026-11-05: photo bark under the old procedural vertex bark, 3-9 % bright, drawn black).

  python -I tools/scripts/models/audit_model_albedo.py [<staged GLB root>] [--json out.json]

The staged root is the folder the mesh pack was cooked from (default build/meshes/src-v38): "/game/x.glb" -> <root>/x.glb.
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
DARK = 0.035  # linear albedo under this reads black in play (a coal-black surface is ~0.02-0.04)


def read_glb(path):
    b = path.read_bytes()
    n = struct.unpack_from('<I', b, 12)[0]
    j = json.loads(b[20:20 + n])
    o = 20 + n
    binb = b''
    if o + 8 <= len(b):
        bl = struct.unpack_from('<I', b, o)[0]
        binb = b[o + 8:o + 8 + bl]
    return j, binb


def accessor(j, binb, i):
    a = j['accessors'][i]
    bv = j['bufferViews'][a['bufferView']]
    dt, sz = CT[a['componentType']]
    n = NC[a['type']]
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', sz * n)
    count = a['count']
    raw = np.frombuffer(binb, dtype=np.uint8, count=stride * (count - 1) + sz * n, offset=off)
    rows = np.lib.stride_tricks.as_strided(raw, shape=(count, sz * n), strides=(stride, 1)).copy()
    v = rows.view(dt).reshape(count, n).astype(np.float64)
    if a.get('normalized'):
        v /= 255.0 if dt == 'u1' else 65535.0
    return v


def srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


_image_cache = {}


def image_mean(j, binb, path, index):
    key = (str(path), index)
    if key in _image_cache:
        return _image_cache[key]
    im = j['images'][index]
    try:
        if 'bufferView' in im:
            bv = j['bufferViews'][im['bufferView']]
            data = binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]
            img = Image.open(io.BytesIO(data))
        else:
            img = Image.open(path.parent / im['uri'])
        img = img.convert('RGBA').resize((64, 64))
        a = np.asarray(img, dtype=np.float64) / 255.0
        rgb = srgb_to_linear(a[..., :3])
        w = a[..., 3:4]
        mean = (rgb * w).sum((0, 1)) / max(w.sum(), 1e-6)
    except Exception as e:  # an image the cooker could not read either
        mean = None
    _image_cache[key] = mean
    return mean


def audit_glb(path):
    j, binb = read_glb(path)
    mats = j.get('materials', [])
    parts = []
    for me in j.get('meshes', []):
        for p in me['primitives']:
            at = p.get('attributes', {})
            mi = p.get('material')
            m = mats[mi] if mi is not None else {}
            pbr = m.get('pbrMetallicRoughness', {})
            factor = np.array(pbr.get('baseColorFactor', [1, 1, 1, 1])[:3], dtype=np.float64)
            tex = pbr.get('baseColorTexture')
            uv = 'TEXCOORD_0' in at
            vc = None
            if 'COLOR_0' in at:
                vc = accessor(j, binb, at['COLOR_0'])[:, :3].mean(0)
            tmean = None
            if tex is not None:
                src = j['textures'][tex['index']].get('source')
                if src is not None:
                    tmean = image_mean(j, binb, path, src)
            albedo = factor * (vc if vc is not None else 1.0) * (tmean if tmean is not None else 1.0)
            problem = []
            if tex is None:
                problem.append('sem textura')
            elif not uv:
                problem.append('sem UV')
            elif tmean is None:
                problem.append('textura ilegivel')
            if float(albedo.max()) < DARK:
                problem.append('escura (%.3f)%s' % (float(albedo.max()), ' pela cor de vertice' if vc is not None and float(vc.max()) < 0.5 else ''))
            parts.append({
                'material': m.get('name'),
                'texture': tex is not None,
                'uv': uv,
                'vertexColor': None if vc is None else [round(float(x), 3) for x in vc],
                'albedo': [round(float(x), 3) for x in albedo],
                'problems': problem,
            })
    return parts


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = None
    if '--json' in sys.argv:
        out = sys.argv[sys.argv.index('--json') + 1]
        args = [a for a in args if a != out]
    stage = Path(args[0]) if args else ROOT / 'build/meshes/src-v38'
    cat = json.loads((ROOT / 'content/data/editor-catalog.json').read_text(encoding='utf-8'))['decorations']
    files = {}
    for did, d in cat.items():
        mdl = d.get('model') if isinstance(d, dict) else None
        if mdl and mdl.get('file'):
            files.setdefault(mdl['file'], []).append(did)
    report = {}
    bad = 0
    for f in sorted(files):
        rel = f[len('/game/'):]
        path = stage / rel
        if not path.exists():
            report[f] = {'ids': files[f], 'missing': True}
            print('FALTA  ', f)
            bad += 1
            continue
        parts = audit_glb(path)
        probs = [p for p in parts if p['problems']]
        report[f] = {'ids': files[f], 'parts': len(parts), 'bad': probs}
        if probs:
            bad += 1
            kinds = sorted({k.split(' (')[0] for p in probs for k in p['problems']})
            print('%-8s %3d/%3d partes  %s  %s' % ('PROBLEMA', len(probs), len(parts), ', '.join(kinds), f))
    print('%d modelos, %d com problema' % (len(files), bad))
    if out:
        Path(out).write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding='utf-8')


if __name__ == '__main__':
    main()
