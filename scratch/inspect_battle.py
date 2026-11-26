import json

with open('build/campaign/battles/wisp-forest-crossing034.battle.json', 'r', encoding='utf-8') as f:
    d = json.load(f)

units = d.get('units', [])
print(f'Units in battle: {len(units)}')
for i, u in enumerate(units):
    print(f"{i}: id={u.get('id')} name={u.get('name')} classId={u.get('classId')} side={u.get('side')} pos=({u.get('x')},{u.get('y')}) hp={u.get('hp')} weapon={u.get('starterWeapon')}")
