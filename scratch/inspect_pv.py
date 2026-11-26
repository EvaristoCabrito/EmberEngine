with open('engine/world/Spells.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i, l in enumerate(lines, 1):
    if 'poleVault' in l or 'disarm' in l:
        if any(k in l for k in ['spellZone', 'spellAimValid', 'canCast', 'valid', 'minRange', 'maxRange', 'zone']):
            print(f"{i}: {l.strip()}")
