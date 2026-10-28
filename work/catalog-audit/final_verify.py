import json,pathlib,struct
root=pathlib.Path(r'C:\EmberEngine'); target='ember-extra-grave-marker-v03'
for rel in ['content/models/models.json','content/data/editor-catalog.json','content/maps/ember/cemiterio-esquecidos007.json','content/maps/ember/cemiterio-esquecidos008.json']:
 p=root/rel; data=json.loads(p.read_bytes().decode('utf-8')); print('JSON_OK',rel)
for rel in ['content/maps/ember/cemiterio-esquecidos006.json','content/maps/ember/cemiterio-esquecidos007.json','content/maps/ember/cemiterio-esquecidos008.json','content/models/models.json','content/data/editor-catalog.json']:
 raw=(root/rel).read_bytes().decode('utf-8'); print('TARGET_COUNT',rel,raw.count(target))
print('SOURCE_RETAINED',(root/'content/models/extra/grave-marker-v03.glb').exists())
