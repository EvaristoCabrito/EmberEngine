"""Texture the flat-colour parts of a GLB with photographs already in the project, as a NEW version (the user, 2026-11-06:
"começar a texturizar todos os objetos que estão sem textura ... com certeza temos algo que você pode reaproveitar").

A part (material) with no baseColorTexture, named after a surface below (its "-photo" suffix ignored), gets one:
  photo   the material's own photograph, factor white (iron, dark glass, clay, leather, burlap, rope);
  detail  a grey detail of a photograph (its luminance, mean ~0.75 in linear), the material's own colour kept as the factor and
          raised by the same mean, so the part keeps the colour it was designed with and gains the surface (cloth, wool, paper,
          brass, silver).
The parts' own UVs are used (the kits' triplanar UVs, the same density as their planks); geometry and the other parts untouched.
Grey details are made once into content/models/textures (*-detail-v01.jpg) from:
  fabric  wt-burlap.jpg (the weave)         paper   wt-limestone.jpg (fine grain, low contrast)
  brass   ambientcg metal-053c             silver  ambientcg metal-061a

  python -I tools/scripts/models/texture_flat_parts.py <in.glb> <out.glb> [...]
"""
import io
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
TEX = ROOT / 'content/models/textures'
ACG = ROOT / 'content/textures/ambientcg'

PHOTO = {
    'iron': TEX / 'wt-iron.jpg',
    'iron-rust': TEX / 'wt-iron.jpg',
    'dark-glass': TEX / 'dark-green-glass-photo-v01.png',
    'clay': TEX / 'terracotta-photo-v01.png',
    'leather': TEX / 'leather-worn-photo-v01.png',
    'burlap': TEX / 'wt-burlap.jpg',
    'rope': TEX / 'winch-rope-hemp-color-v01.png',
}
DETAIL_SOURCES = {
    'fabric': (TEX / 'wt-burlap.jpg', 1.0),
    'paper': (TEX / 'wt-limestone.jpg', 0.35),
    'brass': (ACG / 'metal-053c/metal-053c_color_1k.jpg', 1.0),
    'silver': (ACG / 'metal-061a/metal-061a_color_1k.jpg', 1.0),
}
DETAIL = {'cloth-red': 'fabric', 'cloth-dark': 'fabric', 'wool': 'fabric', 'paper': 'paper', 'brass': 'brass', 'silver': 'silver'}
kDetailMean = 0.75  # linear mean of a grey detail


def srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(np.maximum(c, 0), 1 / 2.4) - 0.055)


def detail_image(kind):
    out = TEX / ('%s-detail-v01.jpg' % kind)
    if not out.exists():
        src, contrast = DETAIL_SOURCES[kind]
        a = np.asarray(Image.open(src).convert('RGB').resize((1024, 1024), Image.LANCZOS), dtype=np.float64) / 255.0
        lum = srgb_to_linear(a) @ np.array([0.2126, 0.7152, 0.0722])
        lum = lum / max(lum.mean(), 1e-6)                       # mean 1
        lum = 1.0 + (lum - 1.0) * contrast                      # paper: a soft grain
        lum = np.clip(lum * kDetailMean, 0.0, 1.0)
        g = (linear_to_srgb(lum) * 255.0 + 0.5).astype(np.uint8)
        Image.fromarray(g, 'L').convert('RGB').save(out, quality=92)
        print('  made', out.relative_to(ROOT))
    return out


def image_bytes(path):
    img = Image.open(path).convert('RGB')
    if max(img.size) > 1024:
        img = img.resize((1024, 1024), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, 'JPEG', quality=92)
    return buf.getvalue()


def texture(src, dst):
    if Path(dst).exists():
        raise SystemExit('refusing to overwrite ' + str(dst))
    b = Path(src).read_bytes()
    n = struct.unpack_from('<I', b, 12)[0]
    j = json.loads(b[20:20 + n])
    o = 20 + n
    bl, kind = struct.unpack_from('<II', b, o)
    assert kind == 0x004E4942
    binb = bytearray(b[o + 8:o + 8 + bl])
    if not j.get('samplers'):
        j['samplers'] = [{'magFilter': 9729, 'minFilter': 9987}]
    sampler = 0
    j.setdefault('images', [])
    j.setdefault('textures', [])
    added = {}  # image file -> texture index

    def add_texture(path):
        if path in added:
            return added[path]
        data = image_bytes(path)
        while len(binb) % 4:
            binb.append(0)
        j['bufferViews'].append({'buffer': 0, 'byteOffset': len(binb), 'byteLength': len(data)})
        binb.extend(data)
        j['images'].append({'bufferView': len(j['bufferViews']) - 1, 'mimeType': 'image/jpeg', 'name': Path(path).stem})
        j['textures'].append({'sampler': sampler, 'source': len(j['images']) - 1})
        added[path] = len(j['textures']) - 1
        return added[path]

    done = []
    for m in j.get('materials', []):
        pbr = m.setdefault('pbrMetallicRoughness', {})
        if 'baseColorTexture' in pbr:
            continue
        key = (m.get('name') or '').removesuffix('-photo')
        factor = pbr.get('baseColorFactor', [1, 1, 1, 1])
        alpha = factor[3] if len(factor) > 3 else 1.0
        if key in PHOTO:
            pbr['baseColorTexture'] = {'index': add_texture(PHOTO[key])}
            pbr['baseColorFactor'] = [1.0, 1.0, 1.0, alpha]
            done.append('%s: photo %s' % (key, PHOTO[key].name))
        elif key in DETAIL:
            pbr['baseColorTexture'] = {'index': add_texture(detail_image(DETAIL[key]))}
            pbr['baseColorFactor'] = [round(min(1.0, c / kDetailMean), 4) for c in factor[:3]] + [alpha]
            done.append('%s: %s detail, factor %s' % (key, DETAIL[key], pbr['baseColorFactor'][:3]))
        else:
            done.append('%s: LEFT FLAT (no photo for it)' % key)
    while len(binb) % 4:
        binb.append(0)
    j['buffers'][0]['byteLength'] = len(binb)
    js = json.dumps(j, separators=(',', ':')).encode('utf-8')
    js += b' ' * ((4 - len(js) % 4) % 4)
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binb))
    out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(binb), 0x004E4942) + bytes(binb)
    Path(dst).write_bytes(out)
    print(dst)
    for d in done:
        print('  ' + d)


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) < 2 or len(a) % 2:
        raise SystemExit(__doc__)
    for s, d in zip(a[0::2], a[1::2]):
        texture(s, d)
