"""Print the catalogue table (Markdown) of content/models/models.json for the given kits, for content/models/README.md.

  python -I tools/scripts/models/readme_table.py nature treasure ...
"""
import json
import sys


def main(*kits):
    with open('content/models/models.json', encoding='utf-8') as f:
        models = json.load(f)['models']
    print('| id | name | file | w × h | tris | footprint |')
    print('|---|---|---|---|---|---|')
    for m in models:
        if kits and m['kit'] not in kits:
            continue
        print(f"| {m['id']} | {m['name']} | {m['file'].replace('/game/ember/', '')} | {m['width']:.2f} × {m['height']:.2f} | {m.get('triangles', '')} | {m['footprint']}{'' if m.get('blocks', True) else ', passável'} |")


if __name__ == '__main__':
    main(*sys.argv[1:])
