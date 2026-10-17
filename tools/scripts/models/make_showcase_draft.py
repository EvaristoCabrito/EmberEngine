"""A Map Editor draft that shows every EmberEngine 3D prop on a real map (the capture in captures/models): the given
Engine2 map with its decorations replaced by one of each model, laid in rows around the board's centre.

  python -I tools/scripts/models/make_showcase_draft.py <map.json> <out.json> [kit,kit,...] [content/models/models.json]
"""
import json
import sys


def main(map_path, out_path, kits='', models_path='content/models/models.json'):
    with open(map_path, encoding='utf-8') as f:
        src = json.load(f)
    with open(models_path, encoding='utf-8') as f:
        models = json.load(f)['models']
    if kits:
        wanted = set(kits.split(','))
        models = [m for m in models if m['kit'] in wanted]
    draft = dict(src['draft'] if 'draft' in src else src)
    cols, rows = draft['cols'], draft['rows']
    draft['decorations'] = []
    draft['enemySpawns'] = []
    draft['neutralSpawns'] = []
    draft['fog'] = False
    x0, y0, step = 3, 4, 3
    x, y = x0, y0
    for m in models:
        draft['decorations'].append({'id': m['id'], 'x': x, 'y': y})
        x += step + (1 if m.get('footprint', 'one') != 'one' else 0)
        if x > cols - 3:
            x, y = x0, y + step
    draft['id'] = 'showcase-models'
    draft['title'] = 'Modelos 3D'
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump({'serial': 1, 'savedAt': 0, 'draft': draft}, f, ensure_ascii=False)
    print(f'{len(models)} models on {cols}x{rows}, last row y={y} -> {out_path}')


if __name__ == '__main__':
    main(*sys.argv[1:])
