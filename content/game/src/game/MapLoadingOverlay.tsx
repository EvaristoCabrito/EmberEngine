import { useCallback, useEffect, useLayoutEffect, useRef, useState, useSyncExternalStore } from "react";

/** Streams the map image once so the loading bar reports bytes actually received. */
export function useMapLoading(source: string) {
  const [progress, setProgress] = useState<number | null>(null);
  const [imageSrc, setImageSrc] = useState<string | null>(null);
  const [failed, setFailed] = useState(false);
  const [visible, setVisible] = useState(true);
  const finished = useRef(false);
  const objectUrl = useRef<string | null>(null);

  const finish = useCallback(() => {
    if (finished.current) return;
    finished.current = true;
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(() => {
        setProgress(100);
        window.setTimeout(() => setVisible(false), 280);
      });
    });
  }, []);

  const fail = useCallback(() => {
    if (finished.current) return;
    finished.current = true;
    setFailed(true);
    setVisible(false);
  }, []);

  useEffect(() => {
    const request = new XMLHttpRequest();
    let measurable = false;
    request.open("GET", source);
    request.responseType = "blob";
    request.onprogress = (event) => {
      if (!event.lengthComputable || event.total <= 0) {
        setProgress(null);
        return;
      }
      measurable = true;
      // Reserve the final point until the browser has decoded the image and the map has
      // positioned its markers. The number before then is the exact transfer byte ratio.
      setProgress(Math.min(99, Math.floor((event.loaded / event.total) * 99)));
    };
    request.onload = () => {
      if (request.status < 200 || request.status >= 300 || !(request.response instanceof Blob)) {
        fail();
        return;
      }
      if (measurable) setProgress(99);
      objectUrl.current = URL.createObjectURL(request.response);
      setImageSrc(objectUrl.current);
    };
    request.onerror = fail;
    request.onabort = fail;
    request.send();
    return () => {
      request.onprogress = null;
      request.onload = null;
      request.onerror = null;
      request.onabort = null;
      request.abort();
      if (objectUrl.current) URL.revokeObjectURL(objectUrl.current);
    };
  }, [source, fail]);

  return { progress, visible, finish, fail, imageSrc, failed };
}

/** The loading art (LoadingSCreen.jpg, 2128x912) scaled to COVER the whole screen — no black
 * border; on narrower screens the far left/right edges are trimmed, the centred title and bar
 * never are — with the progress bar inside the art's own bar frame and the status inside the
 * plaque under it. Positions are fractions of the picture (measured from the file: bar interior
 * x 752-1362 / y 787-811, plaque x 903-1203 / y 842-881), so they stay in place at any size. */
const LOADING_ART_SRC = "/game/ui/LoadingSCreen.jpg?v=2";
// Fetched and decoded once, as soon as the game starts (GameApp imports this module), so the
// art is already there the first time a loading screen opens.
const loadingArtPreload = typeof Image !== "undefined" ? new Image() : null;
if (loadingArtPreload) {
  loadingArtPreload.src = LOADING_ART_SRC;
  loadingArtPreload.decode?.().catch(() => {});
}

/** game.html's castle bar handed on to the loading screen(s) shown right after it (see its
 * script and main.tsx): their bar continues from `offset` instead of restarting at 0, for as
 * long as some loading screen stays up. When the last one closes, the boot's share of the whole
 * time is remembered so the next boot of this entry fills its part of the bar to match. */
let bootChain: { offset: number; bootMs: number; key: string } | null = null;
let openLoadingScreens = 0;
const bootChainListeners = new Set<() => void>();
const subscribeBootChain = (listener: () => void) => {
  bootChainListeners.add(listener);
  return () => { bootChainListeners.delete(listener); };
};
const currentBootChain = () => bootChain;

export function openLoadingScreenCount(): number {
  return openLoadingScreens;
}

export function continueBootBar(offset: number, key: string): void {
  bootChain = { offset, bootMs: performance.now(), key };
  for (const listener of bootChainListeners) listener();
}

function loadingScreenOpened(): void {
  openLoadingScreens++;
}

function loadingScreenClosed(): void {
  openLoadingScreens--;
  // one screen can replace another in the same commit (battle curtain → map): end the chain
  // only if none is up a frame later
  requestAnimationFrame(() => {
    if (openLoadingScreens > 0 || !bootChain) return;
    try { localStorage.setItem(bootChain.key, String(Math.min(1, bootChain.bootMs / performance.now()))); } catch { /* storage blocked */ }
    bootChain = null;
  });
}

function LoadingArt({ progress: ownProgress, label: ownLabel, barLabel, z }: { progress: number | null; label: string; barLabel: string; z: string }) {
  useLayoutEffect(() => {
    loadingScreenOpened();
    return loadingScreenClosed;
  }, []);
  // Continuing game.html's bar: this screen's own 0-100 fills the part after the boot's.
  const chain = useSyncExternalStore(subscribeBootChain, currentBootChain, () => null);
  const progress = chain ? Math.floor(chain.offset + ((100 - chain.offset) * (ownProgress ?? 0)) / 100) : ownProgress;
  const label = chain ? `${ownLabel.replace(/ · \d+%$/, "")} · ${progress}%` : ownLabel;
  // The art, bar and plaque text appear together: only the dark backdrop until the picture is
  // ready, so the bar never shows on its own before the image. (A failed image still reveals
  // the bar, so loading progress is never hidden.)
  const [artReady, setArtReady] = useState(() => !!loadingArtPreload?.complete && loadingArtPreload.naturalWidth > 0);
  return (
    <div className={`absolute inset-0 ${z} overflow-hidden bg-[#131313]`} aria-live="polite" aria-label={label}>
      <div
        className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2"
        style={{ width: "max(100vw, calc(100vh * 2128 / 912))", aspectRatio: "2128 / 912", visibility: artReady ? "visible" : "hidden" }}
      >
        <img src={LOADING_ART_SRC} alt="" className="absolute inset-0 size-full" onLoad={() => setArtReady(true)} onError={() => setArtReady(true)} />
        <div
          className="absolute overflow-hidden"
          style={{ left: "35.34%", top: "86.29%", width: "28.67%", height: "2.63%", borderRadius: 2 }}
          role="progressbar" aria-label={barLabel} aria-valuemin={0} aria-valuemax={100} aria-valuenow={progress ?? undefined}
        >
          {/* LOCKED (2026-10-06): no real number means no fill — never a fake sweeping bar. */}
          {progress !== null && (
            <div
              className="h-full bg-gradient-to-r from-[#444444] via-[#e1a541] to-[#fff0a2] transition-[width] duration-100 ease-linear"
              style={{ width: `${progress}%` }}
            />
          )}
        </div>
        <p
          className="absolute grid place-items-center text-center uppercase text-[#eee0c8] drop-shadow-[0_1px_2px_#000]"
          style={{ left: "42.43%", top: "92.32%", width: "14.1%", height: "4.28%", fontSize: "calc(max(100vh, 100vw * 912 / 2128) * 0.0125)", letterSpacing: "0.08em", lineHeight: 1.15 }}
        >
          {label}
        </p>
      </div>
    </div>
  );
}

export function MapLoadingOverlay({ progress, visible }: { progress: number | null; visible: boolean }) {
  if (!visible) return null;
  const label = progress === null ? "Carregando o mapa…" : `Carregando o mapa · ${progress}%`;
  return <LoadingArt progress={progress} label={label} barLabel="Carregando o mapa" z="z-[70]" />;
}

/** Screen ids heavy enough (a big map background, the battle canvas' whole art set) that
 * the swap into them can outrun a single React commit and leave a blank/frozen frame in
 * between — the general "stuck for a few seconds" complaint. Anything else (briefing,
 * cutscenes, menus) is cheap enough not to need this. The two map screens are NOT listed:
 * each one mounts its own MapLoadingOverlay (same art) in its very first commit with the
 * real byte progress of the map, and this number-less curtain on top of it hid that real bar
 * (empty bar for 380 ms while the map underneath was already at 99-100%). */
const HEAVY_SCREENS = new Set(["battle"]);

/** Curtains any transition into a HEAVY_SCREENS destination behind the same loading art
 * used for the map, for a floor of MIN_VISIBLE_MS so it always reads as a deliberate
 * loading beat rather than a flicker, however fast the actual mount turns out to be. */
export function useLoadingCurtain(screen: string): boolean {
  const [visible, setVisible] = useState(false);
  const prevScreen = useRef(screen);

  // A layout effect runs before the browser paints, so the curtain appears in the very same
  // frame as the new screen — the map/board is never shown, even for one frame, before it.
  useLayoutEffect(() => {
    if (prevScreen.current === screen) return;
    prevScreen.current = screen;
    if (!HEAVY_SCREENS.has(screen)) return;
    setVisible(true);
    const MIN_VISIBLE_MS = 380;
    const timer = window.setTimeout(() => setVisible(false), MIN_VISIBLE_MS);
    return () => window.clearTimeout(timer);
  }, [screen]);

  return visible;
}

export function LoadingCurtain({ visible, progress = null, status = "Preparando a tela…" }: { visible: boolean; progress?: number | null; status?: string }) {
  if (!visible) return null;
  const label = progress === null ? status : `${status} · ${progress}%`;
  return <LoadingArt progress={progress} label={label} barLabel={status} z="z-[80]" />;
}
