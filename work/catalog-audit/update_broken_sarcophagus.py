import pathlib,re,json
root=pathlib.Path(r'C:\EmberEngine')
rel='content/models/models.json'; p=root/rel; raw=p.read_bytes(); text=raw.decode('utf-8')
pat=r'\{\s*"id": "ember-crypt-sarcophagus-broken-lid",.*?\n    \}'
m=re.search(pat,text,re.S)
if not m:raise SystemExit('Target model entry not found')
block=m.group(0)
changes=[
 (r'("file": )"[^"]+"',r'\1"/game/ember/crypt/sarcophagus-broken-lid-v03.glb"'),
 (r'("height": )[0-9.]+',r'\g<1>1.682'),
 (r'("width": )[0-9.]+',r'\g<1>3.113'),
 (r'("triangles": )[0-9]+',r'\g<1>3320'),
]
for pat,repl in changes:
 block,n=re.subn(pat,repl,block,count=1)
 if n!=1:raise SystemExit(f'Expected one field match for {pat}, got {n}')
text=text[:m.start()]+block+text[m.end():]
json.loads(text); p.write_bytes(text.encode('utf-8')); print('Updated models.json exact entry.')
readme=root/'content/models/README.md'; data=readme.read_bytes(); old='| ember-crypt-sarcophagus-broken-lid | Sarcófago com tampa quebrada 3D | crypt/sarcophagus-broken-lid-v01.glb | 3.38 × 1.68 | 3364 | pair |'.encode('utf-8'); new='| ember-crypt-sarcophagus-broken-lid | Sarcófago com tampa quebrada 3D | crypt/sarcophagus-broken-lid-v03.glb | 3.11 × 1.68 | 3320 | pair |'.encode('utf-8')
if data.count(old)!=1:raise SystemExit(f'Expected one README row, found {data.count(old)}')
readme.write_bytes(data.replace(old,new,1)); print('Updated README exact row.')
