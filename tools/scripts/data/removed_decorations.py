"""Decorations taken out of EmberEngine's catalogs on the user's order (Ember's own files untouched):

  wall-3d-low  "Mureta de pedra 3D": Ember gives it no wall texture (wallTexture null), so it draws as a flat grey slab
               (the user, 2026-10-20: "apaga essa mureta 3d sem textura ... para eu nunca mais ter que ver essa merda")
  ember-dungeon-broken-arch  "Arco de pedra quebrado 3D" (the user, 2026-11-05: a list of 3D props "que não quero aparecer em nenhum
               outro lugar do mapa"; also scrubbed from content/models/models.json and every map in content/maps/ember that placed it;
               the GLB stays in content/models, unused)
  ember-winter-snow-boulder  "Rochedo nevado 3D" (the user, 2026-11-05: "pode apagar de tudo onde aparecer e de todos arquivos do
               jogo"): out of the catalog, models.json, every map, kit_winter.py and make_cemetery_v2.py; its GLB, .blend and
               thumbnail deleted (git history keeps them)

Maps too: python -I tools/scripts/data/removed_decorations.py content/maps/ember/<map>.json ... drops every decoration naming a
removed id (the map's other entries untouched).

  python -I tools/scripts/data/removed_decorations.py [file.json ...]   (default: content/data/editor-catalog.json)

Every key and list entry naming a removed id is dropped, at any depth (used once on the committed catalog). export_editor_catalog.ts
and export_board.ts leave the same ids out themselves, so a re-export does not bring them back.
"""

import json
import sys

REMOVED = {
    'wall-3d-low',
    'ember-extra-broken-cart-wheel',
    'ember-nature-dry-bush',
    'ember-nature-dry-branch-pile',
    'prop-3d-vau-stone-v01',
    'prop-3d-vau-well-v01',
    'ember-dungeon-broken-arch',
    'ember-winter-snow-boulder',
}
FILES = ['content/data/editor-catalog.json']


def scrub(v):
    if isinstance(v, dict):
        return {k: scrub(x) for k, x in v.items() if k not in REMOVED and not (isinstance(x, dict) and x.get('id') in REMOVED)}
    if isinstance(v, list):
        return [scrub(x) for x in v if not (isinstance(x, str) and x in REMOVED) and not (isinstance(x, dict) and (x.get('id') in REMOVED or x.get('file') in REMOVED))]
    return v


def main():
    for path in sys.argv[1:] or FILES:
        raw = open(path, encoding='utf-8').read()
        data = json.loads(raw)
        clean = scrub(data)
        if '\n' in raw.strip():
            out = json.dumps(clean, ensure_ascii=False, indent=2)
        else:  # one line: keep the file's own separators
            out = json.dumps(clean, ensure_ascii=False, separators=(', ', ': ') if '", "' in raw[:4000] else (',', ':'))
        open(path, 'w', encoding='utf-8', newline='\n').write(out + ('\n' if raw.endswith('\n') else ''))
        left = sum(out.count(f'"{r}"') for r in REMOVED)
        print(f'{path}: {len(raw) - len(out)} bytes removed, {left} mentions left')


if __name__ == '__main__':
    main()
