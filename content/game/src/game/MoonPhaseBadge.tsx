// The world map's moon box, next to the day clock: the current phase's icon. Until GPT's phase
// art is in public/game/ui/moon/ it draws a simple placeholder moon of the right shape.

import { useState } from "react";
import { daysToMoon, moonIllumination, moonPhaseIcon, moonPhaseOf, MOON_PHASE_LABEL, type MoonPhase } from "./moonPhase";

function PlaceholderMoon({ phase }: { phase: MoonPhase }) {
  const { lit, waxing } = moonIllumination(phase);
  const r = 10, rx = r * Math.abs(1 - 2 * lit);
  // lit side: the right half's outer edge, back up along the terminator ellipse
  const d = `M12 2 A${r} ${r} 0 0 1 12 22 A${rx} ${r} 0 0 ${lit > 0.5 ? 1 : 0} 12 2 Z`;
  return (
    <svg viewBox="0 0 24 24" className="size-6" aria-hidden>
      <circle cx="12" cy="12" r={r} fill="#1d2228" stroke="#5b636c" strokeWidth="0.8" />
      {lit > 0.01 && <path d={d} fill={phase === "blood" ? "#9e1b1b" : "#e6e1cf"} transform={waxing ? undefined : "translate(24 0) scale(-1 1)"} />}
    </svg>
  );
}

export function MoonPhaseBadge({ gameClock }: { gameClock: number }) {
  const phase = moonPhaseOf(gameClock);
  const [missing, setMissing] = useState<Record<string, boolean>>({});
  const toBlood = daysToMoon(gameClock, "blood");
  const label = `${MOON_PHASE_LABEL[phase]}${toBlood === 0 ? "" : ` · lua de sangue em ${toBlood} ${toBlood === 1 ? "dia" : "dias"}`}`;
  return (
    <p aria-label={label} title={label} className="inline-flex items-center justify-center ember-plate size-9 shrink-0">
      {missing[phase] ? (
        <PlaceholderMoon phase={phase} />
      ) : (
        <img
          src={moonPhaseIcon(phase)}
          alt=""
          draggable={false}
          className="size-7 select-none object-contain"
          onError={() => setMissing((m) => ({ ...m, [phase]: true }))}
        />
      )}
    </p>
  );
}
