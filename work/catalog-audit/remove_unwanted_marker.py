import json,pathlib
from json import JSONDecoder
ROOT=pathlib.Path(r'C:\EmberEngine')
TARGET='ember-extra-grave-marker-v03'
DEC=JSONDecoder()
def ws(s,p):
 while p<len(s) and s[p].isspace(): p+=1
 return p
def member_span(s,obj_start,key):
 assert s[obj_start]=='{', (key,s[obj_start:obj_start+30])
 p=obj_start+1
 while True:
  p=ws(s,p)
  if s[p]=='}': raise KeyError(key)
  ks=p; k,p=DEC.raw_decode(s,p); p=ws(s,p); assert s[p]==':'; p=ws(s,p+1); vs=p; _,ve=DEC.raw_decode(s,p)
  if k==key:return ks,vs,ve
  p=ws(s,ve)
  if s[p]==',':p+=1
  elif s[p]=='}':raise KeyError(key)
  else:raise ValueError(f'bad object separator at {p}: {s[p:p+20]}')
def path_span(s,path):
 start=0
 for key in path:
  _,vs,ve=member_span(s,start,key); start=vs
 return start,ve
def remove_array(s,span,predicate):
 a,b=span; assert s[a]=='[' and s[b-1]==']'
 p=ws(s,a+1); elems=[]
 while p<b-1:
  st=p; _,en=DEC.raw_decode(s,p); elems.append((st,en,s[st:en])); p=ws(s,en)
  if p<b-1:
   assert s[p]==','; p=ws(s,p+1)
 if not elems:return s,0
 kept=[x for x in elems if not predicate(json.loads(x[2]))]
 removed=len(elems)-len(kept)
 if not removed:return s,0
 lead=s[a+1:elems[0][0]]; trail=s[elems[-1][1]:b-1]
 sep=s[elems[0][1]:elems[1][0]] if len(elems)>1 else ''
 rebuilt='['+lead+sep.join(x[2] for x in kept)+trail+']'
 return s[:a]+rebuilt+s[b:],removed
def remove_obj_member(s,obj_span,key):
 a,b=obj_span; assert s[a]=='{' and s[b-1]=='}'
 ks,vs,ve=member_span(s,a,key)
 # Find the comma that separates this member from its predecessor/successor.
 before=ks-1
 while before>a and s[before].isspace(): before-=1
 after=ws(s,ve)
 if before>a and s[before]==',':
  start=before
  end=ve
  # Keep any whitespace before the following member.
  return s[:start]+s[end:],1
 if s[after]==',':
  end=after+1
  return s[:ks]+s[end:],1
 # Sole property (not expected for these catalog objects).
 return s[:ks]+s[ve:],1
changes={}
# Remove placements only from the two map saves that contain the rejected v03.
for rel in [r'content/maps/ember/cemiterio-esquecidos007.json',r'content/maps/ember/cemiterio-esquecidos008.json']:
 p=ROOT/rel; s=p.read_bytes().decode('utf-8'); arr=path_span(s,['draft','decorations']); s,n=remove_array(s,arr,lambda x:isinstance(x,dict) and x.get('id')==TARGET)
 if not n:raise RuntimeError(f'No placements removed from {rel}')
 json.loads(s); changes[rel]=(s,n)
# Remove exact registry entry.
rel=r'content/models/models.json'; p=ROOT/rel; s=p.read_bytes().decode('utf-8'); arr=path_span(s,['models']); s,n=remove_array(s,arr,lambda x:isinstance(x,dict) and x.get('id')==TARGET)
if n!=1:raise RuntimeError(f'Expected one model entry, removed {n}')
json.loads(s); changes[rel]=(s,n)
# Remove this key from the editor catalog's three indexes and the ordering list.
rel=r'content/data/editor-catalog.json'; p=ROOT/rel; s=p.read_bytes().decode('utf-8'); removed={}
for path in [['decorations'],['editor','decorationInfo'],['editor','decoMeta']]:
 a,b=path_span(s,path); s,n=remove_obj_member(s,(a,b),TARGET); removed['.'.join(path)]=n
arr=path_span(s,['editor','architectureOrder']); s,n=remove_array(s,arr,lambda x:x==TARGET); removed['editor.architectureOrder']=n
if sum(removed.values())!=4:raise RuntimeError(f'Unexpected editor catalog removals: {removed}')
json.loads(s); changes[rel]=(s,sum(removed.values()))
# Write only after every edited document has parsed successfully.
for rel,(s,n) in changes.items(): (ROOT/rel).write_bytes(s.encode('utf-8')); print(f'{rel}: removed {n}')
print('Preserved content/models/extra/grave-marker-v03.glb and its .blend source.')

