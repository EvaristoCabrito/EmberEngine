import json

with open('build/campaign/scenes/wisp-forest-crossing034.scene.json', 'r', encoding='utf-8') as f:
    d = json.load(f)

units = d.get('units', [])
print(f'Units in scene: {len(units)}')
for i, u in enumerate(units):
    print(f"{i}: {u.get('name')} class={u.get('classId')} pos=({u.get('x')},{u.get('y')}) side={u.get('side')}")
