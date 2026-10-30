"""The Monastery on the world map (2026-10-29; the user: "from the farmlands, im moving twice to the direct left hex [then: 'a direita
falei errado'], you can Pin the Monastery near the edge as the image shows, you will them need to update locais and in game"):
hex (3, 5) = two hexes east of the Farm (1, 5), on its row.

  python -I tools/scripts/data/add_monastery_location.py

Writes the location into content/data/overworld.json (the pin, open access, the floors as submaps) and content/data/campaign.json
(the location + the three missions: mosteiro, mosteiro-andar-2, mosteiro-andar-3, texts from the newest map drafts, one index like
the Inn's floors share theirs), and into content/data/editor-catalog.json editor.locations + editor.locais.submaps (the Map Editor's
Locais). Idempotent: an entry already there is replaced.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')
LOC_ID, NAME = 'mosteiro', 'Mosteiro das Cinzas'
COL, ROW = 3, 5
HEX_SIZE = 5.0
X = round((COL + (0.5 if ROW % 2 else 0.0)) * HEX_SIZE * 3 ** 0.5, 2)  # overworld.ts hexToWorld: odd rows half a hex east
Y = round(ROW * HEX_SIZE * 1.5, 2)
FLOORS = [('mosteiro-andar-2', 2), ('mosteiro-andar-3', 3), ('mosteiro-masmorras', -1), ('mosteiro-laboratorio', -2)]
MISSIONS = ['mosteiro', 'mosteiro-andar-2', 'mosteiro-andar-3', 'mosteiro-masmorras', 'mosteiro-laboratorio']


def newest(map_id):
    best, file = -1, None
    for f in os.listdir(MAPS):
        if f.startswith(map_id) and len(f) == len(map_id) + 8 and f[len(map_id):-5].isdigit() and int(f[len(map_id):-5]) > best:
            best, file = int(f[len(map_id):-5]), f
    if not file:
        raise SystemExit(f'no map file for {map_id}')
    return file, json.load(open(os.path.join(MAPS, file), encoding='utf-8'))['draft']


def indent_of(text):
    m = re.search(r'\n( +)"', text)
    return len(m.group(1)) if m else None


def load(path):
    text = open(path, encoding='utf-8').read()
    return json.loads(text), indent_of(text), text.endswith('\n')


def save(path, data, indent, newline):
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=indent, separators=(',', ': ') if indent else (',', ':'))
        if newline:
            f.write('\n')


def put(lst, entry, key='id'):
    for i, e in enumerate(lst):
        if e.get(key) == entry[key]:
            lst[i] = entry
            return 'replaced'
    lst.append(entry)
    return 'added'


location = {'id': LOC_ID, 'name': NAME, 'x': X, 'y': Y, 'missionIds': ['mosteiro'], 'openAccess': True,
            'submaps': [{'missionId': m, 'floor': f} for m, f in FLOORS], 'col': COL, 'row': ROW}

# overworld.json
p = os.path.join(ROOT, 'content', 'data', 'overworld.json')
o, ind, nl = load(p)
print('overworld.json location', put(o['locations'], location))
save(p, o, ind, nl)

# campaign.json: the location and the missions
p = os.path.join(ROOT, 'content', 'data', 'campaign.json')
c, ind, nl = load(p)
print('campaign.json location', put(c['locations'], location))
index = max(m.get('index', 0) for m in c['missions']) + 1
existing = {m['id']: m.get('index') for m in c['missions'] if m['id'] in MISSIONS}
if existing:
    index = min(existing.values())
for mid in MISSIONS:
    file, d = newest(mid)
    entry = {'id': mid, 'index': index, 'title': d.get('title', mid), 'place': d.get('place', ''), 'briefing': d.get('briefing', ''),
             'objective': d.get('objective', ''), 'win': d.get('win', 'rout'), 'mapFile': file}
    print('campaign.json mission', mid, put(c['missions'], entry), 'index', index, 'map', file)
save(p, c, ind, nl)

# editor-catalog.json: the editor's Locais
p = os.path.join(ROOT, 'content', 'data', 'editor-catalog.json')
cat, ind, nl = load(p)
e = cat['editor']
e.setdefault('locations', [])
print('editor locations', put(e['locations'], {'id': LOC_ID, 'name': NAME, 'missionIds': ['mosteiro']}))
e.setdefault('locais', {}).setdefault('submaps', {})[LOC_ID] = [{'missionId': m, 'floor': f} for m, f in FLOORS]
save(p, cat, ind, nl)
print(f'Mosteiro at hex ({COL}, {ROW}), pin x {X} y {Y}')
