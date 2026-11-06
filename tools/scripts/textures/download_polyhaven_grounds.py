"""Every free Poly Haven ground texture (CC0), downloaded into content/textures/polyhaven/<id>/ (the user, 2026-11-06: "baixar todos os
solos gratuitos do PolyHaven que a gente vai usar um pouquinho de cada").

Grounds = the textures in Poly Haven's categories terrain, sand, gravel, snow and aerial (168 on 2026-11-06). Per texture, at 2K JPG:
the colour (diff), the OpenGL normal (nor_gl), AO / roughness / metal packed (arm) and the height (disp), as Poly Haven names them
(<id>_diff_2k.jpg ...). Files already there are skipped (re-run to resume). content/textures/polyhaven/grounds.json lists each one:
name, categories, tags, the files.

  python -I tools/scripts/textures/download_polyhaven_grounds.py [--res 2k] [--only id,id]
"""
import json
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'content/textures/polyhaven'
API = 'https://api.polyhaven.com'
CATEGORIES = {'terrain', 'sand', 'gravel', 'snow', 'aerial'}
MAPS = {'Diffuse': 'diff', 'nor_gl': 'nor_gl', 'arm': 'arm', 'Displacement': 'disp'}
HEADERS = {'User-Agent': 'EmberEngine-asset-fetch/1.0'}


def get(url, tries=4):
    for t in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=120) as r:
                return r.read()
        except Exception as e:
            if t == tries - 1:
                raise
            print('  retry', url, e)
            time.sleep(3 * (t + 1))


def main():
    res = sys.argv[sys.argv.index('--res') + 1] if '--res' in sys.argv else '2k'
    only = set(sys.argv[sys.argv.index('--only') + 1].split(',')) if '--only' in sys.argv else None
    assets = json.loads(get(API + '/assets?t=textures'))
    ids = sorted(k for k, v in assets.items() if CATEGORIES & set(v.get('categories', [])) and (not only or k in only))
    manifest_path = OUT / 'grounds.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {}
    total = 0
    for n, tid in enumerate(ids, 1):
        a = assets[tid]
        files = json.loads(get('%s/files/%s' % (API, tid)))
        folder = OUT / tid
        folder.mkdir(parents=True, exist_ok=True)
        got = {}
        for key, short in MAPS.items():
            entry = files.get(key, {}).get(res, {}).get('jpg')
            if not entry:
                continue
            dst = folder / ('%s_%s_%s.jpg' % (tid, short, res))
            if not dst.exists() or dst.stat().st_size != entry.get('size', -1):
                data = get(entry['url'])
                tmp = dst.with_suffix('.part')
                tmp.write_bytes(data)
                tmp.replace(dst)
                total += len(data)
            got[short] = dst.name
        manifest[tid] = {'name': a.get('name'), 'categories': a.get('categories', []), 'tags': a.get('tags', []), 'res': res,
                         'files': got}
        manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False), encoding='utf-8')
        print('%3d/%d %s %s (%.0f MB so far)' % (n, len(ids), tid, sorted(got), total / 1048576), flush=True)
    print('done: %d grounds, %.0f MB downloaded this run' % (len(ids), total / 1048576))


if __name__ == '__main__':
    main()
