import { useEffect, useState } from 'react';
import { GROUND_SET_LABEL, getGroundSet, onGroundSet, setGroundSet, type GroundSet } from './groundSets';

const GROUND_SETS: GroundSet[] = ['proto', 'claude', 'gpt', 'engine2-v3', 'engine2-v4'];

/** Lets map painters switch the baked material set without changing saved terrain variants. */
export function GroundSetPicker() {
  const [selected, setSelected] = useState<GroundSet>(() => getGroundSet());
  useEffect(() => onGroundSet(setSelected), []);
  return (
    <label className="flex items-center gap-2 text-xs text-muted">
      <span>Texturas 3D</span>
      <select
        aria-label="Conjunto de texturas 3D"
        value={selected}
        onChange={(event) => setGroundSet(event.target.value as GroundSet)}
        className="rounded border border-border bg-bg px-2 py-1 text-fg"
      >
        {GROUND_SETS.map((set) => <option key={set} value={set}>{GROUND_SET_LABEL[set]}</option>)}
      </select>
    </label>
  );
}
