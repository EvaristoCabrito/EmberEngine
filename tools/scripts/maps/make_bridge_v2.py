"""A Ponte de Pedra, version 023: the bridge 6 hexes wider, its own deck tile, the painting as the gorge floor, the wind fog.

  python -I tools/scripts/maps/make_bridge_v2.py <thebridge022.json> [out]

Writes content/maps/ember/thebridge023.json (serial 23: the campaign plays it; 022 stays as it was). The board goes from
15 to 21 rows; everything keeps its place against its own wall: the north parapet row and what stands beside it (rows 0-1)
stay, the south parapet and what stands beside it (rows 12-14) move 6 rows down, the middle spreads (rows 2-7 by 2, 8-11
by 4). Every shift is even, so each hex keeps its offset (odd rows shifted half a hex) and its neighbours.
The draft names its EmberEngine extras (engine/world/Board.h MapDraft): emberTiles "ponte-v01" (the user's BridgeTiles photo on
the deck), emberFloor (the map's painting laid flat under the bridge, emberFloorDepth under the deck, emberFloorTile wide) and
emberWindFog (fog look 010: the wind fog in the gorge, outside the bridge walls), emberDeckClip (the deck ends at the walls).
"""

import json
import sys

ADDED = 6
WALL_COLS = (1, 6, 11, 16, 21, 26, 31, 35)


def shift(y):
    if y <= 1:
        return 0
    if y <= 7:
        return 2
    if y <= 11:
        return 4
    return ADDED


def main():
    src = json.load(open(sys.argv[1], encoding='utf-8'))
    out_path = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/thebridge023.json'
    d = src['draft']
    cols, rows = d['cols'], d['rows']
    new_rows = rows + ADDED
    grid = {}
    for k in ('tiles', 'tileVariants', 'tileRots'):
        old = d[k]
        assert len(old) == cols * rows
        # the deck is one terrain everywhere: each new row repeats the row it was opened beside
        new = [None] * (cols * new_rows)
        for r in range(rows):
            for c in range(cols):
                new[(r + shift(r)) * cols + c] = old[r * cols + c]
        for r in range(new_rows):
            for c in range(cols):
                if new[r * cols + c] is None:
                    src_r = max(rr for rr in range(rows) if rr + shift(rr) <= r)
                    new[r * cols + c] = old[src_r * cols + c]
        grid[k] = new
    draft = dict(d)
    draft['rows'] = new_rows
    draft.update(grid)
    draft['decorations'] = [dict(x, y=x['y'] + shift(x['y'])) for x in d['decorations']]
    # Both parapets at the same columns, spanning the deck from end to end (the user, 2026-10-21: "make sure the tiles finish right
    # at the walls"): eight 11.85-unit cards (each drawn 2 columns east of its anchor hex), 5 columns apart (4 for the last): the west
    # end 0.13 inside the deck's end (where the party starts), the east end 0.71 past it (the columns are 1.73 apart: no closer fit);
    # the deck is cut along their planes and at their ends (emberDeckClip).
    for row_id in ('bridge-parapet-tall-001', 'bridge-parapet-tall-statues-001'):
        walls = sorted((x for x in draft['decorations'] if x['id'] == row_id), key=lambda x: x['x'])
        assert len(walls) == len(WALL_COLS), (row_id, len(walls))
        for x, col in zip(walls, WALL_COLS):
            x['x'] = col
    for k in ('playerSpawns', 'enemySpawns', 'neutralSpawns'):
        draft[k] = [dict(u, y=u['y'] + shift(u['y'])) for u in d.get(k, [])]
    draft['emberTiles'] = 'ponte-v01'
    draft['emberFloor'] = 'thebridge-bg.jpg'
    draft['emberFloorDepth'] = 40.0
    draft['emberFloorTile'] = 240.0
    draft['emberWindFog'] = 10
    draft['emberDeckClip'] = True
    taken = set()
    for k in ('playerSpawns', 'enemySpawns', 'neutralSpawns'):
        for u in draft[k]:
            assert 0 <= u['x'] < cols and 0 <= u['y'] < new_rows, u
            assert (u['x'], u['y']) not in taken, u
            taken.add((u['x'], u['y']))
    out = {'serial': 23, 'savedAt': src.get('savedAt', 0) + 1, 'draft': draft}
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(f'wrote {out_path}: {cols}x{new_rows}, {len(draft["decorations"])} decorations, {len(taken)} spawns')


if __name__ == '__main__':
    main()
