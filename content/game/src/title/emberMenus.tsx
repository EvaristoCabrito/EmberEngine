// Ember's own Options window and How to play window (EmberAshes3D's OptionsMenu.tsx and
// GameApp's HelpModal, unchanged), opened from the Engine2 title screen's buttons.
// Loaded on first use, so the title's boot bar is not slowed by the game bundle.
import "@/styles.css";
import { createRoot, type Root } from "react-dom/client";
import { OptionsDialog } from "../game/OptionsMenu";
import { HelpModal } from "../game/GameApp";

let host: HTMLDivElement | null = null;
let root: Root | null = null;

function mount(): Root {
  if (!host) {
    host = document.createElement("div");
    host.className = "ember-title-menus";
    // HelpModal fills its positioned parent (absolute inset-0), as on Ember's title screen.
    host.style.cssText = "position:fixed;inset:0;z-index:50;display:none";
    document.body.append(host);
    root = createRoot(host);
  }
  host.style.display = "block";
  return root!;
}

function close(onClosed?: () => void): void {
  root?.render(null);
  if (host) host.style.display = "none";
  onClosed?.();
}

export function openHelp(onClosed?: () => void): void {
  mount().render(<HelpModal onClose={() => close(onClosed)} />);
}

export function openOptions({ muted, onMute, onClosed }: { muted: () => boolean; onMute: () => void; onClosed?: () => void }): void {
  const r = mount();
  const render = () => r.render(<OptionsDialog muted={muted()} onMute={() => { onMute(); render(); }} onClose={() => close(onClosed)} />);
  render();
}
