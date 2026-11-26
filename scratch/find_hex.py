import os, re

with open('c:/EmberEngineB/engine/world/Spells.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i, line in enumerate(lines, 1):
    if 'BattleHex hexBeyond' in line:
        print(f'Spells.cpp line {i}: {line.strip()}')
