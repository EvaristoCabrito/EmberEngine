"""A Ponte de Pedra 024: thebridge023 with its parapet cards (bridge-parapet-tall-001 / -tall-statues-001) replaced by the gothic
parapet kit in 3D (the user, 2026-10-21: "Faz a thebridge024 com os parapeitos 3D (kit gótico) no lugar das cartas, a 023 fica como
está"). Both parapet rows (0 and the last) are runs of ember-arch-gothic-* pieces over the whole width: a pilaster every four hexes,
the guardian's niche between two window panels (pier, wall, niche, wall, ...); the run ends get their pilasters by themselves. The
guardians face the deck by themselves (MapStage). Everything else is 023's. Writes content/maps/ember/thebridge024.json (serial 24: the
campaign plays the highest serial; 023 untouched).

  python -I tools/scripts/maps/make_bridge_gothic.py [content/maps/ember/thebridge023.json] [content/maps/ember/thebridge024.json]
"""
import json
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'content/maps/ember/thebridge023.json'
out = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/thebridge024.json'
doc = json.load(open(src, encoding='utf-8'))
d = doc['draft']
cols, rows = d['cols'], d['rows']
cards = [x for x in d['decorations'] if x['id'].startswith('bridge-parapet')]
decos = [x for x in d['decorations'] if not x['id'].startswith('bridge-parapet')]
for y in (0, rows - 1):
    for x in range(cols):
        k = (x - 1) % 4
        kind = 'pier' if k == 0 else 'niche' if k == 2 else 'wall'
        decos.append({'id': f'ember-arch-gothic-{kind}', 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal'})
d['decorations'] = decos
doc['serial'] = 24
doc['savedAt'] = doc.get('savedAt', 0) + 1
json.dump(doc, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
print(f'{out}: {len(cards)} parapet cards -> {2 * cols} gothic pieces on rows 0 and {rows - 1}; {len(decos)} decorations')
