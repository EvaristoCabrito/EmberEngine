import "@/styles.css";
import { createRoot } from "react-dom/client";
import { GameApp } from "./GameApp";
import { continueBootBar, openLoadingScreenCount } from "./MapLoadingOverlay";
import { returnToTitle, shouldReturnToTitle } from "./titleNavigation";

const root = document.getElementById("app");
if (!root) throw new Error("Missing #app root for the Ember campaign.");

// Right-click belongs to the game (inspect / cancel): the browser's own menu never opens over it.
// Text fields keep it for copy and paste.
document.addEventListener("contextmenu", (e) => {
  if (!(e.target as HTMLElement | null)?.closest("input, textarea, [contenteditable='true']")) e.preventDefault();
});

// The title launches /game.html?start=... ; a browser reload of that page goes back to the title
// instead of replaying the launch (new campaign intro). A map-editor session still resumes on
// reload, as in Ember (its own resume snapshot).
const nav = performance.getEntriesByType("navigation")[0] as PerformanceNavigationTiming | undefined;
let editorResume = false;
try { editorResume = !!sessionStorage.getItem("ember:editor-resume"); } catch { /* storage blocked */ }

if (shouldReturnToTitle(new URLSearchParams(location.search).get("start"), nav?.type === "reload", editorResume)) returnToTitle();
else {
  createRoot(root).render(<GameApp />);
  // game.html's boot loading screen covers the code load; fade it once the app has painted.
  const boot = document.getElementById("boot-loading");
  if (boot) {
    requestAnimationFrame(() => requestAnimationFrame(() => {
      // (game.html's __finishBoot remembers how many files this boot needed.)
      const w = window as Window & { __finishBoot?: (handedOn?: boolean) => void; __bootShown?: () => number; __bootShareKey?: string };
      // A loading screen already up underneath (a battle or the map) continues this same bar;
      // otherwise the boot was the whole load and its bar ends at 100.
      const handOn = !boot.hidden && openLoadingScreenCount() > 0 && !!w.__bootShown && !!w.__bootShareKey;
      if (handOn) continueBootBar(w.__bootShown!(), w.__bootShareKey!);
      else if (!boot.hidden && w.__bootShareKey) { try { localStorage.setItem(w.__bootShareKey, "1"); } catch { /* storage blocked */ } }
      w.__finishBoot?.(handOn);
    }));
  }
}
