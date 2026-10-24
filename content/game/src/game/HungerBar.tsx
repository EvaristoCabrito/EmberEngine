import { fullness } from "./hunger";
import { POISON_TIERS, poisonDice } from "./poison";
import type { PoisonTier } from "./types";

export function HungerBar({ value, name, travel = false }: { value?: number; name: string; travel?: boolean }) {
  const remaining = fullness(value);
  const label = `Saciedade: ${Number(remaining.toFixed(1))}%`;
  const cost = travel ? "Estrada (6h): −12,5 · Planície (12h): −25 · Floresta (24h): −50 · Montanha/caverna (36h): −75" : "−50 por dia · −2 por ação";
  return (
    <span className="hunger-bar" role="progressbar" aria-label={`Saciedade de ${name}`} aria-valuemin={0} aria-valuemax={120} aria-valuenow={remaining} aria-valuetext={`${label} · ${cost}`} title={`${label} · ${cost}`}>
      <span className={remaining <= 25 ? "bg-danger" : "bg-accent"} style={{ width: `${(Math.min(100, remaining) / 120) * 100}%` }} />
      {remaining > 100 && <span className="hunger-bonus" style={{ width: `${((remaining - 100) / 120) * 100}%` }} />}
    </span>
  );
}

/** World-map life bar, under the hunger bar: red when healthy, green while poisoned (poison
 * rolls its damage on every hex step), purple while diseased; green-to-purple when both. */
export function LifeBar({ name, hp, maxHp, poisonTier, diseased }: { name: string; hp: number; maxHp: number; poisonTier?: PoisonTier; diseased?: boolean }) {
  const max = Math.max(1, maxHp);
  const current = Math.max(0, Math.min(max, hp));
  const state = poisonTier && diseased ? "both" : poisonTier ? "poisoned" : diseased ? "diseased" : "healthy";
  const conditions = [
    poisonTier ? `Envenenado · ${POISON_TIERS[poisonTier].name} (${poisonDice(poisonTier)} de dano a cada hex)` : "",
    diseased ? "Doente (−10% nos atributos)" : "",
  ].filter(Boolean);
  const label = `Vida: ${current}/${max}${conditions.length ? ` · ${conditions.join(" · ")}` : ""}`;
  return (
    <span className={`hunger-bar life-bar is-${state}`} role="progressbar" aria-label={`Vida de ${name}`} aria-valuemin={0} aria-valuemax={max} aria-valuenow={current} aria-valuetext={label} title={label}>
      <span style={{ width: `${(current / max) * 100}%` }} />
    </span>
  );
}
