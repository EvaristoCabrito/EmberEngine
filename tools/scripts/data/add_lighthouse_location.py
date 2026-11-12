"""Keep the Lighthouse's location and campaign missions aligned with its newest maps.

  python -I tools/scripts/data/add_lighthouse_location.py

The current project stores world-map pins in content/data/locations.json, mission
records in content/data/campaign.json, and editor mission groups in
content/data/editor-catalog.json. This updater preserves the pin's established
position and only refreshes the Lighthouse's mission links, mission text, and
mapFile values. It is safe to run after creating a new serial of any Farol map.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')
LOCATION_FILE = os.path.join(ROOT, 'content', 'data', 'locations.json')
CAMPAIGN_FILE = os.path.join(ROOT, 'content', 'data', 'campaign.json')
CATALOG_FILE = os.path.join(ROOT, 'content', 'data', 'editor-catalog.json')
LOC_ID = 'lighthouse'
ENTRY = 'farol'
FLOORS = [('farol-andar-2', 2), ('farol-topo', 3), ('farol-poroes', -1), ('farol-gruta', -2)]
MISSIONS = [ENTRY] + [mission_id for mission_id, _ in FLOORS]


def newest(map_id):
    best, filename = -1, None
    for name in os.listdir(MAPS):
        stem, ext = os.path.splitext(name)
        if ext == '.json' and stem.startswith(map_id):
            serial = stem[len(map_id):]
            if serial.isdigit() and int(serial) > best:
                best, filename = int(serial), name
    if not filename:
        raise SystemExit(f'no map file for {map_id}')
    with open(os.path.join(MAPS, filename), encoding='utf-8') as source:
        draft = json.load(source)['draft']
    return filename, draft


def read(path):
    with open(path, encoding='utf-8') as source:
        text = source.read()
    match = re.search(r'\n( +)"', text)
    return json.loads(text), (len(match.group(1)) if match else None), text.endswith('\n')


def write(path, data, indent, newline):
    with open(path, 'w', encoding='utf-8') as target:
        json.dump(data, target, ensure_ascii=False, indent=indent,
                  separators=(',', ': ') if indent else (',', ':'))
        if newline:
            target.write('\n')


locations, indent, newline = read(LOCATION_FILE)
pin = next((item for item in locations['locations'] if item.get('id') == LOC_ID), None)
if pin is None:
    raise SystemExit('locations.json has no Lighthouse pin')
pin.update({
    'missionIds': [ENTRY],
    'openAccess': True,
    'submaps': [{'missionId': mission_id, 'floor': floor} for mission_id, floor in FLOORS],
})
write(LOCATION_FILE, locations, indent, newline)
print('locations.json: refreshed Lighthouse mission links; kept its position at', (pin['col'], pin['row']))

campaign, indent, newline = read(CAMPAIGN_FILE)
by_id = {mission.get('id'): mission for mission in campaign['missions']}
next_index = max((mission.get('index', 0) for mission in campaign['missions']), default=0) + 1
for mission_id in MISSIONS:
    filename, draft = newest(mission_id)
    mission = by_id.get(mission_id)
    if mission is None:
        mission = {'id': mission_id, 'index': next_index}
        next_index += 1
        campaign['missions'].append(mission)
    for key in ('title', 'place', 'briefing', 'objective', 'win'):
        mission[key] = draft.get(key, mission.get(key, ''))
    mission['mapFile'] = filename
    print('campaign.json:', mission_id, '->', filename)
write(CAMPAIGN_FILE, campaign, indent, newline)

catalog, indent, newline = read(CATALOG_FILE)
editor_locations = catalog.get('editor', {}).get('locais', {})
order = editor_locations.get('order')
location_order = editor_locations.get('locationOrder')
if isinstance(order, dict) and isinstance(location_order, list):
    order.setdefault(LOC_ID, [ENTRY])
    if LOC_ID not in location_order:
        location_order.append(LOC_ID)
    write(CATALOG_FILE, catalog, indent, newline)
    print('editor-catalog.json: included the Lighthouse mission group')
