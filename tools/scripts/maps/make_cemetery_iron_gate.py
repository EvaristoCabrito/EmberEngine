"""Cemitério dos Esquecidos 012: 011 with the 3D iron gate in the wall's opening (the user, 2026-11-03: "we have a new 3D iron fences
gate you should use", then "Open, like the doors"). The opening is four hexes (28..31) and the gate three (ember-polyhaven-large-iron-
gate, footprint dx -1..1): the gate on 29..31 (anchor 30), the parapet run carried one panel further to 28 (its end gets its
pilaster by itself). The exit waypoint at (29, 47) is just outside the gate. The gate opens for anyone beside it (MapStage
kModelGates). Writes content/maps/ember/cemiterio-esquecidos012.json (serial 12; 011 untouched).

  python -I tools/scripts/maps/make_cemetery_iron_gate.py [src.json] [out.json]
"""
import json
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'content/maps/ember/cemiterio-esquecidos011.json'
out = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/cemiterio-esquecidos012.json'
doc = json.load(open(src, encoding='utf-8'))
d = doc['draft']
row = 46
taken = {(x['x'], x['y']) for x in d['decorations']}
for x in range(28, 32):
    assert (x, row) not in taken, f'({x}, {row}) is not open'
d['decorations'].append({'id': 'ember-arch-gothic-wall', 'x': 28, 'y': row, 'rot': 0, 'wallOrientation': 'horizontal'})
d['decorations'].append({'id': 'ember-polyhaven-large-iron-gate', 'x': 30, 'y': row, 'rot': 0})
doc['serial'] = 12
doc['savedAt'] = doc.get('savedAt', 0) + 1
json.dump(doc, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
print(f'{out}: the iron gate on (29..31, {row}), a parapet panel on (28, {row})')
