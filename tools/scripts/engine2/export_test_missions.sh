#!/usr/bin/env bash
# Every campaign mission as Engine2's Modo teste plays it (Engine2 read-only): make_test_map.ts (the missing heroes added,
# the party at level 15), then the scene (export_scene.ts + measure_art.py) and the battle (export_battle.ts with
# TEST_LEVEL=15, no reference answers). Output: build/engine2-test/{maps,scenes,battles}/<map>.*; failures listed in
# build/engine2-test/failed.txt.
#
#   bash tools/scripts/engine2/export_test_missions.sh <vite-node cache dir> [map stem ...]
set -u
cd "$(dirname "$0")/../../.."
CACHE="${1:?usage: export_test_missions.sh <vite-node cache dir> [map stem ...]}"
shift
E2="${ENGINE2:-C:/Engine2}"
VN="node $E2/node_modules/vite-node/vite-node.mjs --root $CACHE"
OUT=build/engine2-test
mkdir -p "$OUT/maps" "$OUT/scenes" "$OUT/battles"
if [ $# -gt 0 ]; then
  STEMS="$*"
else
  STEMS=$(python -I -c "import json;print(' '.join(m['mapFile'][:-5] for m in json.load(open('content/engine2/campaign.json',encoding='utf-8'))['missions'] if m.get('mapFile')))")
fi
: > "$OUT/failed.txt"
n=0
for m in $STEMS; do
  n=$((n+1))
  src="$E2/maps/ember/$m.json"
  if [ ! -f "$src" ]; then echo "$m: no map file" >> "$OUT/failed.txt"; continue; fi
  if $VN tools/scripts/engine2/make_test_map.ts -- "$src" "$OUT/maps/$m.json" > "$OUT/maps/$m.log" 2>&1 &&
     $VN tools/scripts/engine2/export_scene.ts -- "$OUT/maps/$m.json" "$OUT/scenes/$m.scene.json" > "$OUT/scenes/$m.log" 2>&1 &&
     python -I tools/scripts/engine2/measure_art.py "$OUT/scenes/$m.scene.json" "$E2/public" >> "$OUT/scenes/$m.log" 2>&1 &&
     TEST_LEVEL=15 NO_REFS=1 $VN tools/scripts/engine2/export_battle.ts -- "$OUT/maps/$m.json" "$OUT/battles/$m.battle.json" > "$OUT/battles/$m.log" 2>&1; then
    echo "[$n] $m ok"
  else
    echo "$m" >> "$OUT/failed.txt"
    echo "[$n] $m FAILED"
  fi
done
echo "done: $n maps, $(wc -l < "$OUT/failed.txt") failed"
