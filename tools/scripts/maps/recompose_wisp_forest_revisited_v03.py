"""Space the new dead trees around the playable clearing while preserving every existing map item."""
import copy, json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'content/maps/ember/wisp-forest009.json'
OUT=ROOT/'content/maps/ember/wisp-forest010.json'
POSITIONS={
    'ember-wisp-revisited-giant-dead-oak-v02':(9,19),
    'ember-wisp-revisited-broken-crown-v02':(6,10),
    'ember-wisp-revisited-spiral-twisted-v02':(13,8),
    'ember-wisp-revisited-lightning-split-v02':(3,17),
    'ember-wisp-revisited-uprooted-leaning-v02':(18,2),
    'ember-wisp-revisited-root-arch-v02':(14,17),
    'ember-wisp-revisited-hollow-snag-v02':(1,2),
    'ember-wisp-revisited-weeping-willow-v02':(10,2),
    'ember-wisp-revisited-tall-burnt-pine-v02':(4,5),
    'ember-wisp-revisited-broken-stump':(13,3),
    'ember-wisp-revisited-rotted-hollow-log':(5,14),
    'ember-wisp-revisited-roots-and-fungi':(15,4),
}

def main():
    if OUT.exists(): raise SystemExit(f'preserving existing output: {OUT}')
    data=copy.deepcopy(json.loads(SOURCE.read_text(encoding='utf-8')))
    data['serial']=int(data.get('serial',0))+1; data['savedAt']='2026-10-24T00:00:00.000Z'
    d=data['draft']; d['id']='wisp-forest-revisited-v03'
    valid={(i%d['cols'],i//d['cols']) for i,t in enumerate(d['tiles']) if t!='void'}
    found=set()
    for item in d['decorations']:
        if item['id'] not in POSITIONS: continue
        x,y=POSITIONS[item['id']]
        if (x,y) not in valid: raise SystemExit(f'{item["id"]} is outside playable outline at {x},{y}')
        item['x']=x; item['y']=y; found.add(item['id'])
    if found != set(POSITIONS): raise SystemExit(f'map is missing expected new objects: {set(POSITIONS)-found}')
    # Retain the original irregular mask, themed tiles, original contents and all authored spawns.
    OUT.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'created {OUT.relative_to(ROOT)}; repositioned only this revision’s nine trees and three props')

if __name__=='__main__': main()
