"""Poly Haven foliage whose transparency was lost at import, as NEW versions (the user, 2026-11-06, of the grass and the moss drawn
as dark cards: "o download que você precisar").

The v01 GLBs carry the colour map as an RGB PNG (no alpha) and the material as BLEND, so every blade / moss card is drawn whole,
with its black padding. Poly Haven publishes the alpha as its own map (<id>_alpha_2k.png, CC0, kept in build/polyhaven-src/2k/<id>/).
Each new version: the colour map becomes RGBA (the embedded RGB + that alpha, 16-bit taken to 8), named <id>_diff_alpha_2k like the
fern's v02, the material MASK (the engine's alpha test, cutoff 0.5), everything else byte for byte. The binary chunk is rebuilt
compactly (the old RGB image is not kept inside).

  python -I tools/scripts/models/polyhaven_alpha_v02.py <alpha.png> <in.glb> <out.glb> [<in.glb> <out.glb> ...]
"""
import io
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def load_alpha(path):
    im = Image.open(path)
    a = np.asarray(im)
    if a.dtype != np.uint8:
        a = (a.astype(np.float64) / (65535.0 if a.max() > 255 else 255.0) * 255.0 + 0.5).clip(0, 255).astype(np.uint8)
    if a.ndim == 3:
        a = a[..., 0]
    return a


def convert(alpha, src, dst):
    if Path(dst).exists():
        raise SystemExit('refusing to overwrite ' + str(dst))
    b = Path(src).read_bytes()
    if b[:4] != b'glTF':
        raise SystemExit(str(src) + ' is not a GLB (an LFS pointer? run git lfs pull)')
    n = struct.unpack_from('<I', b, 12)[0]
    j = json.loads(b[20:20 + n])
    o = 20 + n
    bl = struct.unpack_from('<I', b, o)[0]
    binb = b[o + 8:o + 8 + bl]
    done = set()
    replace = {}  # bufferView index -> new bytes
    for m in j['materials']:
        pbr = m.get('pbrMetallicRoughness', {})
        if 'baseColorTexture' not in pbr:
            continue
        img_i = j['textures'][pbr['baseColorTexture']['index']]['source']
        if img_i not in done:
            im = j['images'][img_i]
            bv = j['bufferViews'][im['bufferView']]
            rgb = Image.open(io.BytesIO(binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']])).convert('RGB')
            a = alpha
            if a.shape != (rgb.height, rgb.width):
                a = np.asarray(Image.fromarray(a, 'L').resize(rgb.size, Image.LANCZOS))
            rgba = np.dstack([np.asarray(rgb), a])
            buf = io.BytesIO()
            Image.fromarray(rgba, 'RGBA').save(buf, 'PNG', optimize=False)
            replace[im['bufferView']] = buf.getvalue()
            im['mimeType'] = 'image/png'
            im['name'] = (im.get('name') or 'diff').replace('_diff_', '_diff_alpha_')
            done.add(img_i)
        m['alphaMode'] = 'MASK'
        m.pop('alphaCutoff', None)  # 0.5, the glTF default (as the fern's v02)
    out = bytearray()
    for i, bv in enumerate(j['bufferViews']):
        data = replace.get(i)
        if data is None:
            data = binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]
        while len(out) % 4:
            out.append(0)
        bv['byteOffset'] = len(out)
        bv['byteLength'] = len(data)
        out += data
    while len(out) % 4:
        out.append(0)
    j['buffers'] = [{'byteLength': len(out)}]
    js = json.dumps(j, separators=(',', ':')).encode('utf-8')
    js += b' ' * ((4 - len(js) % 4) % 4)
    glb = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(out))
    glb += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(out), 0x004E4942) + bytes(out)
    Path(dst).write_bytes(glb)
    print(dst, '%.1f MB' % (len(glb) / 1048576))


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) < 3 or len(a) % 2 == 0:
        raise SystemExit(__doc__)
    alpha = load_alpha(a[0])
    for s, d in zip(a[1::2], a[2::2]):
        convert(alpha, s, d)
