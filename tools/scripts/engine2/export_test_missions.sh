#!/usr/bin/env bash
# Prepare the campaign artifacts consumed by both normal play and Modo Teste.
# The test save supplies six level-30 heroes; this script does not create a separate test-level battle.
#
#   bash tools/scripts/engine2/export_test_missions.sh <vite-node cache dir> [map stem ...]
set -u
cd "$(dirname "$0")/../../.."
CACHE="${1:?usage: export_test_missions.sh <vite-node cache dir> [map stem ...]}"
shift
GAME_DATA="${EMBER_GAME_DATA_ROOT:-content/game}"
run_vite() {
  EMBER_GAME_DATA_ROOT="$GAME_DATA" node "$GAME_DATA/node_modules/vite-node/vite-node.mjs" \
    --root "$CACHE" --config "$GAME_DATA/vite.export.config.mjs" "$@"
}
OUT=build/engine2-campaign
mkdir -p "$OUT/maps" "$OUT/scenes" "$OUT/battles"
if [ $# -gt 0 ]; then
  STEMS="$*"
else
  STEMS=$(python -I -c "import json,os;print(' '.join(os.path.basename(m['mapFile'])[:-5] for m in json.load(open('content/engine2/campaign.json',encoding='utf-8'))['missions'] if m.get('mapFile'))) ")
fi
: > "$OUT/failed.txt"
n=0
for m in $STEMS; do
  n=$((n+1))
  src="content/maps/ember/$m.json"
  if [ ! -f "$src" ]; then echo "$m: no local map file" >> "$OUT/failed.txt"; continue; fi
  if run_vite tools/scripts/engine2/make_test_map.ts -- "$src" "$OUT/maps/$m.json" > "$OUT/maps/$m.log" 2>&1 &&
     run_vite tools/scripts/engine2/export_scene.ts -- "$OUT/maps/$m.json" "$OUT/scenes/$m.scene.json" > "$OUT/scenes/$m.log" 2>&1 &&
     python -I tools/scripts/engine2/measure_art.py "$OUT/scenes/$m.scene.json" "$GAME_DATA/public" >> "$OUT/scenes/$m.log" 2>&1 &&
     NO_REFS=1 run_vite tools/scripts/engine2/export_battle.ts -- "$OUT/maps/$m.json" "$OUT/battles/$m.battle.json" > "$OUT/battles/$m.log" 2>&1; then
    echo "[$n] $m ok"
  else
    echo "$m" >> "$OUT/failed.txt"
    echo "[$n] $m FAILED"
  fi
done
echo "done: $n maps, $(wc -l < "$OUT/failed.txt") failed"
