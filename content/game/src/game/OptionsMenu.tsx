import { useEffect, useRef, useState, useSyncExternalStore, type CSSProperties } from "react";
import { Monitor, Volume2, Languages, Check, Settings } from "lucide-react";
import { getDevGfx, setDevGfx, subscribeDevGfx } from "./gfx/three/devGfx";
import { getAudioVolumes, getCutsceneAudioSettings, setCutsceneEnabled, setMusicVolume, setSfxVolume, setCutsceneVolume } from "./audio";
import { uiText as t, useGamePreferences, setGamePreferences } from "./gamePreferences";

type GameMenuActions = { onSaveGame?: () => void; onLoadGame?: () => void; onReturnToTitle?: () => void };

export function OptionsButton({ muted, onMute, iconOnly = false, escapeKey = false, ...actions }: { muted: boolean; onMute: () => void; iconOnly?: boolean; escapeKey?: boolean } & GameMenuActions) {
 const [open, setOpen] = useState(false);
 // escapeKey: Esc opens this menu (an open menu closes on Esc by itself, as a <dialog>).
 useEffect(() => {
  if (!escapeKey || open) return;
  const onKey = (e: KeyboardEvent) => {
   if (e.key !== "Escape" || e.defaultPrevented || document.querySelector("dialog[open]") || (e.target as HTMLElement | null)?.closest("input, textarea, select")) return;
   e.preventDefault();
   setOpen(true);
  };
  window.addEventListener("keydown", onKey);
  return () => window.removeEventListener("keydown", onKey);
 }, [escapeKey, open]);
 return <>
 <button type="button" className={iconOnly ? "size-9 grid place-items-center ember-icon-btn" : "w-full ember-btn ember-btn-ghost py-3"} onClick={() => setOpen(true)} aria-label={t("Opções")} aria-haspopup="dialog" aria-expanded={open} title={t("Opções")}>
 {iconOnly ? <Settings className="size-4" aria-hidden="true" /> : t("Opções")}
 </button>
 {open && <OptionsDialog muted={muted} onMute={onMute} onClose={() => setOpen(false)} {...actions} />}
 </>;
}

/** Ember's Options window itself (also opened by the Engine2 title screen's Opções button). */
export function OptionsDialog({ muted, onMute, onClose, onSaveGame, onLoadGame, onReturnToTitle }: { muted: boolean; onMute: () => void; onClose: () => void } & GameMenuActions) {
 const prefs = useGamePreferences();
 const gfx = useSyncExternalStore(subscribeDevGfx, getDevGfx, getDevGfx);
 const [volumes, setVolumes] = useState(getAudioVolumes);
 const [cutsceneEnabled, setCutsceneEnabledUi] = useState(() => getCutsceneAudioSettings().enabled);
 const [error, setError] = useState("");
 const dialog = useRef<HTMLDialogElement>(null);
 useEffect(() => { dialog.current?.showModal(); }, []);
 return <>
 <dialog ref={dialog} onCancel={onClose} onClose={onClose} className="game-options ember-panel m-auto w-[min(94vw,44rem)] max-h-[94dvh] overflow-y-auto p-5" aria-labelledby="game-options-title">
 <div className="game-options-header flex items-center justify-between gap-4 mb-3 pb-3"><h2 id="game-options-title" className="ember-title text-2xl leading-none">{t("Opções")}</h2><button type="button" autoFocus className="ember-btn ember-btn-ghost ember-btn-sm px-4 py-2" onClick={onClose}>{t("Fechar")}</button></div>
 <div className="grid gap-2 sm:grid-cols-2 items-start">
 {(onSaveGame || onLoadGame || onReturnToTitle) && <section className="game-options-section ember-slot space-y-1 px-3 py-2"><h3 className="game-options-heading ember-title">{t("Jogo")}</h3>
 {onSaveGame && <button type="button" className="ember-btn w-full px-3 py-2" onClick={onSaveGame}>{t("Salvar jogo")}</button>}
 {onLoadGame && <button type="button" className="ember-btn w-full px-3 py-2" onClick={onLoadGame}>{t("Carregar jogo")}</button>}
 {onReturnToTitle && <button type="button" className="ember-btn ember-btn-ghost w-full px-3 py-2" onClick={onReturnToTitle}>{t("Voltar à tela inicial")}</button>}
 </section>}
 <section className="game-options-section ember-slot space-y-1 px-3 py-2"><h3 className="game-options-heading ember-title"><Monitor aria-hidden="true" />{t("Gráficos")}</h3>
 {([['realShadows', 'Sombras'], ['localLights', 'Luzes locais'], ['atmosphericFx', 'FX atmosféricos']] as const).map(([key,label]) => <label key={key} className="flex justify-between gap-3 text-sm"><span>{t(label)}</span><input type="checkbox" checked={gfx[key]} onChange={e => setDevGfx({ [key]: e.target.checked })} /></label>)}
 <button type="button" className="ember-btn ember-btn-ghost ember-btn-sm w-full px-3 py-1.5 mt-1" onClick={async () => { try { if (document.fullscreenElement) await document.exitFullscreen(); else await document.documentElement.requestFullscreen(); setError(""); } catch { setError(t("Tela cheia indisponível neste navegador.")); } }}>{t("Tela cheia")}</button>{error && <p role="status">{error}</p>}
 </section>
 <section className="game-options-section ember-slot space-y-1 px-3 py-2"><h3 className="game-options-heading ember-title"><Volume2 aria-hidden="true" />{t("Áudio")}</h3><label className="flex justify-between text-sm">{t("Silenciar")}<input type="checkbox" checked={muted} onChange={onMute} /></label>
 <label className="flex justify-between text-sm">{t("Som das cutscenes")}<input type="checkbox" checked={cutsceneEnabled} onChange={e => { const enabled=e.target.checked; setCutsceneEnabled(enabled); setCutsceneEnabledUi(enabled); }} /></label>
 {([['music','Música',setMusicVolume],['sfx','Efeitos sonoros',setSfxVolume],['cutscene','Volume da cutscene',setCutsceneVolume]] as const).map(([key,label,setter]) => <label key={key} className="block text-sm">{t(label)} <span className="float-right">{Math.round(volumes[key]*100)}%</span><input className="w-full" type="range" min="0" max="1" step="0.01" value={volumes[key]} style={{ "--fill": `${volumes[key] * 100}%` } as CSSProperties} disabled={key === 'cutscene' && !cutsceneEnabled} onChange={e => { const value=Number(e.target.value); setter(value); setVolumes(v => ({...v,[key]:value})); }} /></label>)}
 </section>
 <section className="game-options-section ember-slot space-y-1 px-3 py-2"><h3 className="game-options-heading ember-title"><Languages aria-hidden="true" />{t("Idiomas")}</h3>
 {([['uiLanguage','Interface e nomes'],['dialogueLanguage','Diálogos']] as const).map(([key,label]) => <label key={key} className="flex items-center justify-between gap-2 text-sm">{t(label)}<select className="bg-bg border border-border rounded p-2" value={prefs[key]} onChange={e => setGamePreferences({[key]:e.target.value as 'pt'|'en'})}><option value="pt">Português</option><option value="en">English</option></select></label>)}
 <label className="flex items-center justify-between gap-2 text-sm">{t("Idioma das legendas")}<select className="bg-bg border border-border rounded p-2" value={prefs.subtitleLanguage} onChange={e => setGamePreferences({subtitleLanguage:e.target.value as 'pt'|'en'})}><option value="pt">{t("Português (Brasil)")}</option><option value="en">English</option></select></label></section>
 <section className="game-options-section ember-slot space-y-1 px-3 py-2"><h3 className="game-options-heading ember-title">{t("Acessibilidade")}</h3><label className="flex justify-between text-sm">{t("Exibir legendas")}<input type="checkbox" checked={prefs.subtitles} onChange={e => setGamePreferences({subtitles:e.target.checked})} /></label>
 <label className="flex justify-between gap-2 text-sm">{t("Tamanho do diálogo")}<select className="bg-bg border border-border rounded p-2" value={prefs.dialogueScale} onChange={e => setGamePreferences({dialogueScale:Number(e.target.value)})}>{([[1,'Normal'],[1.15,'Grande'],[1.3,'Muito grande']] as const).map(([value,label]) => <option key={value} value={value}>{t(label)}</option>)}</select></label></section>
 </div><p className="game-options-footer ember-rule mt-3 pt-3 text-xs text-muted flex items-center justify-center gap-2"><Check className="size-3.5" aria-hidden="true" />{t("Alterações salvas automaticamente.")}</p>
 </dialog></>;
}
