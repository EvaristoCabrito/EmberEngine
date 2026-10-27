import json,pathlib
files=[r'content/maps/ember/cemiterio-esquecidos007.json',r'content/maps/ember/cemiterio-esquecidos008.json',r'content/models/models.json',r'content/engine2/editor-catalog.json']
target='ember-extra-grave-marker-v03'
def walk(x,path='$'):
 if isinstance(x,dict):
  for k,v in x.items():
   if k==target: print('KEY',path+'.'+k)
   walk(v,path+'.'+k)
 elif isinstance(x,list):
  for i,v in enumerate(x): walk(v,f'{path}[{i}]')
 elif x==target: print('VALUE',path)
for f in files:
 p=pathlib.Path(f); data=json.loads(p.read_text(encoding='utf-8')); print('\nFILE',f); walk(data)
