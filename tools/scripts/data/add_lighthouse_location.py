"""The Light House on the world map (2026-11-11): the pin `lighthouse` already stands at hex (1, 3) with no mission; this gives it
the five maps of tools/scripts/maps/make_lighthouse.py.

  python -I tools/scripts/data/add_lighthouse_location.py

Writes content/data/overworld.json (the location: its pin kept as it is, the mission `farol`, the floors as submaps, open access like
the Mosteiro), content/data/campaign.json (the location + the five missions, texts from the newest map drafts, one index, `mapFile`
= the highest serial now) and content/data/editor-catalog.json (editor.locations + editor.locais.submaps). Idempotent.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')
LOC_ID = 'lighthouse'
ENTRY = 'farol'
FLOORS = [('farol-andar-2', 2), ('farol-topo', 3), ('farol-poroes', -1), ('farol-gruta', -2)]
MISSIONS = [ENTRY] + [m for m, _ in FLOORS]


def newest(map_id):
    best, file = -1, None
    for f in os.listdir(MAPS):
        if f.startswith(map_id) and len(f) == len(map_id) + 8 and f[len(map_id):-5].isdigit() and int(f[len(map_id):-5]) > best:
            best, file = int(f[len(map_id):-5]), f
    if not file:
        raise SystemExit(f'no map file for {map_id}')
    return file, json.load(open(os.path.join(MAPS, file), encoding='utf-8'))['draft']


def load(path):
    text = open(path, encoding='utf-8').read()
    m = re.search(r'\n( +)"', text)
    return json.loads(text), (len(m.group(1)) if m else None), text.endswith('\n')


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


submaps = [{'missionId': m, 'floor': f} for m, f in FLOORS]

p = os.path.join(ROOT, 'content', 'data', 'overworld.json')
o, ind, nl = load(p)
pin = next((l for l in o['locations'] if l['id'] == LOC_ID), None)
if pin is None:
    raise SystemExit('overworld.json has no lighthouse pin')
pin.update({'missionIds': [ENTRY], 'openAccess': True, 'submaps': submaps})
save(p, o, ind, nl)
print('overworld.json: lighthouse at hex', (pin['col'], pin['row']), '->', ENTRY)

p = os.path.join(ROOT, 'content', 'data', 'campaign.json')
c, ind, nl = load(p)
print('campaign.json location', put(c['locations'], {k: pin[k] for k in ('id', 'name', 'x', 'y', 'missionIds', 'openAccess', 'submaps', 'col', 'row')}))
index = max(m.get('index', 0) for m in c['missions']) + 1
existing = [m.get('index') for m in c['missions'] if m['id'] in MISSIONS]
if existing:
    index = min(existing)
for mid in MISSIONS:
    file, d = newest(mid)
    entry = {'id': mid, 'index': index, 'title': d.get('title', mid), 'place': d.get('place', ''), 'briefing': d.get('briefing', ''),
             'objective': d.get('objective', ''), 'win': d.get('win', 'rout'), 'mapFile': file}
    print('campaign.json mission', mid, put(c['missions'], entry), 'index', index, 'map', file)
save(p, c, ind, nl)

p = os.path.join(ROOT, 'content', 'data', 'editor-catalog.json')
cat, ind, nl = load(p)
e = cat['editor']
print('editor locations', put(e.setdefault('locations', []), {'id': LOC_ID, 'name': pin['name'], 'missionIds': [ENTRY]}))
e.setdefault('locais', {}).setdefault('submaps', {})[LOC_ID] = submaps
save(p, cat, ind, nl)
