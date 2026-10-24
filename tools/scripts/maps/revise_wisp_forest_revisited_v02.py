"""Keep the first reviewed draft and create a second tree silhouette pass."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'content/maps/ember/wisp-forest008.json'
OUT = ROOT / 'content/maps/ember/wisp-forest009.json'
TREES = ['weeping-willow','giant-dead-oak','hollow-snag','uprooted-leaning','lightning-split',
         'spiral-twisted','tall-burnt-pine','broken-crown','root-arch']


def main():
    if OUT.exists():
        raise SystemExit(f'preserving existing output; choose a new revision: {OUT}')
    data = copy.deepcopy(json.loads(SOURCE.read_text(encoding='utf-8')))
    data['serial'] = int(data.get('serial', 0)) + 1
    data['savedAt'] = '2026-10-24T00:00:00.000Z'
    d = data['draft']
    d['id'] = 'wisp-forest-revisited-v02'
    for decoration in d['decorations']:
        for tree in TREES:
            if decoration['id'] == f'ember-wisp-revisited-{tree}':
                decoration['id'] += '-v02'
                break
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'created {OUT.relative_to(ROOT)} with nine distinct v02 tree models; source draft preserved')


if __name__ == '__main__': main()
