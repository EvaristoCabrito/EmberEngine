"""A test draft for the gothic parapet kit: thebridge023 with its parapet cards replaced by runs of the 3D pieces on the two
parapet rows (pilaster, window, guardian, window, every four hexes), plus a short free-standing L at the west end to show a corner
and a run end. Written to build/engine2-test/maps/thebridge-gothic.json (the test pipeline runs on it); content/ untouched."""
import json
import sys

src = 'C:/EmberEngine/content/maps/ember/thebridge023.json'
out = 'C:/EmberEngine/build/engine2-test/maps/thebridge-gothic-src.json'
doc = json.load(open(src, encoding='utf-8'))
d = doc['draft']
d['id'] = 'thebridge-gothic'
d['title'] = 'Ponte gótica (teste)'
decos = [x for x in d['decorations'] if not x['id'].startswith('bridge-parapet')]
cols, rows = d['cols'], d['rows']


def piece(x, y, kind):
    return {'id': f'ember-arch-gothic-{kind}', 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal'}


for y in (0, rows - 1):
    for x in range(1, cols - 1):
        k = (x - 1) % 4
        decos.append(piece(x, y, 'pier' if k == 0 else 'niche' if k == 2 else 'wall'))
# a corner + a run end inside the deck (rows 2..5 at the east end), well away from the spawns at the west
for y in range(2, 5):
    decos.append({'id': 'ember-arch-gothic-wall', 'x': cols - 4, 'y': y, 'rot': 1, 'wallOrientation': 'vertical'})
for x in range(cols - 4, cols - 1):
    decos.append(piece(x, 5, 'wall'))
d['decorations'] = decos
json.dump(doc, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
print(out, len(decos), 'decorations')
