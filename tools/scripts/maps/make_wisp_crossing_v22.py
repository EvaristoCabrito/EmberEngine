"""Wisp Forest Crossing 022 = 021 with its uprooted dead trees standing upright.

Reads content/maps/ember/wisp-forest-crossing021.json (left untouched) and writes wisp-forest-crossing022.json (the campaign plays
the highest serial): every `ember-wisp-revisited-uprooted-leaning-v02` (the trunk leans ~21 deg) becomes `-v03` (the same tree,
trunk straight), same hex and turn. Nothing else changes. Refuses to overwrite an existing 022.
"""
import json, os, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / 'content/maps/ember/wisp-forest-crossing021.json'
DST = ROOT / 'content/maps/ember/wisp-forest-crossing022.json'
OLD, NEW = 'ember-wisp-revisited-uprooted-leaning-v02', 'ember-wisp-revisited-uprooted-leaning-v03'
SERIAL = 22


def main():
    if DST.exists():
        sys.exit(f'{DST.name} exists: bump the serial instead of overwriting')
    data = json.loads(SRC.read_text(encoding='utf-8'))
    swapped = 0
    for deco in data['draft']['decorations']:
        if deco.get('id') == OLD:
            deco['id'] = NEW; swapped += 1
    if not swapped:
        sys.exit(f'no {OLD} on {SRC.name}')
    data['serial'] = SERIAL
    DST.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'{DST.name}: {swapped} trees upright')


if __name__ == '__main__':
    main()
