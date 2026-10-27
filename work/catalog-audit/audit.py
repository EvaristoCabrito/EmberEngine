import json,struct,pathlib
root=pathlib.Path(r'C:\EmberEngine')
data=json.loads((root/'content/models/models.json').read_text(encoding='utf-8'))
def tris(glb):
 b=glb.read_bytes(); off=12; js=None
 while off<len(b):
  n,t=struct.unpack_from('<II',b,off); off+=8; chunk=b[off:off+n]; off+=n
  if t==0x4E4F534A: js=json.loads(chunk.decode('utf-8')); break
 if not js:return None,0
 access=js.get('accessors',[]); total=0
 for mesh in js.get('meshes',[]):
  for p in mesh.get('primitives',[]):
   mode=p.get('mode',4); count=access[p['indices']]['count'] if 'indices' in p else access[p['attributes']['POSITION']]['count']
   if mode==4: total+=count//3
   elif mode in (5,6): total+=max(0,count-2)
 return total,len(js.get('images',[]))
rows=[]
for m in data['models']:
 path=m.get('file','').replace('/game/ember/','content/models/')
 f=root/path
 if f.exists():
  t,imgs=tris(f); rows.append((t or 0,imgs,m['id'],m['name'],str(f.relative_to(root))))
for t,imgs,i,n,p in sorted(rows):
 if t<=2100: print(f'{t:5} tris | {imgs} images | {i} | {n} | {p}')
print(f'COUNT={sum(1 for r in rows if r[0]<=2100)} TOTAL={len(rows)} MISSING={len(data["models"])-len(rows)}')
