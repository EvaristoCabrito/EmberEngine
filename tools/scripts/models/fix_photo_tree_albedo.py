"""New versions of the photo trees whose photo textures were drawn near black (the user, 2026-11-06: "texturas que não
aparecem nas árvores em Ashen Forest").

The game draws baseColorFactor x COLOR_0 x texture. These GLBs kept the colours of the procedural trees they were made from:
the bark's vertex colour (3-10 % bright) and the leaves' flat green factor (0.07, 0.16, 0.03), both multiplied over the photos.
Each new version:
  - COLOR_0 scaled so the GLB's brightest vertex (95th percentile luminance) is 1: the crevices / hollow stay relatively darker,
    the photo shows at its own brightness;
  - a textured material whose factor is a flat colour (max < 0.5) gets a neutral grey of the same relative brightness
    (its luminance over the brightest such material's, at least 0.6): the leaves keep their light / dark variation, no double green.
Geometry, UVs, images and everything else are byte for byte the same. The source GLB is never touched.

  python -I tools/scripts/models/fix_photo_tree_albedo.py <in.glb> <out.glb> [...]
"""
import json
import struct
import sys
from pathlib import Path

import numpy as np

CT = {5126: ('<f4', 4), 5121: ('u1', 1), 5123: ('<u2', 2)}
NC = {'VEC3': 3, 'VEC4': 4}


kTintLum = 0.35


def image_saturation(j, b, bin_start, index):
    import io
    from PIL import Image
    im = j['images'][index]
    bv = j['bufferViews'][im['bufferView']]
    start = bin_start + bv.get('byteOffset', 0)
    img = Image.open(io.BytesIO(bytes(b[start:start + bv['byteLength']]))).convert('RGB').resize((64, 64))
    a = np.asarray(img, dtype=np.float64) / 255.0
    mean = a.reshape(-1, 3).mean(0)
    return float((mean.max() - mean.min()) / max(mean.max(), 1e-6))


def lum(c):
    return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]


def fix(src, dst):
    b = bytearray(Path(src).read_bytes())
    n = struct.unpack_from('<I', b, 12)[0]
    j = json.loads(bytes(b[20:20 + n]))
    bin_start = 20 + n + 8
    # every COLOR_0 accessor, read
    colours = {}
    for me in j['meshes']:
        for p in me['primitives']:
            i = p['attributes'].get('COLOR_0')
            if i is None or i in colours:
                continue
            a = j['accessors'][i]
            bv = j['bufferViews'][a['bufferView']]
            dt, sz = CT[a['componentType']]
            k = NC[a['type']]
            off = bin_start + bv.get('byteOffset', 0) + a.get('byteOffset', 0)
            stride = bv.get('byteStride', sz * k)
            scale = 1.0 if dt == '<f4' else (255.0 if dt == 'u1' else 65535.0)
            vals = np.array([np.frombuffer(b, dtype=dt, count=k, offset=off + r * stride) for r in range(a['count'])],
                            dtype=np.float64) / scale
            colours[i] = (vals, dt, sz, k, off, stride, scale)
    changed = []
    if colours:
        allrgb = np.concatenate([v[0][:, :3] for v in colours.values()])
        top = float(np.percentile(lum(allrgb), 95))
        if top < 0.5:
            gain = 1.0 / max(top, 1e-4)
            for i, (vals, dt, sz, k, off, stride, scale) in colours.items():
                vals = vals.copy()
                vals[:, :3] = np.clip(vals[:, :3] * gain, 0.0, 1.0)
                for r in range(len(vals)):
                    raw = (vals[r] * scale)
                    if dt != '<f4':
                        raw = np.round(raw)
                    b[off + r * stride:off + r * stride + sz * k] = raw.astype(dt).tobytes()
            changed.append('COLOR_0 x%.1f (95th percentile luminance %.3f -> 1)' % (gain, top))
    flat = []
    for m in j.get('materials', []):
        pbr = m.get('pbrMetallicRoughness', {})
        f = pbr.get('baseColorFactor')
        if f and 'baseColorTexture' in pbr and max(f[:3]) < 0.5:
            flat.append(m)
    if flat:
        lums = [float(lum(np.array(m['pbrMetallicRoughness']['baseColorFactor'][:3]))) for m in flat]
        brightest = max(lums)
        for m, l in zip(flat, lums):
            old = m['pbrMetallicRoughness']['baseColorFactor']
            alpha = old[3] if len(old) > 3 else 1.0
            src = j['textures'][m['pbrMetallicRoughness']['baseColorTexture']['index']].get('source')
            if src is not None and image_saturation(j, b, bin_start, src) < 0.15:
                # a colourless photo (the snowy pine's needles wear the snow image): the factor is the only colour, keep its
                # hue, brightest such material at luminance kTintLum
                gain = kTintLum / brightest
                new = [round(min(1.0, c * gain), 3) for c in old[:3]]
                m['pbrMetallicRoughness']['baseColorFactor'] = new + [alpha]
                changed.append('%s: factor %s -> %s (hue kept: the photo has no colour)' % (m.get('name'), [round(x, 3) for x in old[:3]], new))
                continue
            g = round(max(0.6, l / brightest), 3)
            m['pbrMetallicRoughness']['baseColorFactor'] = [g, g, g, alpha]
            changed.append('%s: factor %s -> %.3f grey' % (m.get('name'), [round(x, 3) for x in old[:3]], g))
    # the JSON chunk again (padded with spaces to 4 bytes), the binary chunk as it is
    js = json.dumps(j, separators=(',', ':')).encode('utf-8')
    js += b' ' * ((4 - len(js) % 4) % 4)
    rest = bytes(b[20 + n:])
    out = bytearray()
    out += struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + len(rest))
    out += struct.pack('<II', len(js), 0x4E4F534A) + js + rest
    if Path(dst).exists():
        raise SystemExit('refusing to overwrite ' + str(dst))
    Path(dst).write_bytes(bytes(out))
    print(dst)
    for c in changed:
        print('  ' + c)


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) < 2 or len(a) % 2:
        raise SystemExit(__doc__)
    for s, d in zip(a[0::2], a[1::2]):
        fix(s, d)
