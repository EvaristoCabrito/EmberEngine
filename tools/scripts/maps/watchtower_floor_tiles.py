"""The watchtower's other floors on the user's new floor tiles (2026-10-21, Attachments/watchtower): each floor's map again as the
next version (serial + 1, the campaign plays it; the old file stays), with nothing changed but the ground set it picks
(GroundAssets.h: "torre-v01" for the tower's own floors, "prisao-v01" for the prison).

  python -I tools/scripts/maps/watchtower_floor_tiles.py
"""

import json

FLOORS = [  # (old map, new map, tile set)
    ('content/maps/ember/watchtower-barracks001.json', 'content/maps/ember/watchtower-barracks002.json', 'torre-v01'),
    ('content/maps/ember/watchtower-command001.json', 'content/maps/ember/watchtower-command002.json', 'torre-v01'),
    ('content/maps/ember/watchtower-beacon001.json', 'content/maps/ember/watchtower-beacon002.json', 'torre-v01'),
    ('content/maps/ember/watchtower-prison002.json', 'content/maps/ember/watchtower-prison003.json', 'prisao-v01'),
]

for src, out, tiles in FLOORS:
    m = json.load(open(src, encoding='utf-8'))
    m['draft']['emberTiles'] = tiles
    m['serial'] = m.get('serial', 1) + 1
    m['savedAt'] = m.get('savedAt', 0) + 1
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(m, f, ensure_ascii=False)
    print(f'{out}: serial {m["serial"]}, {tiles}')
