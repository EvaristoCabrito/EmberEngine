// Ember's screen-space mist vignettes ("Tipo de névoa": Vinheta 1-4), copied as-is from its
// BattleCanvas: CSS overlays tied to the viewport, not the world, so they work the same over the
// 3D map. Off when Dev Controls' atmospheric FX are off, as in Ember.

import { useSyncExternalStore } from "react";
import type { BattleEngine } from "./engine";
import { getDevGfx, subscribeDevGfx } from "./gfx/three/devGfx";

export function BattleVignettes({ engine }: { engine: BattleEngine }) {
  const atmosphericFx = useSyncExternalStore(subscribeDevGfx, () => getDevGfx().atmosphericFx, () => true);
  // Mission.mistType === "vignette" (Map Editor's "Tipo de névoa") turns this from the always-on
  // subtle diorama edge shading into an author-controlled hazy corner effect, driven by the same
  // Névoa slider/intensity that would otherwise drive the world-space mist systems — see
  // ThreeAtmosphere.ts's own comment on why "vignette" forces both of those to zero instead of
  // stacking with this. Screen-space, tied to the viewport rather than world position, so the
  // center (where the actual battle happens) is always guaranteed clear by construction — it
  // never grows past clearRadius no matter how high intensity goes.
  const isVignetteMist = engine.mission.mistType === "vignette";
  const isVignette2Mist = engine.mission.mistType === "vignette2";
  const isVignette3Mist = engine.mission.mistType === "vignette3";
  const isVignette4Mist = engine.mission.mistType === "vignette4";
  const vignetteIntensity = engine.mission.mistIntensity ?? 0.5;
  // A proper vignette is a dependable screen-space radial falloff: foggy/dark around the full
  // edge and completely clear at the battle's center. It deliberately avoids CSS masks and blend
  // isolation, which was why the former animated treatment could disappear in some browsers.
  const vignetteAlpha = Math.min(isVignette2Mist ? 0.46 : 0.52, 0.12 + vignetteIntensity * (isVignette2Mist ? 0.30 : 0.36));
  const vignetteClearRadius = Math.max(isVignette2Mist ? 55 : 48, (isVignette2Mist ? 76 : 68) - vignetteIntensity * 14);

  return (
    <>
      {/* A screen vignette belongs to the viewport rather than the world: it therefore covers the
          complete painted backdrop and stays fixed while the map pans. */}
      {atmosphericFx && isVignette2Mist && (
        <>
          <style>{`
            @keyframes vignetteMistPulse { 0%, 100% { opacity: 0.88; } 50% { opacity: 1; } }
            @keyframes vignetteFogDrift { 0%, 100% { transform: scale(1.06) translate3d(-2.5%, -1.5%, 0); } 50% { transform: scale(1.13) translate3d(2.5%, 1.5%, 0); } }
            @keyframes vignetteFogDriftNear { 0%, 100% { transform: scale(1.16) translate3d(2.2%, -1.8%, 0); } 50% { transform: scale(1.24) translate3d(-2.4%, 2.1%, 0); } }
            @keyframes vignette2FarDrift { 0%, 100% { transform: scale(1.12) translate3d(-4%, 2%, 0) rotate(-2deg); } 50% { transform: scale(1.24) translate3d(4%, -3%, 0) rotate(2deg); } }
            @keyframes vignette2NearDrift { 0%, 100% { transform: scale(1.3) translate3d(4%, -3%, 0) rotate(3deg); } 50% { transform: scale(1.18) translate3d(-4%, 3%, 0) rotate(-2deg); } }
          `}</style>
          <div
            className="pointer-events-none absolute inset-0 overflow-hidden"
            style={{
              background: isVignette2Mist
                ? `radial-gradient(ellipse 118% 112% at 50% 46%, transparent ${vignetteClearRadius}%, rgba(96,108,108,${vignetteAlpha * 0.22}) 77%, rgba(14,19,22,${vignetteAlpha}) 100%)`
                : `radial-gradient(ellipse 98% 92% at 50% 46%, transparent ${vignetteClearRadius}%, rgba(77,88,89,${vignetteAlpha * 0.42}) 76%, rgba(7,10,13,${vignetteAlpha}) 100%)`,
              animation: "vignetteMistPulse 5.5s ease-in-out infinite",
              zIndex: 5,
            }}
          >
            {isVignette2Mist && (
              <>
                <img
                  src="/game/assets/vignette-fog.png"
                  alt=""
                  draggable={false}
                  className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
                  style={{
                    opacity: 0.38 + vignetteIntensity * 0.23,
                    animation: "vignette2FarDrift 24s ease-in-out infinite",
                  }}
                />
                <img
                  src="/game/assets/vignette-fog.png"
                  alt=""
                  draggable={false}
                  className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
                  style={{
                    opacity: 0.34 + vignetteIntensity * 0.18,
                    animation: "vignette2NearDrift 31s ease-in-out infinite reverse",
                  }}
                />
                <div
                  className="pointer-events-none absolute -inset-[20%]"
                  style={{
                    background: `radial-gradient(ellipse 42% 30% at 8% 90%, rgba(164,178,174,${0.20 + vignetteIntensity * 0.14}) 0%, transparent 72%), radial-gradient(ellipse 38% 28% at 93% 8%, rgba(144,159,158,${0.16 + vignetteIntensity * 0.12}) 0%, transparent 74%)`,
                    filter: "blur(18px)",
                    animation: "vignette2FarDrift 27s ease-in-out infinite reverse",
                    mixBlendMode: "screen",
                  }}
                />
              </>
            )}
          </div>
        </>
      )}
      {atmosphericFx && isVignetteMist && (
        <>
          <style>{`
            @keyframes vignetteMistPulse { 0%, 100% { opacity: 0.88; } 50% { opacity: 1; } }
            @keyframes vignetteFogDrift { 0%, 100% { transform: scale(1.06) translate3d(-2.5%, -1.5%, 0); } 50% { transform: scale(1.13) translate3d(2.5%, 1.5%, 0); } }
            @keyframes vignetteFogDriftNear { 0%, 100% { transform: scale(1.16) translate3d(2.2%, -1.8%, 0); } 50% { transform: scale(1.24) translate3d(-2.4%, 2.1%, 0); } }
          `}</style>
          <div
            className="pointer-events-none absolute inset-0 overflow-hidden"
            style={{
              background: `radial-gradient(ellipse 125% 115% at 50% 44%, transparent ${vignetteClearRadius}%, rgba(62,70,71,${vignetteAlpha * 0.34}) 78%, rgba(7,10,13,${vignetteAlpha}) 100%)`,
              animation: "vignetteMistPulse 5.5s ease-in-out infinite",
              zIndex: 5,
            }}
          >
            <img
              src="/game/assets/vignette-fog.png"
              alt=""
              draggable={false}
              className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
              style={{
                opacity: 0.68 + vignetteIntensity * 0.26,
                animation: "vignetteFogDrift 13s ease-in-out infinite",
              }}
            />
            <img
              src="/game/assets/vignette-fog.png"
              alt=""
              draggable={false}
              className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
              style={{
                opacity: 0.22 + vignetteIntensity * 0.16,
                animation: "vignetteFogDriftNear 19s ease-in-out infinite",
              }}
            />
          </div>
        </>
      )}
      {atmosphericFx && isVignette3Mist && (
        <>
          <style>{`
            @keyframes vinheta3Drift { 0%, 100% { transform: scale(1.025) translate3d(-1.2%, 0.8%, 0); } 50% { transform: scale(1.07) translate3d(1.2%, -0.8%, 0); } }
          `}</style>
          <div className="pointer-events-none absolute inset-0 overflow-hidden" style={{ zIndex: 5 }}>
            <img
              src="/game/assets/vinheta-3-fog.png"
              alt=""
              draggable={false}
              className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
              style={{
                // One supplied artwork layer only. Its painted open center is deliberately
                // preserved; it is never tiled, duplicated, mirrored, or masked into the board.
                opacity: 0.38 + vignetteIntensity * 0.58,
                animation: "vinheta3Drift 22s ease-in-out infinite",
              }}
            />
          </div>
        </>
      )}
      {atmosphericFx && isVignette4Mist && (
        <>
          <style>{`
            @keyframes vinheta4Drift { 0%, 100% { transform: scale(1.02) translate3d(-0.8%, 0.6%, 0); opacity: .72; } 50% { transform: scale(1.06) translate3d(0.8%, -0.6%, 0); opacity: 1; } }
          `}</style>
          <div className="pointer-events-none absolute inset-0 overflow-hidden" style={{ zIndex: 5 }}>
            <img
              src="/game/assets/vinheta-4-fog.png"
              alt=""
              draggable={false}
              className="pointer-events-none absolute inset-0 h-full w-full select-none object-cover"
              style={{
                opacity: 0.28 + vignetteIntensity * 0.52,
                mixBlendMode: "screen",
                animation: "vinheta4Drift 24s ease-in-out infinite",
              }}
            />
          </div>
        </>
      )}
    </>
  );
}
