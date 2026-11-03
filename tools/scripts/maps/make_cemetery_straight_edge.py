"""Cemitério dos Esquecidos 013: 012 with a straight board edge (the user, 2026-11-03: "EU JA FALEI QUE EU NÃO QUERO MAPA COM BORDA
HEXAGONAL", then "Only the cemetery"): "emberStraightEdge": true (MapDraft; BoardGround cuts the ground to the rectangle inside the
hexes' sawtooth and stands its sides along the cut). Everything else is 012's. Writes content/maps/ember/cemiterio-esquecidos013.json
(serial 13; 012 untouched).

  python -I tools/scripts/maps/make_cemetery_straight_edge.py [src.json] [out.json]
"""
import json
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'content/maps/ember/cemiterio-esquecidos012.json'
out = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/cemiterio-esquecidos013.json'
doc = json.load(open(src, encoding='utf-8'))
doc['draft']['emberStraightEdge'] = True
doc['serial'] = 13
doc['savedAt'] = doc.get('savedAt', 0) + 1
json.dump(doc, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
print(f'{out}: straight board edge')
