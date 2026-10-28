"""Adds Ember's art measurements to an imported scene (export_scene.ts output), in place.

Ember measures each decoration image when it loads it (src/world/decor.ts loadArt): on a copy at most
256 px wide, the lowest row with solid pixels (alpha > 128) is the art's base and the centre of the solid
pixels in the band just above it anchors the card; the brightness-weighted centroid of fire-coloured
pixels is the painted flame (where a light-emitting decoration's light sits). Same rules here, with v
measured upwards from the bottom like Ember's flipped bitmap. The 256-wide copy is made with a bilinear
resize (the browser's canvas smoothing is similar, not identical).

Usage: python -I measure_art.py <scene.json> <ember public dir>
       python -I measure_art.py --catalog <editor-catalog.json> <ember public dir>
"""
import json
import sys
from pathlib import Path

from PIL import Image


def measure(path):
    img = Image.open(path).convert('RGBA')
    sw = min(256, img.width)
    sh = max(1, round(img.height * sw / img.width))
    small = img.resize((sw, sh), Image.BILINEAR).transpose(Image.FLIP_TOP_BOTTOM)  # row 0 = bottom
    d = small.load()
    weight = fu = fv = 0.0
    for y in range(sh):
        for x in range(sw):
            r, g, b, a = d[x, y]
            if a < 128 or r < 190 or g < 90 or r < g or g < b + 20:
                continue
            w = (r + g - b) / 255
            weight += w
            fu += w * (x + 0.5)
            fv += w * (y + 0.5)
    flame_found = weight > 4
    flame = {'u': fu / weight / sw, 'v': fv / weight / sh} if flame_found else {'u': 0.5, 'v': 0.7}
    bottom = 0
    found = False
    for y in range(sh):
        for x in range(sw):
            if d[x, y][3] > 128:
                bottom, found = y, True
                break
        if found:
            break
    band = max(2, round(sh * 0.08))
    sx = n = 0
    for y in range(bottom, min(sh, bottom + band)):
        for x in range(sw):
            if d[x, y][3] > 128:
                sx += x
                n += 1
    return {'base': {'u': (sx / n + 0.5) / sw if n else 0.5, 'v': bottom / sh}, 'flame': flame, 'flameFound': flame_found,
            'size': [img.width, img.height]}


def measure_catalog(catalog_path, public):
    """--catalog: every image the editor catalog's facings can show (export_editor_catalog.ts), measured once."""
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    measures = catalog.get('measures', {})
    for faces in catalog['facings'].values():
        for f in faces:
            if f['art'] and f['image'] not in measures:
                path = public / f['image'].lstrip('/')
                if path.exists():
                    measures[f['image']] = measure(path)
    catalog['measures'] = measures
    catalog_path.write_text(json.dumps(catalog), encoding='utf-8')
    print(f'{catalog_path.name}: {len(measures)} images measured')


def main():
    if sys.argv[1] == '--catalog':
        return measure_catalog(Path(sys.argv[2]), Path(sys.argv[3]))
    scene_path, public = Path(sys.argv[1]), Path(sys.argv[2])
    scene = json.loads(scene_path.read_text(encoding='utf-8'))
    cache = {}
    for d in scene['decor']:
        if d['kind'] != 'card':
            continue
        if d['image'] not in cache:
            cache[d['image']] = measure(public / d['image'].lstrip('/'))
        d['art'] = cache[d['image']]
    scene_path.write_text(json.dumps(scene, indent=1), encoding='utf-8')
    print(f'{scene_path.name}: measured {len(cache)} images')


if __name__ == '__main__':
    main()
