import json,pathlib
from json import JSONDecoder
ROOT=pathlib.Path(r'C:\EmberEngine'); STAGE=ROOT/'work/catalog-audit/stage'
STAGE.mkdir(exist_ok=True); TARGET='ember-extra-grave-marker-v03'; DEC=JSONDecoder()
def ws(s,p):
 while p<len(s) and s[p].isspace():p+=1
 return p
def member_span(s,obj_start,key):
 assert s[obj_start]=='{'; p=obj_start+1
 while True:
  p=ws(s,p)
  if s[p]=='}':raise KeyError(key)
  kstart=p; k,p=DEC.raw_decode(s,p); p=ws(s,p); assert s[p]==':'; p=ws(s,p+1); vs=p; _,ve=DEC.raw_decode(s,p)
  if k==key:return kstart,vs,ve
  p=ws(s,ve)
  if s[p]==',':p+=1
  elif s[p]=='}':raise KeyError(key)
  else:raise ValueError(f'bad separator at {p}')
def path_span(s,path):
 start=0
 for key in path:_,start,_=member_span(s,start,key)
 # Correct final value start/end by resolving the last member once.
 parent=0
 for key in path[:-1]:_,parent,_=member_span(s,parent,key)
 _,a,b=member_span(s,parent,path[-1]); return a,b
def remove_array(s,span,pred):
 a,b=span; assert s[a]=='[' and s[b-1]==']'; p=ws(s,a+1); items=[]
 while p<b-1:
  st=p; _,en=DEC.raw_decode(s,p); items.append((st,en,s[st:en])); p=ws(s,en)
  if p<b-1:assert s[p]==','; p=ws(s,p+1)
 if not items:return s,0
 kept=[it for it in items if not pred(json.loads(it[2]))]; removed=len(items)-len(kept)
 if not removed:return s,0
 lead=s[a+1:items[0][0]]; trail=s[items[-1][1]:b-1]; sep=s[items[0][1]:items[1][0]] if len(items)>1 else ''
 return s[:a]+'['+lead+sep.join(x[2] for x in kept)+trail+']'+s[b:],removed
def remove_obj_member(s,obj_span,key):
 a,b=obj_span; ks,vs,ve=member_span(s,a,key); before=ks-1
 while before>a and s[before].isspace():before-=1
 after=ws(s,ve)
 if before>a and s[before]==',':return s[:before]+s[ve:],1
 if s[after]==',':return s[:ks]+s[after+1:],1
 return s[:ks]+s[ve:],1
changes={}
for rel in ['content/maps/ember/cemiterio-esquecidos007.json','content/maps/ember/cemiterio-esquecidos008.json']:
 s=(ROOT/rel).read_bytes().decode('utf-8'); a=path_span(s,['draft','decorations']); s,n=remove_array(s,a,lambda x:isinstance(x,dict) and x.get('id')==TARGET); json.loads(s); changes[rel]=s; print(rel,'placements removed=',n)
rel='content/models/models.json'; s=(ROOT/rel).read_bytes().decode('utf-8'); a=path_span(s,['models']); s,n=remove_array(s,a,lambda x:isinstance(x,dict) and x.get('id')==TARGET)
if n!=1:raise RuntimeError(f'model registry expected 1, removed {n}')
json.loads(s); changes[rel]=s; print(rel,'registry entries removed=',n)
rel='content/data/editor-catalog.json'; s=(ROOT/rel).read_bytes().decode('utf-8')
for path in [['decorations'],['editor','decorationInfo'],['editor','decoMeta']]:
 a=path_span(s,path); s,n=remove_obj_member(s,(a[0],a[1]),TARGET)
 if n!=1:raise RuntimeError(f'catalog key missing: {path}')
 print('catalog key removed=','.'.join(path))
a=path_span(s,['editor','architectureOrder']); s,n=remove_array(s,a,lambda x:x==TARGET)
if n!=1:raise RuntimeError(f'architecture order expected 1, removed {n}')
json.loads(s); changes[rel]=s; print('editor.architectureOrder entries removed=',n)
for rel,s in changes.items():
 if TARGET in s:raise RuntimeError(f'residual target in {rel}')
 out=STAGE/pathlib.Path(rel).name; out.write_bytes(s.encode('utf-8')); print('STAGED',rel,'->',out,'bytes',out.stat().st_size)

