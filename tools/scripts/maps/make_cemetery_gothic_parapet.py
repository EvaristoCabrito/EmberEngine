"""Cemitério dos Esquecidos 011: 010 with its south wall of parapet cards (bridge-parapet-gothic-statues-001 / -wall-001, row 46,
each card over four columns: 0..27 and 32..59, the gate open at 28..31) replaced by A Ponte de Pedra's gothic parapet in 3D (the user,
2026-11-03: "você precisa usar o mesmo parapeito da Stone Bridge no lugar desse muro do cemitério"). The same run as thebridge024
(make_bridge_gothic.py): a pilaster every four hexes, the guardian's niche between two window panels (pier, wall, niche, wall, ...),
the run ends getting their pilasters by themselves, the guardians facing the board by themselves (MapStage). Everything else is 010's.
Writes content/maps/ember/cemiterio-esquecidos011.json (serial 11: the campaign plays the highest serial; 010 untouched).

  python -I tools/scripts/maps/make_cemetery_gothic_parapet.py [src.json] [out.json]
"""
import json
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'content/maps/ember/cemiterio-esquecidos010.json'
out = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/cemiterio-esquecidos011.json'
doc = json.load(open(src, encoding='utf-8'))
d = doc['draft']
cards = [x for x in d['decorations'] if x['id'].startswith('bridge-parapet')]
decos = [x for x in d['decorations'] if not x['id'].startswith('bridge-parapet')]
rows = {c['y'] for c in cards}
assert len(rows) == 1, f'parapet cards on rows {rows}'
row = rows.pop()
columns = sorted({c['x'] + i for c in cards for i in range(4)})  # each card covers its anchor and the three columns east of it
taken = {(x['x'], x['y']) for x in decos}
assert not any((x, row) in taken for x in columns), 'something already stands on the wall row'
for x in columns:
    k = (x - 1) % 4
    kind = 'pier' if k == 0 else 'niche' if k == 2 else 'wall'
    decos.append({'id': f'ember-arch-gothic-{kind}', 'x': x, 'y': row, 'rot': 0, 'wallOrientation': 'horizontal'})
d['decorations'] = decos
doc['serial'] = 11
doc['savedAt'] = doc.get('savedAt', 0) + 1
json.dump(doc, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
gaps = [x for x in range(d['cols']) if x not in columns]
print(f'{out}: {len(cards)} parapet cards -> {len(columns)} gothic pieces on row {row}; open columns {gaps}')
