import json,struct,pathlib
p=pathlib.Path(r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v02.glb');b=p.read_bytes();off=12
while off<len(b):
 n,t=struct.unpack_from('<II',b,off);off+=8;c=b[off:off+n];off+=n
 if t==0x4e4f534a:d=json.loads(c);break
print(json.dumps({'images':d.get('images'),'textures':d.get('textures'),'materials':d.get('materials'),'meshes':d.get('meshes')},ensure_ascii=False,indent=2)[:6000])
