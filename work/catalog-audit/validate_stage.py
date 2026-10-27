import json,pathlib
root=pathlib.Path(r'C:\EmberEngine\work\catalog-audit\stage')
for n in ['cemiterio-esquecidos008.json','models.json','editor-catalog.json']:
 d=json.loads((root/n).read_text(encoding='utf-8')); print(n,'valid json')
 for target in ['ember-extra-grave-marker-v03','/game/ember/extra/grave-marker-v03.glb']:
  text=(root/n).read_text(encoding='utf-8');
  if target in text: raise SystemExit(f'residual {target} in {n}')
 print('  rejected id absent')
