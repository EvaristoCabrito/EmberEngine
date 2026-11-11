"""The Farmlands houses' wall kit: `ember-arch-fazenda-*` (2026-11-11; the user: "você precisa habilitar luzes e probes vindos das
janelas e entradas das casas do farmlands", "crie janelas"). The houses stood on Ember's plain plaster wall
(`wall-3d-farmland-aged-plaster-014`): no window, no door piece, so a house had no opening for the light to come in through.
This kit is the Mosteiro's building pieces (wall, window, slit, wall torch, portal, door, locked door) with the houses' own aged
plaster and a house wall's thickness, so a house is lit through its windows and its door like the Torre de Vigia.

  python -I tools/scripts/models/register_farmhouse_architecture.py

Writes content/models/architecture.json (the pieces) and content/data/editor-catalog.json (decorations, decoMeta, decorationInfo,
architectureDefs, architectureOrder: each a copy of the Mosteiro piece's entry). Only `ember-arch-fazenda-*` is touched. Idempotent.
"""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ARCH = ROOT / 'content/models/architecture.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'
SRC, DST = 'ember-arch-mosteiro-', 'ember-arch-fazenda-'
PIECES = ('wall', 'window', 'slit', 'wall-torch', 'portal', 'door', 'door-locked')
TEXTURE, REPEAT, THICKNESS = '/game/textures/walls/farmland-aged-plaster-014.png', 3.2, 1.3


def rename(text):
    return text.replace('do Mosteiro', 'da Fazenda').replace('Mosteiro', 'Fazenda') if isinstance(text, str) else text


def main():
    arch = json.loads(ARCH.read_text(encoding='utf-8'))
    pieces = [p for p in arch['pieces'] if not p['id'].startswith(DST)]
    made = []
    for p in pieces:
        if not p['id'].startswith(SRC) or p['id'][len(SRC):] not in PIECES:
            continue
        q = copy.deepcopy(p)
        q['id'] = DST + p['id'][len(SRC):]
        q['name'] = rename(p.get('name', q['id']))
        q['wallTexture'], q['textureRepeat'], q['wallThicknessScale'] = TEXTURE, REPEAT, THICKNESS
        q.pop('pendingTexture', None)
        made.append(q)
    if len(made) != len(PIECES):
        raise SystemExit(f'the Mosteiro kit has {len(made)} of the {len(PIECES)} pieces wanted')
    arch['pieces'] = pieces + made
    text = ARCH.read_text(encoding='utf-8')
    ARCH.write_text(json.dumps(arch, ensure_ascii=False, indent=2) + ('\n' if text.endswith('\n') else ''), encoding='utf-8')

    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    order = [k for k in editor['architectureOrder'] if not k.startswith(DST)]
    at = max(i for i, k in enumerate(order) if k.startswith('wall-3d-farmland-')) + 1  # beside the houses' old plaster walls
    for q in made:
        src = SRC + q['id'][len(DST):]
        for table in (catalog['decorations'], editor['decoMeta'], editor['decorationInfo'], editor['architectureDefs']):
            e = copy.deepcopy(table[src])
            if 'name' in e:
                e['name'] = q['name']
            if 'wallTexture' in e:
                e['wallTexture'] = TEXTURE
            if 'textureRepeat' in e:
                e['textureRepeat'] = REPEAT
            if 'wallThicknessScale' in e:
                e['wallThicknessScale'] = THICKNESS
            table[q['id']] = e
    editor['architectureOrder'] = order[:at] + [q['id'] for q in made] + order[at:]
    CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(len(made), 'pieces:', ', '.join(q['id'] for q in made))


if __name__ == '__main__':
    main()
