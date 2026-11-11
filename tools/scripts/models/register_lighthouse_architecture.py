"""The Light House's wall kit: `ember-arch-farol-*`, the Mosteiro kit's thirteen pieces (wall, window, slit, wall torch, portal, door,
locked door, stairs up / down, rampart, rampart torch, rampart gate, gate) with the Watchtower's round-tower stone until the Light
House's own photograph arrives (docs/LIGHTHOUSE.md: farol-wall-v01.jpg).

  python -I tools/scripts/models/register_lighthouse_architecture.py

Writes content/models/architecture.json (the pieces) and content/data/editor-catalog.json (decorations, decoMeta, decorationInfo,
architectureDefs, architectureOrder: each a copy of the Mosteiro piece's entry). Only `ember-arch-farol-*` is touched. Idempotent.
"""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ARCH = ROOT / 'content/models/architecture.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
SRC, DST = 'ember-arch-mosteiro-', 'ember-arch-farol-'
TEXTURE, REPEAT = '/ember/textures/walls/watchtower-v01.jpg', 3.6
PENDING = "/ember/textures/walls/farol-v01.jpg (docs/LIGHTHOUSE.md: the user's photo; until it arrives, the Watchtower's stone)"


def rename(text):
    return text.replace('Mosteiro', 'Farol').replace('mosteiro', 'farol') if isinstance(text, str) else text


def main():
    arch = json.loads(ARCH.read_text(encoding='utf-8'))
    pieces = [p for p in arch['pieces'] if not p['id'].startswith(DST)]
    made = []
    for p in pieces:
        if not p['id'].startswith(SRC):
            continue
        q = copy.deepcopy(p)
        q['id'] = DST + p['id'][len(SRC):]
        q['name'] = rename(p.get('name', q['id']))
        q['wallTexture'], q['textureRepeat'] = TEXTURE, REPEAT
        q.pop('pendingTexture', None)
        if q['id'] == DST + 'wall':
            q['pendingTexture'] = PENDING
        made.append(q)
    # the tower's round wall and its torch (ArchitectureDef::round: one smooth curve between its two neighbours)
    for style, name in (('wall', 'Parede redonda do Farol 3D'), ('wall-torch', 'Parede redonda do Farol com tocha 3D')):
        base = next(q for q in made if q['id'] == DST + style)
        r = copy.deepcopy(base)
        r['id'], r['name'], r['round'] = DST + 'round-' + style, name, True
        r.pop('pendingTexture', None)
        made.append(r)
    arch['pieces'] = pieces + made
    text = ARCH.read_text(encoding='utf-8')
    ARCH.write_text(json.dumps(arch, ensure_ascii=False, indent=2) + ('\n' if text.endswith('\n') else ''), encoding='utf-8')

    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    order = [k for k in editor['architectureOrder'] if not k.startswith(DST)]
    at = max(i for i, k in enumerate(order) if k.startswith(SRC)) + 1
    for q in made:
        src = SRC + q['id'][len(DST):].replace('round-', '')
        for table in (catalog['decorations'], editor['decoMeta'], editor['decorationInfo'], editor['architectureDefs']):
            e = copy.deepcopy(table[src])
            if 'name' in e:
                e['name'] = q['name']
            if 'wallTexture' in e:
                e['wallTexture'] = TEXTURE
            if 'textureRepeat' in e:
                e['textureRepeat'] = REPEAT
            if table is editor['architectureDefs']:
                e['round'] = bool(q.get('round'))
            table[q['id']] = e
    editor['architectureOrder'] = order[:at] + [q['id'] for q in made] + order[at:]
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(len(made), 'pieces:', ', '.join(q['id'] for q in made))


if __name__ == '__main__':
    main()
