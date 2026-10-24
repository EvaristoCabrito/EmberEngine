import { getDevGfx, subscribeDevGfx } from "./gfx/three/devGfx";
import { uiText as t, useGamePreferences } from "./gamePreferences";
import { useSyncExternalStore } from "react";
import { getGraphicsQuality, getGraphicsQualityServerGfx, setGraphicsQuality, subscribeGraphicsQuality, graphicsQualityIsCustom, type GraphicsQuality } from "./graphicsQuality";

export function GraphicsQualityControl() {
  useGamePreferences();
  const quality = useSyncExternalStore(subscribeGraphicsQuality, getGraphicsQuality, () => "high" as GraphicsQuality);
  const gfx = useSyncExternalStore(subscribeDevGfx, getDevGfx, getGraphicsQualityServerGfx);
  const custom = graphicsQualityIsCustom(gfx);
  return <fieldset className="rounded-lg border border-border bg-bg/80 p-3">
    <legend className="px-1 text-sm">{t("Qualidade gráfica")}</legend>
    <div className="flex gap-2">{([['low', 'Baixa'], ['medium', 'Média'], ['high', 'Alta']] as const).map(([id, label]) =>
      <button key={id} type="button" aria-pressed={!custom && quality === id} onClick={() => setGraphicsQuality(id)}
        className={`flex-1 rounded border px-3 py-2 text-sm ${!custom && quality === id ? 'border-accent bg-accent/20' : 'border-border'}`}>{t(label)}</button>)}</div>
    <p className="mt-2 text-xs text-muted">{t("Baixa: sem sombras ou FX atmosféricos. Média: sombras 2048 sem contato. Alta: sombras 4096 com contato.")}</p>
    {custom && <p className="mt-2 text-xs">{t("Personalizada")} · {gfx.shadowResolution}</p>}
    <p className="mt-2 text-xs text-muted">{t("Ajusta resolução, sombras e iluminação. Mantém os modelos e as regras do jogo.")}</p>
  </fieldset>;
}
