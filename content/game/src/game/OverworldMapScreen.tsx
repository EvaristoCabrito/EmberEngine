import { FORMATION_SLOTS, cleanPartyFormation, partyLeaderOf } from "./partyFormation";
import { useEffect, useMemo, useRef, useState, type MouseEvent as ReactMouseEvent, type PointerEvent as ReactPointerEvent } from "react";
import { BookOpen, Check, ChevronLeft, Clock, Lock, MapPin, X, ZoomIn, ZoomOut } from "lucide-react";
import { isCrossingDungeon, missionsForLocation } from "./mapstore";
import type { EquipSlot, Mission, PotionId, SaveData, WeaponType, WorldLocation } from "./types";
import { PartyInventoryOverlay, preloadPartyInventoryAssets } from "./InventoryScreens";
import { CREATE_FOOD_AND_WATER, POTIONS, WARP, createFoodAndWaterFormula, createFoodAndWaterPower, gearStatBonus, heroRecruited, rulesClass, spellIcon, statsFor, tierUses, warpPortalDuration } from "./data";
import { fullness, travelHungerCost } from "./hunger";
import { GoldAmount } from "./GoldAmount";
import { OptionsButton } from "./OptionsMenu";
import { canStepOverworld, hungerPenaltyFor, wispForestHex, wispForestUncleared, hexToWorld, isOverworldCell, locationExpired, neighborsOf, OVERWORLD_START_HEX, travelHoursForHex, travelTimeLabel, type OverworldEvent, worldToHex } from "./overworld";
import { HungerBar, LifeBar } from "./HungerBar";
import { poisonTierOf } from "./poison";
import { SKILL_CAP, SKILL_GAIN, SKILL_IDS, SKILLS, skillValue, TRAVEL_TRAINING_HINT_FLAG, TRAVEL_TRAINING_HOURS, type SkillId } from "./skills";
import { weaponTypesForClass } from "./weaponSkills";
import { portraitFor } from "./assets";
import { LEADER_FRAME_BOUNDS } from "./leaderFrameBounds";
import { key } from "./pathfinding";
import { QUESTS, questProgress, questStatus } from "./quests";
import { MapLoadingOverlay, useMapLoading } from "./MapLoadingOverlay";
import { campaignHour, campaignTimeOfDay } from "./campaignTime";
import { MoonPhaseBadge } from "./MoonPhaseBadge";
import { AFFINITY_HEROES, affinityBonus, affinityGrade, affinityScore, canUseAffinityDuo, canUseAffinityUltimate, type AffinityHero } from "./affinity";
import { uiText } from "./gamePreferences";

/** Party "Skills" tab order: weapon skills first, then the resistances with Poison Resistance
 * leading them; otherwise as skills.ts lists them. */
const PARTY_SKILL_ORDER = [
  ...SKILL_IDS.filter(id => id.endsWith("Weapon")),
  "poisonResistance" as const,
  ...SKILL_IDS.filter(id => !id.endsWith("Weapon") && id !== "poisonResistance"),
];

export type LocationStatus = "locked" | "available" | "done";

const ZOOM_STOPS = [70, 90, 110, 130];

/** Idle-breathing frames of the party leader's own battle sprite (Party menu; Kael by default:
 * public/game/sprites/Kael_Final/kael-final-002, see assets.ts's "kaelFinal" entry). Only every
 * third frame of a 36-frame idle is used: plenty smooth at the size this renders (a small
 * JRPG-style overworld token), for a third of the image requests. Playback follows the
 * battle idle's forward/backward clock before sampling those frames. Crop transparent padding
 * per frame so every leader has the same visible height and feet line. */
const LEADER_MARKER: Record<AffinityHero, { dir: string; idle: number; bust: string }> = {
  Kael: { dir: "Kael_Final/kael-final-002", idle: 36, bust: "?v=kael-final-002" },
  Neera: { dir: "neera/neera-v2-001/idle-", idle: 36, bust: "?v=neera-v2-003" },
  Voss: { dir: "voss", idle: 12, bust: "" },
  Salazar: { dir: "salazar-final", idle: 36, bust: "" },
  Aldric: { dir: "aldric", idle: 36, bust: "?v=aldric-final-001" },
  Malrec: { dir: "malrec", idle: 36, bust: "" },
};

function LeaderMarker({ hero, facingLeft }: { hero: AffinityHero; facingLeft: boolean }) {
  const sheet = LEADER_MARKER[hero];
  const frames = sheet.idle === 36 ? Array.from({ length: 12 }, (_, i) => 1 + i * 3) : Array.from({ length: sheet.idle }, (_, i) => 1 + i);
  const [frame, setFrame] = useState(0);
  useEffect(() => {
    // Match BattleEngine.idleFrame: long sheets run at n / 3 frames per second;
    // short sheets complete a ping-pong cycle in 2.6 seconds. Malrec is 10% slower.
    const cycle = sheet.idle * 2 - 2;
    const pace = (sheet.idle >= 24 ? sheet.idle / 3 : cycle / 2.6) * (hero === "Malrec" ? 0.9 : 1);
    const startedAt = performance.now();
    setFrame(0);
    const id = window.setInterval(() => {
      const step = Math.floor((performance.now() - startedAt) / 1000 * pace) % cycle;
      const sourceFrame = step < sheet.idle ? step : cycle - step;
      setFrame(sheet.idle === 36 ? Math.floor(sourceFrame / 3) : sourceFrame);
    }, 1000 / pace);
    return () => window.clearInterval(id);
  }, [hero, sheet.idle]);
  const frameIndex = frame % frames.length;
  const sampledBounds = LEADER_FRAME_BOUNDS[hero];
  // Neera's and Salazar V2's sheets already have consistent body scale and a shared ground line.
  // Use one crop for the whole animation so changing poses cannot resize or recenter them.
  const bounds = hero === "Neera" || hero === "Salazar" ? (() => {
    const left = Math.min(...sampledBounds.map(b => b[0]));
    const top = Math.min(...sampledBounds.map(b => b[1]));
    const right = Math.max(...sampledBounds.map(b => b[0] + b[2]));
    const bottom = Math.max(...sampledBounds.map(b => b[1] + b[3]));
    return [left, top, right - left, bottom - top, sampledBounds[0][4], sampledBounds[0][5]];
  })() : sampledBounds[frameIndex];
  const [left, top, width, height, canvasWidth, canvasHeight] = bounds;
  const scale = 48 / height;
  return (
    <span className="flex h-12 w-16 items-end justify-center select-none drop-shadow-[0_2px_3px_rgba(0,0,0,0.6)]" style={{ transform: facingLeft ? "scaleX(-1)" : undefined }}>
      <span className="relative block shrink-0 overflow-hidden" style={{ width: width * scale, height: 48 }}>
    <img
      src={`/game/sprites/${sheet.dir}${hero === "Neera" ? "" : "/"}${frames[frame % frames.length]}.png${sheet.bust}`}
      alt=""
      draggable={false}
      className="absolute max-w-none"
      style={{ width: canvasWidth * scale, height: canvasHeight * scale, left: -left * scale, top: -top * scale }}
    />
      </span>
    </span>
  );
}

/** Portrait sprite per hero, same pairs as the party panel's own list. */
const HERO_PORTRAIT_SPRITE = { Kael: "kaelFinal", Neera: "neera", Voss: "voss", Salazar: "salazar", Aldric: "aldric", Malrec: "conjurer" } as const;

/** The RPG map: same background art and pan/zoom viewport as the classic map, but travel is
 * hex-by-hex instead of jumping straight to any unlocked location. The hex grid itself
 * (src/game/overworld.ts) is never drawn — only the party's current position (a small Kael
 * sprite) and the immediate neighbors it can step to are ever shown as interactive. Every
 * existing location pin still renders exactly where the classic map puts it; only whether a
 * click on it does anything depends on reachability this turn. */
export function OverworldMapScreen({
  locations,
  status,
  missionStatus,
  missionsOf = missionsForLocation,
  ember,
  test,
  muted,
  onMute,
  overworldPos,
  gameClock,
  rations,
  hungerStreak,
  heroHunger,
  save,
  onUseRation,
  onUseRationAll,
  onCastCreateFoodAndWater,
  onCastWarp,
  onEquipWeapon,
  onEquipItem,
  onUsePotion,
  onDiscardWeapon,
  onDiscardEquipment,
  onDiscardRation,
  onDiscardBagItem,
  inventoryRequestHero,
  inventoryRequestView,
  onInventoryRequestHandled,
  onOpenStatus,
  event,
  onDismissEvent,
  onStep,
  onTeleport,
  onBack,
  onSave,
  onLoad,
  onSaveFormation,
  onSaveLeader,
  onSetTravelTraining,
  onSeenTravelTrainingHint,
  onPick,
}: {
  locations: WorldLocation[];
  status: (loc: WorldLocation) => LocationStatus;
  missionStatus: (missionId: string) => LocationStatus;
  /** The missions of a location that are actually on the map right now (hidden, quest-gated
   * ones left out — see progression.ts). Defaults to every mission of the location. */
  missionsOf?: (loc: WorldLocation) => Mission[];
  ember: number;
  test: boolean;
  muted: boolean;
  onMute: () => void;
  overworldPos: { col: number; row: number };
  gameClock: number;
  rations: number;
  hungerStreak: number;
  heroHunger: Record<string, number>;
  save: SaveData;
  onUseRation: (hero: string) => void;
  onUseRationAll?: (heroes: string[]) => number;
  /** World-map cast of Create Food and Water (Healer tier 4) — returns false (and does
   * nothing) when the hero isn't a healer, has no tier-4 charges left, or is already at the
   * spell's own fullness target. See GameApp.tsx's castCreateFoodAndWater. */
  onCastCreateFoodAndWater?: (hero: string) => boolean;
  /** Warps a Mage to a city whose hex the campaign has already explored. */
  onCastWarp?: (hero: string, cityId: string) => boolean;
  /** Wired into the Mochila/Paperdoll's own equip picker — omitted for a while, which left
   * every tap there a silent no-op (see PartyInventoryOverlay below). */
  onEquipWeapon?: (hero: string, weaponId: string) => void;
  onEquipItem?: (hero: string, slot: EquipSlot, itemId: string | null) => void;
  onUsePotion?: (hero: string, kind: PotionId) => void;
  onDiscardWeapon?: (weaponId: string) => void;
  onDiscardEquipment?: (itemId: string) => void;
  onDiscardRation?: () => void;
  onDiscardBagItem?: (hero: string, kind: PotionId | "lockpick") => void;
  inventoryRequestHero?: string | null;
  inventoryRequestView?: "backpack" | "equipment";
  onInventoryRequestHandled?: () => void;
  onOpenStatus: (hero: string) => void;
  event: OverworldEvent | null;
  onDismissEvent: () => void;
  /** Commits one hex step (or a no-op re-click on the current hex) — day/ration/recovery
   * math lives in overworld.ts's stepOverworld, called by the parent. */
  onStep: (col: number, row: number) => void;
  /** Modo teste only: jumps straight to a non-adjacent pin, no day/supply cost. Never called
   * for a walkable (adjacent) pin — those always go through onStep instead, so the day
   * clock and rations stay visible and testable even in test mode. */
  onTeleport?: (col: number, row: number) => void;
  onBack: () => void;
  onSave?: () => void;
  onLoad?: () => void;
  /** Saves the formation board and reports whether it really landed (read back from the save). */
  onSaveFormation?: (order: string[]) => { ok: boolean; test: boolean };
  /** Party menu: saves the hero who walks the world map and free-roam maps. */
  onSaveLeader?: (hero: string) => void;
  /** Party menu Skills tab: picks (or clears, with null) the one skill a hero trains on the road. */
  onSetTravelTraining?: (hero: string, skill: SkillId | null) => void;
  /** Marks the one-time travel-training hint as seen. */
  onSeenTravelTrainingHint?: () => void;
  onPick: (missionId: string) => void;
}) {
  const [open, setOpen] = useState<WorldLocation | null>(null);
  const travelCost = (col: number, row: number) => travelTimeLabel(travelHoursForHex(col, row, locations));
  const timeOfDay = campaignTimeOfDay(campaignHour(save));
  const skyTint = timeOfDay === "darkNight"
    ? "rgba(5,12,43,0.68)"
    : timeOfDay === "brightNight"
      ? "rgba(34,62,132,0.48)"
      : timeOfDay === "dusk"
        ? "rgba(166,67,39,0.32)"
        : timeOfDay === "dawn"
          ? "rgba(230,137,112,0.25)"
          : timeOfDay === "noon"
            ? "rgba(255,197,108,0.12)"
            : "rgba(0,0,0,0)";
  const [questLogOpen, setQuestLogOpen] = useState(false);
  const [affinityOpen, setAffinityOpen] = useState(false);
  const [partyTab, setPartyTab] = useState<"group" | "skills">("group");
  const partyHeroes = AFFINITY_HEROES.filter(hero => test || heroRecruited(hero, save.completed, save.flags));
  const leader = partyLeaderOf(save.partyLeader, (hero) => test || heroRecruited(hero, save.completed, save.flags));
  const [formationOrder, setFormationOrder] = useState<string[]>([]);
  // Formation map: the slot picked first, waiting for a second click to swap with.
  const [formationPick, setFormationPick] = useState<number | null>(null);
  // Result of the last "Salvar formação": confirmed only after the save is read back.
  const [formationSaved, setFormationSaved] = useState<null | "saved" | "test" | "failed">(null);
  useEffect(() => {
    if (affinityOpen) {
      // The Type 7 board: saved cells in place (party members only), newcomers into the first free cell.
      const saved = cleanPartyFormation(save.partyFormation);
      const cells = Array.from({ length: FORMATION_SLOTS }, (_, i) => (saved[i] && partyHeroes.includes(saved[i] as typeof partyHeroes[number]) ? saved[i]! : ""));
      for (const hero of partyHeroes) if (!cells.includes(hero)) { const free = cells.indexOf(""); if (free >= 0) cells[free] = hero; }
      setFormationOrder(cells);
      setFormationPick(null);
    }
  }, [affinityOpen, save.partyFormation]);
  const [movementOpen, setMovementOpen] = useState(false);
  const [warpMenuHero, setWarpMenuHero] = useState<string | null>(null);

  const [inventoryHero, setInventoryHero] = useState<string | null>(null);
  const stepLock = useRef(false);
  useEffect(() => {
    if (!inventoryRequestHero) return;
    setInventoryHero(inventoryRequestHero);
    onInventoryRequestHandled?.();
  }, [inventoryRequestHero, onInventoryRequestHandled]);
  useEffect(() => {
    stepLock.current = false;
  }, [overworldPos.col, overworldPos.row]);
  useEffect(() => {
    const cancel = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setMovementOpen(false);
        setWarpMenuHero(null);
      }
    };
    window.addEventListener("keydown", cancel);
    return () => window.removeEventListener("keydown", cancel);
  }, []);
  const [artOk, setArtOk] = useState(true);
  const mapLoading = useMapLoading("/game/assets/world-map.jpg");
  useEffect(() => {
    if (mapLoading.visible) return;
    let idleId: number | null = null;
    let timerId: number | null = null;
    const idleWindow = window as unknown as {
      requestIdleCallback?: (callback: () => void, options?: { timeout: number }) => number;
      cancelIdleCallback?: (id: number) => void;
    };
    const warm = () => preloadPartyInventoryAssets(save);
    if (idleWindow.requestIdleCallback) idleId = idleWindow.requestIdleCallback(warm, { timeout: 1500 });
    else timerId = window.setTimeout(warm, 250);
    return () => {
      if (idleId != null) idleWindow.cancelIdleCallback?.(idleId);
      if (timerId != null) window.clearTimeout(timerId);
    };
  }, [mapLoading.visible, save.weapons, save.equipment, save.looseEquipment]);
  useEffect(() => {
    if (mapLoading.failed) setArtOk(false);
  }, [mapLoading.failed]);
  const [flashId, setFlashId] = useState<string | null>(null);
  const [hint, setHint] = useState<string | null>(null);
  const [zoomIdx, setZoomIdx] = useState(ZOOM_STOPS.length - 1);
  const [fieldSpellsOpen, setFieldSpellsOpen] = useState(false);
  const [warpVisual, setWarpVisual] = useState<"forming" | "open" | "closing" | null>(null);
  const [dragging, setDragging] = useState(false);
  const viewportRef = useRef<HTMLDivElement | null>(null);
  const dragRef = useRef<{ x: number; y: number; scrollLeft: number; scrollTop: number; moved: boolean } | null>(null);
  const centerFracRef = useRef({ x: 0.5, y: 0.5 });

  // Which way Kael last actually moved — kept across steps (not reset when standing still),
  // so re-clicking the current hex or arriving straight-up/down doesn't suddenly flip him.
  const [facingLeft, setFacingLeft] = useState(false);
  const prevPosRef = useRef(overworldPos);
  useEffect(() => {
    const prev = prevPosRef.current;
    if (overworldPos.col !== prev.col) setFacingLeft(overworldPos.col < prev.col);
    prevPosRef.current = overworldPos;
  }, [overworldPos.col, overworldPos.row]);

  const recenterOn = (fx: number, fy: number) => {
    const el = viewportRef.current;
    if (!el) return;
    el.scrollLeft = Math.max(0, Math.min(el.scrollWidth - el.clientWidth, fx * el.scrollWidth - el.clientWidth / 2));
    el.scrollTop = Math.max(0, Math.min(el.scrollHeight - el.clientHeight, fy * el.scrollHeight - el.clientHeight / 2));
  };
  const captureCenterFrac = () => {
    const el = viewportRef.current;
    if (!el || el.scrollWidth === 0 || el.scrollHeight === 0) return;
    centerFracRef.current = {
      x: (el.scrollLeft + el.clientWidth / 2) / el.scrollWidth,
      y: (el.scrollTop + el.clientHeight / 2) / el.scrollHeight,
    };
  };

  const mounted = useRef(false);
  useEffect(() => {
    if (!mounted.current) return;
    recenterOn(centerFracRef.current.x, centerFracRef.current.y);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [zoomIdx]);

  // Opens centered on the party's current hex, same idea as the classic map centering on
  // centerLocationId — just derived from overworldPos instead. Mount-only, so panning
  // afterward isn't fought on every step.
  useEffect(() => {
    const world = hexToWorld(overworldPos.col, overworldPos.row);
    centerFracRef.current = { x: world.x / 100, y: world.y / 100 };
    recenterOn(centerFracRef.current.x, centerFracRef.current.y);
    mounted.current = true;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const onPointerDown = (e: ReactPointerEvent<HTMLDivElement>) => {
    if (e.pointerType === "touch") return;
    const el = viewportRef.current;
    if (!el) return;
    dragRef.current = { x: e.clientX, y: e.clientY, scrollLeft: el.scrollLeft, scrollTop: el.scrollTop, moved: false };
    setDragging(true);
  };
  const onPointerMove = (e: ReactPointerEvent<HTMLDivElement>) => {
    const d = dragRef.current;
    const el = viewportRef.current;
    if (!d || !el) return;
    if (e.pointerType === "mouse" && e.buttons === 0) {
      dragRef.current = null;
      setDragging(false);
      return;
    }
    const dx = e.clientX - d.x;
    const dy = e.clientY - d.y;
    if (!d.moved && (Math.abs(dx) > 4 || Math.abs(dy) > 4)) {
      d.moved = true;
      el.setPointerCapture(e.pointerId);
    }
    if (!d.moved) return;
    el.scrollLeft = d.scrollLeft - dx;
    el.scrollTop = d.scrollTop - dy;
  };
  const endDrag = (e: ReactPointerEvent<HTMLDivElement>) => {
    const el = viewportRef.current;
    if (el?.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
    setDragging(false);
  };
  const onClickCapture = (e: ReactMouseEvent<HTMLDivElement>) => {
    if (dragRef.current?.moved) {
      e.preventDefault();
      e.stopPropagation();
      dragRef.current = null;
    }
  };

  const showHint = (text: string) => {
    setHint(text);
    window.setTimeout(() => setHint((h) => (h === text ? null : h)), 1200);
  };

  const playWarpGate = (level: number) => {
    const durationMs = warpPortalDuration(level) * 1000;
    setWarpVisual("forming");
    requestAnimationFrame(() => setWarpVisual("open"));
    window.setTimeout(() => setWarpVisual("closing"), durationMs * 0.7);
    window.setTimeout(() => setWarpVisual(null), durationMs);
  };

  const warpMenuLevel = warpMenuHero ? save.levels[warpMenuHero] ?? 1 : 1;
  const warpMenuCities = locations.filter((location) => {
    const hex = worldToHex(location.x, location.y);
    return (hex.x !== overworldPos.col || hex.y !== overworldPos.row) &&
      ((save.exploredHexes ?? []).includes(key(hex.x, hex.y)) || location.missionIds.some((id) => save.completed.includes(id)));
  });

  // Everything the party can step to right now: its own hex (re-clicking it just reopens
  // whatever's there, no day spent — see stepOverworld's same-hex no-op) plus its six
  // neighbors. The only adjacency rule the UI is allowed to know about.
  const reachable = useMemo(() => {
    const set = new Set<string>();
    set.add(key(overworldPos.col, overworldPos.row));
    for (const n of neighborsOf(overworldPos.col, overworldPos.row)) {
      if (canStepOverworld(save, { x: overworldPos.col, y: overworldPos.row }, n, test)) set.add(key(n.x, n.y));
    }
    return set;
  }, [overworldPos.col, overworldPos.row, save, test]);

  const wildDots = useMemo(
    () =>
      neighborsOf(overworldPos.col, overworldPos.row)
        .filter((n) => isOverworldCell(n.x, n.y) && canStepOverworld(save, { x: overworldPos.col, y: overworldPos.row }, n, test))
        .map((n) => ({ ...n, world: hexToWorld(n.x, n.y) })),
    [overworldPos.col, overworldPos.row, save, test],
  );

  // Standing exactly on a location's hex snaps the marker to that location's own authored
  // x/y (sub-hex precision) instead of the hex-center approximation, so Kael visibly lands
  // right on the pin rather than somewhere nearby within the same hex.
  const standingOn = useMemo(
    () => locations.find((l) => { const h = worldToHex(l.x, l.y); return h.x === overworldPos.col && h.y === overworldPos.row; }),
    [locations, overworldPos.col, overworldPos.row],
  );
  const partyWorld = hexToWorld(overworldPos.col, overworldPos.row);

  // Fog of war: every hex the party has ever stood on (see stepOverworld in overworld.ts).
  // The Inn is exempt from it entirely — always shown regardless — every other pin only
  // shows once its own hex is in this set. Test mode ignores fog like it ignores every
  // other travel restriction on this map.
  const exploredSet = useMemo(() => new Set(save.exploredHexes ?? []), [save.exploredHexes]);
  const isExplored = (loc: WorldLocation) => {
    if (!save.completed.includes("vau") && loc.id === "stonebridge") return true;
    if (test || loc.id === "estalagem" || loc.id === standingOn?.id) return true;
    const h = worldToHex(loc.x, loc.y);
    return exploredSet.has(key(h.x, h.y));
  };
  /** Percent-of-image reveal radius around one explored hex — a bit more than one hex's own
   * OVERWORLD_HEX_SIZE so the cleared patch reads as "the area around here," not just the
   * single dot the party stood on. */
  const FOG_REVEAL_RADIUS = 9;

  const walkTo = (col: number, row: number) => {
    if (!movementOpen || stepLock.current || !isOverworldCell(col, row)) return;
    if (!canStepOverworld(save, { x: overworldPos.col, y: overworldPos.row }, { x: col, y: row }, test)) return;
    stepLock.current = true;
    setMovementOpen(false);
    onStep(col, row);
    // Walking onto a location's hex only arrives there — it no longer pops the mission
    // list open on its own. The pin now reads as "standing here" (see standingOn) and
    // waits for its own click, same as any other pin, so arriving never yanks a panel
    // over the map before the player has looked around.
  };

  const enterLocation = (loc: WorldLocation, st: LocationStatus) => {
    if (locationExpired(loc, gameClock)) {
      showHint("Prazo esgotado por aqui.");
      return;
    }
    if (st === "locked") {
      setFlashId(loc.id);
      window.setTimeout(() => setFlashId((f) => (f === loc.id ? null : f)), 500);
      return;
    }
    const missions = missionsOf(loc);
    if (missions.length > 1) {
      setOpen(loc);
      return;
    }
    if (missions[0]) {
      // A crossing the party has already completed is optional on later visits: open its
      // panel so the player can pass through or deliberately return to explore it.
      if (isCrossingDungeon(missions[0]) && missionStatus(missions[0].id) === "done") setOpen(loc);
      else onPick(missions[0].id);
    }
  };

  return (
    <section className="relative h-dvh min-h-0 flex flex-col overflow-hidden bg-bg">
      <div className="absolute inset-0" style={{ background: "radial-gradient(ellipse at 30% 20%, #1e1e1e 0%, #0b0b0b 70%)" }} />
      {warpMenuHero && onCastWarp && (
        <div
          className="fixed inset-0 z-[75] grid place-items-center bg-black/60 p-4"
          role="presentation"
          onClick={() => setWarpMenuHero(null)}
        >
          <div
            className="relative aspect-square w-[min(86vw,380px)] rounded-full border border-[#dfbf8e]/45 bg-[#131313]/95 shadow-[0_0_48px_rgba(65,113,190,0.32),inset_0_0_36px_rgba(65,113,190,0.16)]"
            role="dialog"
            aria-modal="true"
            aria-label={`${uiText("Escolha o destino do Warp", { en: "Choose a Warp destination" })} · ${warpMenuHero}`}
            onClick={(event) => event.stopPropagation()}
          >
            <div className="absolute left-1/2 top-1/2 z-10 grid size-24 -translate-x-1/2 -translate-y-1/2 place-items-center rounded-full border border-[#dfbf8e]/50 bg-[#1c1c1c] p-2 text-center shadow-lg">
              <div>
                <img src={spellIcon("warp")} alt="" onError={(event) => { event.currentTarget.onerror = null; event.currentTarget.src = "/game/icons/warp.jpg"; }} className="mx-auto mb-1 size-9 rounded object-cover mix-blend-screen drop-shadow-[0_0_5px_rgba(74,150,255,0.7)]" />
                <p className="text-xs text-[#dfbf8e]">Warp</p>
                <p className="max-w-20 truncate text-[10px] text-muted">{warpMenuHero}</p>
              </div>
            </div>
            {warpMenuCities.map((city, index) => {
              const angle = (index / warpMenuCities.length) * Math.PI * 2 - Math.PI / 2;
              const left = 50 + Math.cos(angle) * 34;
              const top = 50 + Math.sin(angle) * 34;
              return (
                <button
                  key={city.id}
                  type="button"
                  className="absolute z-20 flex min-h-11 w-[clamp(92px,27vw,124px)] -translate-x-1/2 -translate-y-1/2 items-center justify-center rounded-full border border-[#dfbf8e]/60 bg-[#282828] px-2 text-center text-xs text-[#f0dfc5] shadow-md transition hover:bg-[#404040] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#dfbf8e]"
                  style={{ left: `${left}%`, top: `${top}%` }}
                  onClick={() => {
                    const ok = onCastWarp(warpMenuHero, city.id);
                    setWarpMenuHero(null);
                    if (ok) playWarpGate(warpMenuLevel);
                    showHint(ok ? `${warpMenuHero} atravessou para ${city.name}.` : "Não foi possível lançar Warp.");
                  }}
                >
                  {city.name}
                </button>
              );
            })}
            <button
              type="button"
              className="absolute bottom-[-3.25rem] left-1/2 -translate-x-1/2 rounded-md border border-white/20 bg-[#131313]/95 px-3 py-1.5 text-xs text-[#dfbf8e]"
              onClick={() => setWarpMenuHero(null)}
            >
              {uiText("Cancelar", { en: "Cancel" })}
            </button>
          </div>
        </div>
      )}
      {warpVisual && <div aria-hidden className="pointer-events-none fixed inset-0 z-[70] grid place-items-center bg-black/35">
        <img src={spellIcon("warp")} alt="" onError={(event) => { event.currentTarget.onerror = null; event.currentTarget.src = "/game/icons/warp.jpg"; }} draggable={false} className="h-[58dvh] max-h-[620px] w-auto select-none object-contain mix-blend-screen drop-shadow-[0_0_30px_rgba(74,150,255,0.8)]" style={{ opacity: warpVisual === "closing" ? 0 : warpVisual === "forming" ? 0.15 : 1, transform: warpVisual === "forming" ? "scale(0.42,0.5)" : warpVisual === "closing" ? "scale(1.08,1.12)" : "scale(1,1)", transition: "opacity 550ms ease-out, transform 950ms cubic-bezier(.18,.7,.26,1)" }} />
      </div>}

      <header className="relative z-20 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 flex-wrap">
        {test && <button type="button" onClick={onBack} className="size-10 grid place-items-center ember-icon-btn" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>}
        <div className="flex-1 min-w-0">
          <p className="text-sm ember-kicker">{test ? "Modo teste" : "Campanha"} · RPG</p>
          <h1 className="font-display text-3xl leading-none ember-title">Mapa</h1>
        </div>
        <button type="button" onClick={() => { setFormationSaved(null); setAffinityOpen(true); }} aria-haspopup="dialog" aria-expanded={affinityOpen} className="h-9 ember-plate px-2.5 text-xs sm:text-sm">Party</button>
        <button
          type="button"
          onClick={() => setQuestLogOpen(true)}
          aria-label="Registro de missões"
          className="h-10 inline-flex items-center gap-2 ember-plate px-3 text-sm"
        >
          <BookOpen className="size-4" />
          <span>Missões</span>
        </button>
        <p aria-label="Relógio do jogo" aria-live="polite" className="inline-flex items-center gap-2 text-sm text-muted ember-plate px-2 py-1">
          <Clock aria-hidden className="size-4" />
          <span>Dia <span className="text-fg tabular-nums">{gameClock} · {String(campaignHour(save)).padStart(2, "0")}:00</span></span>
        </p>
        <MoonPhaseBadge gameClock={gameClock} />
        <p className="text-sm text-muted ember-plate px-2 py-1">Rações <span className="text-fg tabular-nums">{rations}</span></p>
        {hungerStreak > 0 && (
          <p className="text-sm ember-plate px-2 py-1 text-danger">
            Fome <span className="tabular-nums">{hungerStreak}d</span>
          </p>
        )}
        {onUseRationAll && (
          <button
            type="button"
            className="h-9 px-3 ember-plate text-sm"
            onClick={() => {
              const heroes = (["Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"] as const).filter(
                (name) => test || heroRecruited(name, save.completed, save.flags),
              );
              const fed = onUseRationAll(heroes);
              showHint(
                fed === 0
                  ? "Ninguém comeu — sem rações ou já saciados."
                  : fed === heroes.length
                    ? "Todos comeram."
                    : `${fed} comeram — rações não deram pros demais.`,
              );
            }}
          >
            Alimentar todos
          </button>
        )}
        {(onCastCreateFoodAndWater || onCastWarp) && (
          <div className="relative">
            <button
              type="button"
              onClick={() => setFieldSpellsOpen((o) => !o)}
              className="size-11 overflow-hidden rounded-md bg-transparent p-0 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
              aria-label={uiText("Feitiços de campo", { en: "Field spells" })}
              aria-expanded={fieldSpellsOpen}
            >
              <img src="/game/icons/field-spells.png" alt="" draggable={false} className="block h-full w-full object-cover" />
            </button>
            {fieldSpellsOpen && (
              <div className="absolute right-0 top-full mt-2 w-72 ember-plate p-3 flex flex-col gap-2 z-20">
                <p className="text-xs ember-kicker">{uiText("Feitiços de campo", { en: "Field spells" })}</p>
                {(() => {
                  const healers = (["Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"] as const).filter((name) => {
                    if (!(test || heroRecruited(name, save.completed, save.flags))) return false;
                    if ((save.unitHp[name] ?? 1) <= 0) return false;
                    const classId = save.promotions[name] ?? { Kael: "swordsman", Neera: "archer", Voss: "mage", Salazar: "healer", Aldric: "aldric", Malrec: "conjurer" }[name];
                    return rulesClass(classId) === "healer";
                  });
                  const mages = (Object.keys(HERO_PORTRAIT_SPRITE) as (keyof typeof HERO_PORTRAIT_SPRITE)[]).filter((name) => {
                    if (!(test || heroRecruited(name, save.completed, save.flags)) || (save.unitHp[name] ?? 1) <= 0) return false;
                    const classId = save.promotions[name] ?? { Kael: "swordsman", Neera: "archer", Voss: "mage", Salazar: "healer", Aldric: "aldric", Malrec: "conjurer" }[name];
                    return rulesClass(classId) === "mage" && (save.levels[name] ?? 1) >= WARP.unlockLevel;
                  });
                  const cities = locations.filter((location) => {
                    const hex = worldToHex(location.x, location.y);
                    if (hex.x === overworldPos.col && hex.y === overworldPos.row) return false;
                    return (save.exploredHexes ?? []).includes(key(hex.x, hex.y)) || location.missionIds.some((id) => save.completed.includes(id));
                  });
                  return <>
                  {onCastCreateFoodAndWater && healers.map((name) => {
                    const level = save.levels[name] ?? 1;
                    const classId = save.promotions[name] ?? "healer";
                    const spent = save.spellUses[name]?.tier4 ?? 0;
                    const remaining = Math.max(0, tierUses(classId, 4, level) - spent);
                    const power = createFoodAndWaterPower(level);
                    const alreadyFull = fullness(save.heroHunger[name]) >= power.fullness;
                    const disabled = remaining <= 0 || alreadyFull;
                    return (
                      <div key={name} className="flex items-center justify-between gap-2 text-sm">
                        <div className="min-w-0">
                          <p className="text-fg truncate">{CREATE_FOOD_AND_WATER.name}</p>
                          <p className="text-xs text-muted truncate">{name} (Lv {level}) · {createFoodAndWaterFormula(level)} · {remaining}x</p>
                        </div>
                        <button
                          type="button"
                          disabled={disabled}
                          className="h-8 px-2 ember-plate text-xs disabled:opacity-40"
                          onClick={() => {
                            const ok = onCastCreateFoodAndWater(name);
                            showHint(ok ? `${name} restaurou a fome.` : "Não foi possível lançar.");
                          }}
                        >
                          Lançar
                        </button>
                      </div>
                    );
                  })}
                  {onCastWarp && mages.map((name) => {
                    const level = save.levels[name] ?? 1;
                    const classId = save.promotions[name] ?? "mage";
                    const remaining = Math.max(0, tierUses(classId, WARP.tier, level) - (save.spellUses[name]?.tier3 ?? 0));
                    return <div key={`warp-${name}`} className="border-t border-[var(--ember-border)] pt-2">
                      <button
                        type="button"
                        disabled={remaining <= 0 || cities.length === 0}
                        aria-label={`${uiText("Warp", { en: "Warp" })} · ${name} · ${remaining}x`}
                        className="flex w-full items-center gap-2 rounded-md p-1 text-left text-sm hover:bg-surface-2 disabled:opacity-40"
                        onClick={() => {
                          setFieldSpellsOpen(false);
                          setWarpMenuHero(name);
                        }}
                      >
                        <img src={spellIcon("warp")} alt="" onError={(event) => { event.currentTarget.onerror = null; event.currentTarget.src = "/game/icons/warp.jpg"; }} className="size-9 rounded object-cover mix-blend-screen drop-shadow-[0_0_5px_rgba(74,150,255,0.7)]" />
                        <span className="text-fg">{uiText("Warp", { en: "Warp" })}</span>
                        <span className="ml-auto text-xs text-muted">{name} · Lv {level} · {remaining}x</span>
                      </button>
                    </div>;
                  })}
                  {onCastWarp && mages.length > 0 && cities.length === 0 && <p className="text-xs text-subtle">{uiText("Nenhum outro local visitado.", { en: "No other visited locations." })}</p>}
                  {healers.length === 0 && mages.length === 0 && <p className="text-sm text-subtle">{uiText("Nenhum feitiço de campo disponível.", { en: "No field spells available." })}</p>}
                  </>;
                })()}
              </div>
            )}
          </div>
        )}
        <OptionsButton
          muted={muted}
          onMute={onMute}
          iconOnly
          escapeKey
          onSaveGame={!test ? onSave : undefined}
          onLoadGame={onLoad}
          onReturnToTitle={onBack}
        />
        <p className="text-sm text-muted ember-plate px-2 py-1"><GoldAmount amount={ember} /></p>
      </header>

      <div
        ref={viewportRef}
        className={`relative z-10 flex-1 min-h-0 overflow-auto overscroll-contain touch-pan-x touch-pan-y select-none${dragging ? " world-map-pan-grab" : " cursor-default"}`}
        style={{ WebkitOverflowScrolling: "touch" }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onPointerLeave={endDrag}
        onClickCapture={onClickCapture}
      >
        <div className="relative inline-block m-2" style={{ width: artOk ? `${ZOOM_STOPS[zoomIdx]}%` : undefined }}>
          {artOk && mapLoading.imageSrc ? (
            <img
              src={mapLoading.imageSrc}
              alt=""
              className="block w-full h-auto rounded-lg select-none"
              draggable={false}
              onError={() => { setArtOk(false); mapLoading.fail(); }}
              onLoad={() => {
                recenterOn(centerFracRef.current.x, centerFracRef.current.y);
                window.requestAnimationFrame(() => window.requestAnimationFrame(mapLoading.finish));
              }}
            />
          ) : (
            <div className="w-[70dvw] h-[70dvh] max-w-md" />
          )}
          {artOk && !test && (
            // Fog of war: dark everywhere except a soft radius around every hex the party
            // has ever stood on (see exploredSet above). Test mode skips this like it skips
            // every other travel restriction on this map — testing needs the whole map
            // visible, not walked hex by hex. An SVG mask rather than CSS mask-composite:
            // browser support for compositing many stacked mask layers is inconsistent,
            // while an SVG <mask> just paints shapes on top of each other, so it works the
            // same everywhere.
            <svg viewBox="0 0 100 100" preserveAspectRatio="none" className="pointer-events-none absolute inset-0 h-full w-full">
              <defs>
                <radialGradient id="ow-fog-reveal">
                  <stop offset="0%" stopColor="#000" stopOpacity="1" />
                  <stop offset="65%" stopColor="#000" stopOpacity="1" />
                  <stop offset="100%" stopColor="#000" stopOpacity="0" />
                </radialGradient>
                <mask id="ow-fog-mask" maskContentUnits="userSpaceOnUse">
                  <rect x="0" y="0" width="100" height="100" fill="#fff" />
                  {[...exploredSet].map((hexKey) => {
                    const [hx, hy] = hexKey.split(",").map(Number);
                    if (!Number.isFinite(hx) || !Number.isFinite(hy)) return null;
                    const p = hexToWorld(hx, hy);
                    return <circle key={hexKey} cx={p.x} cy={p.y} r={FOG_REVEAL_RADIUS} fill="url(#ow-fog-reveal)" />;
                  })}
                </mask>
              </defs>
              <rect x="0" y="0" width="100" height="100" fill="rgba(6, 6, 6,0.78)" mask="url(#ow-fog-mask)" />
            </svg>
          )}
          {artOk && <div aria-hidden className="pointer-events-none absolute inset-0 transition-colors duration-[1500ms]" style={{ backgroundColor: skyTint }} />}
          <div className="absolute inset-0">
            {locations.filter(isExplored).map((loc) => {
              const st = status(loc);
              const missions = missionsOf(loc);
              const multi = missions.length > 1;
              const hex = worldToHex(loc.x, loc.y);
              const walkable = reachable.has(key(hex.x, hex.y));
              // Modo teste: every pin is clickable, no walking required — testing needs to
              // jump straight to any location, the same freedom the classic map already
              // gives it. A walkable (adjacent) pin still walks normally even in test mode,
              // so the day-clock/rations math stays visible and testable there too; only a
              // distant pin gets the free teleport.
              const openingVau = !test && !save.completed.includes("vau") && loc.id === "stonebridge";
              const isReachable = test || walkable || openingVau;
              const expired = locationExpired(loc, gameClock);
              return (
                <button
                  key={loc.id}
                  type="button"
                  onClick={() => {
                    if (openingVau) { onPick("vau"); return; }
                    if (loc.id === standingOn?.id) { enterLocation(loc, st); return; }
                    if (walkable) { walkTo(hex.x, hex.y); return; }
                    if (!isReachable) {
                      showHint("Ande até lá primeiro.");
                      return;
                    }
                    onTeleport?.(hex.x, hex.y);
                    enterLocation(loc, st);
                  }}
                  className={`group absolute -translate-x-1/2 -translate-y-1/2 ${isReachable ? "" : "opacity-70"}`}
                  style={{ left: `${loc.x}%`, top: `${loc.y}%` }}
                  aria-label={st === "locked" ? `${loc.name} (bloqueado)` : loc.name}
                >
                  <span
                    className={`relative size-10 rounded-full border-2 grid place-items-center bg-bg/80 transition-transform group-hover:scale-110 group-active:scale-95 ${
                      st === "locked"
                        ? `border-border opacity-50 ${loc.id === flashId ? "locked-flash" : ""}`
                        : st === "done"
                          ? "border-accent"
                          : missions.some((m) => m.hub)
                            ? "inn-open"
                            : "border-accent"
                    } ${expired ? "border-dashed" : ""}`}
                  >
                    {st === "locked" ? (
                      <Lock className="size-4 text-muted" />
                    ) : st === "done" ? (
                      <Check className="size-4 text-accent" />
                    ) : (
                      <MapPin className="size-4 text-accent" />
                    )}
                    {multi && (
                      <span className="absolute -top-1.5 -right-1.5 size-4 rounded-full bg-bg border border-border text-[10px] leading-none grid place-items-center text-fg/90">
                        {missions.length}
                      </span>
                    )}
                  </span>
                </button>
              );
            })}

            {/* Wild-hex stepping stones: the invisible grid's only visible trace, and only
                right around the party — not pre-authored pins, so they appear and vanish as
                it moves instead of cluttering the whole map. */}
            {movementOpen && wildDots.map((dot) => (
              <button
                key={key(dot.x, dot.y)}
                type="button"
                onClick={() => walkTo(dot.x, dot.y)}
                className="overworld-step absolute -translate-x-1/2 -translate-y-1/2"
                style={{ left: `${dot.world.x}%`, top: `${dot.world.y}%`, width: `${Math.sqrt(3) * 5}%`, height: "10%" }}
                title={`Saciedade: −${travelHungerCost(travelHoursForHex(dot.x, dot.y, locations))}`}
                aria-label={`Andar para ${dot.x}, ${dot.y} · ${travelCost(dot.x, dot.y)} · saciedade −${travelHungerCost(travelHoursForHex(dot.x, dot.y, locations))}`}
              >
                <span>{travelCost(dot.x, dot.y)}</span>
              </button>
            ))}

            {/* The party's own marker — the leader's tiny idle sprite, sliding hex to hex as
                the party steps (the transition is what reads as "movement": there's no
                walk-cycle art in use here, just the idle loop, so distance covered does the talking). */}
            <button
              type="button"
              aria-label={`Mover ${leader}`}
              aria-expanded={movementOpen}
              onClick={() => {
                setMovementOpen((value) => !value);
              }}
              className="absolute z-20 -translate-x-1/2 -translate-y-full min-w-11 min-h-11 transition-all duration-500 ease-in-out focus-visible:outline-2 focus-visible:outline-accent"
              style={{ left: `${partyWorld.x}%`, top: `${partyWorld.y}%` }}
            >
              <LeaderMarker hero={leader} facingLeft={facingLeft} />
            </button>
          </div>
        </div>
      </div>

      {affinityOpen && (
        <div className="absolute inset-0 z-40 ember-veil flex items-center justify-center p-4" onClick={(event) => { if (event.target === event.currentTarget) setAffinityOpen(false); }}>
          <section role="dialog" aria-modal="true" aria-labelledby="party-title" className="relative flex max-h-[82dvh] w-full max-w-3xl flex-col overflow-hidden ember-panel">
            <header className="flex items-start justify-between gap-3 border-b border-[#525252]/60 p-4">
              <div>
                <p className="text-xs ember-kicker">Grupo · {partyHeroes.length} {partyHeroes.length === 1 ? "membro" : "membros"} · Líder: {leader}</p>
                <h2 id="party-title" className="mt-1 font-display text-2xl ember-title">Party</h2>
                <div role="tablist" aria-label="Seções do grupo" className="mt-3 flex gap-2">
                  {([["group", "Grupo"], ["skills", "Skills"]] as const).map(([id, label]) => (
                    <button key={id} type="button" role="tab" aria-selected={partyTab === id} onClick={() => setPartyTab(id)} className={`ember-btn ember-btn-sm ${partyTab === id ? "ember-btn-primary" : "ember-btn-ghost"}`}>{label}</button>
                  ))}
                </div>
              </div>
              <button type="button" onClick={() => setAffinityOpen(false)} aria-label="Fechar Party" className="grid size-9 place-items-center ember-icon-btn">
                <X className="size-4" />
              </button>
            </header>
            <div className="overflow-y-auto p-4">
              {partyTab === "skills" ? (
                <section aria-label="Skills">
                  <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                    {partyHeroes.map(hero => (
                      <article key={hero} aria-label={`Skills de ${hero}`} className="ember-slot p-3">
                        <h4 className="mb-1 font-display text-lg leading-tight ember-title">{hero}</h4>
                        {(() => {
                          const training = save.travelTraining?.[hero];
                          if (!training) return <p className="mb-2 text-xs text-muted">Treino na estrada: nenhum — toque em “Treinar” numa skill.</p>;
                          const banked = (save.travelTrainingHours?.[hero] ?? 0) % TRAVEL_TRAINING_HOURS;
                          const left = Math.max(1, Math.ceil(TRAVEL_TRAINING_HOURS - banked));
                          return <p className="mb-2 text-xs text-accent">Treino na estrada: {SKILLS[training].name} · próximo +{SKILL_GAIN.toLocaleString("pt-BR")} em {left}h de viagem</p>;
                        })()}
                        {PARTY_SKILL_ORDER.filter(id => !id.endsWith("Weapon") || weaponTypesForClass(mapHeroClass(hero, save)).includes(id.slice(0, -"Weapon".length) as WeaponType)).map(id => {
                          const points = skillValue(save.heroSkills, hero, id);
                          const training = save.travelTraining?.[hero] === id;
                          return <div key={id} className="mb-3 last:mb-0" title={SKILLS[id].description}>
                            <div className="flex items-center justify-between gap-2 text-sm"><span>{SKILLS[id].name}{id === "healing" ? ` · +${points.toLocaleString("pt-BR", { maximumFractionDigits: 2 })}% healing` : ""}</span>
                              <button type="button" disabled={!onSetTravelTraining} aria-pressed={training} aria-label={`${training ? "Parar de treinar" : "Treinar"} ${SKILLS[id].name} de ${hero} na estrada`} onClick={() => onSetTravelTraining?.(hero, training ? null : id)}
                                className={`ml-auto ember-btn ember-btn-sm ${training ? "ember-btn-primary" : "ember-btn-ghost"}`}>{training ? "Treinando" : "Treinar"}</button>
                              <span className="text-muted tabular-nums">{points.toLocaleString("pt-BR", { minimumFractionDigits: 0, maximumFractionDigits: 2 })}/{SKILL_CAP}</span></div>
                            {id.endsWith("Weapon") && <p className="text-xs text-muted">Dados da arma: +{points}%. Precisão contra DEX 0: {Math.min(100, 75 + points)}%. A DEX do alvo reduz a precisão. Ao atacar um inimigo 5 níveis abaixo, o ganho é 0,05; com 10 níveis abaixo, não há ganho.</p>}
                            <div role="progressbar" aria-label={`${SKILLS[id].name} de ${hero}`} aria-valuemin={0} aria-valuemax={SKILL_CAP} aria-valuenow={points} className="my-1 h-1.5 overflow-hidden rounded-full bg-border">
                              <div className="h-full bg-accent" style={{ width: `${Math.max(0, Math.min(100, (points / SKILL_CAP) * 100))}%` }} />
                            </div>
                          </div>;
                        })}
                      </article>
                    ))}
                  </div>
                  <p className="mt-2 text-xs text-muted">Perícias de arma podem ganhar +{SKILL_GAIN.toLocaleString("pt-BR")} por tentativa contra inimigos, inclusive erros; cada ponto dá +1 ponto percentual de precisão e +1% aos dados da arma. Resistências podem ganhar +{SKILL_GAIN.toLocaleString("pt-BR")} ao usar o elemento ou ser atingido por ele. Magias dos familiares também treinam a resistência do conjurador ao elemento usado. Em magias de área, cada inimigo atingido permite uma tentativa de ganho para o conjurador. Quanto maior a perícia, menor a chance de ganho. Cada ponto de resistência vale 1%.</p>
                  <p className="mt-1 text-xs text-muted">Healing pode ganhar +{SKILL_GAIN.toLocaleString("pt-BR")} ao recuperar HP com magias ou poções e ao curar doenças. Cada ponto dá +1% ao total de HP recuperado, usando a perícia do conjurador ou de quem entrega a poção. Exemplo: Healing 11,4 dá +11,4%; o resultado final é arredondado para baixo. Curar doenças treina Healing, mas não recebe bônus no efeito. Bless e poções de mana não são afetados.</p>
                  <p className="mt-1 text-xs text-muted">Treino na estrada: cada personagem pode treinar uma skill por vez enquanto o grupo viaja — +{SKILL_GAIN.toLocaleString("pt-BR")} a cada {TRAVEL_TRAINING_HOURS}h de viagem. Trocar de skill recomeça a contagem.</p>
                </section>
              ) : (
              <div className="flex flex-col gap-4">
                <section aria-label="Líder do grupo">
                  <h3 className="mb-2 text-sm ember-kicker">Líder do grupo</h3>
                  <div className="ember-slot p-3">
                    <p className="mb-3 text-sm text-muted">O líder anda pelo mapa e pelas áreas livres.</p>
                    <div className="flex flex-wrap gap-2">
                      {partyHeroes.map(hero => (
                        <button key={hero} type="button" disabled={!onSaveLeader} aria-pressed={leader === hero} onClick={() => onSaveLeader?.(hero)}
                          className={`ember-slot flex w-16 flex-col items-center gap-1 p-1.5 text-xs disabled:opacity-60 ${leader === hero ? "is-last text-accent" : ""}`}>
                          <img src={portraitFor(HERO_PORTRAIT_SPRITE[hero]).src} alt="" style={{ objectPosition: portraitFor(HERO_PORTRAIT_SPRITE[hero]).position }} className="h-12 w-10 rounded object-cover" />
                          {hero}
                        </button>
                      ))}
                    </div>
                  </div>
                </section>
                <section aria-label="Formação inicial">
                  <h3 className="mb-2 text-sm ember-kicker">Formação inicial</h3>
                  <div className="ember-slot p-3">
                    <p className="mb-3 text-sm text-muted">A linha de frente começa nas posições mais próximas do inimigo; a retaguarda, nas mais distantes. Clique em duas casas para trocá-las de lugar (vale a casa vazia). Mapas com aberturas perigosas ou posições especiais preservam sua formação própria.</p>
                    <div className="mb-3 flex flex-wrap items-center gap-3">
                      <button type="button" className="h-11 px-5 ember-btn ember-btn-primary text-sm" onClick={() => { const result = onSaveFormation?.(formationOrder); setFormationSaved(!result ? "failed" : !result.ok ? "failed" : result.test ? "test" : "saved"); }}>Salvar formação</button>
                      {formationSaved === "saved" && <span role="status" className="text-sm text-accent">✓ Formação salva — conferida no arquivo do save.</span>}
                      {formationSaved === "test" && <span role="status" className="text-sm text-accent">✓ Formação salva nesta sessão de teste (o modo teste nunca grava no save real).</span>}
                      {formationSaved === "failed" && <span role="alert" className="text-sm text-danger">Não foi possível salvar a formação. Tente de novo.</span>}
                    </div>
                    {(() => {
                      // Radius-2 board (rows 3-4-5-4-3, center cell 10) — cells numbered front to back.
                      const cells = Array.from({ length: FORMATION_SLOTS }, (_, i) => formationOrder[i] ?? "");
                      const rows = [
                        { label: "Frente", slots: [0, 1, 2] },
                        { label: "", slots: [3, 4, 5, 6] },
                        { label: "Meio", slots: [7, 8, 9, 10, 11] },
                        { label: "", slots: [12, 13, 14, 15] },
                        { label: "Retaguarda", slots: [16, 17, 18] },
                      ].map(row => ({ label: row.label, slots: row.slots.map(slot => ({ hero: cells[slot]!, slot })) }));
                      const hex = "polygon(50% 0, 100% 25%, 100% 75%, 50% 100%, 0 75%, 0 25%)";
                      const pick = (slot: number) => {
                        if (formationPick === null) return setFormationPick(slot);
                        const from = formationPick;
                        setFormationPick(null);
                        if (from === slot || (!cells[from] && !cells[slot])) return;
                        setFormationOrder(cells.map((hero, i) => (i === from ? cells[slot]! : i === slot ? cells[from]! : hero)));
                        setFormationSaved(null);
                      };
                      return (
                        <div role="group" aria-label="Mapa da formação" className="mb-3 flex flex-col items-center">
                          <p className="mb-1 text-xs ember-kicker">Inimigo ▲</p>
                          <div className="flex flex-col">
                            {rows.map((row, r) => (
                              <div key={r} className={`flex items-center gap-1 ${r ? "-mt-5" : ""}`}>
                                <span className="w-20 shrink-0 pr-2 text-right text-xs ember-kicker">{row.label}</span>
                                {row.slots.length < 5 && <span aria-hidden="true" className="shrink-0" style={{ width: (5 - row.slots.length) * 44 }} />}
                                {row.slots.map(({ hero, slot }) => !hero ? (
                                  <button key={slot} type="button" aria-pressed={formationPick === slot} aria-label={`${row.label}, posição ${slot + 1}: vazia`} onClick={() => pick(slot)}
                                    className="grid h-[96px] w-[84px] place-items-center"
                                    style={{ clipPath: hex, background: formationPick === slot ? "#d9621c" : "rgba(190, 150, 95, 0.25)" }}>
                                    <span className="flex h-[92px] w-[80px] flex-col items-center justify-center gap-0.5 text-[11px] leading-none text-muted" style={{ clipPath: hex, background: "linear-gradient(180deg, #0c0c0c, #050505)" }}>
                                      <span>vazia</span>
                                      <span className="tabular-nums">{slot + 1}</span>
                                    </span>
                                  </button>
                                ) : (
                                  <button key={slot} type="button" aria-pressed={formationPick === slot} aria-label={`${row.label}, posição ${slot + 1}: ${hero}`} onClick={() => pick(slot)}
                                    className="grid h-[96px] w-[84px] place-items-center"
                                    style={{ clipPath: hex, background: formationPick === slot ? "#d9621c" : "rgba(190, 150, 95, 0.45)" }}>
                                    <span className="flex h-[92px] w-[80px] flex-col items-center justify-center gap-0.5 text-[11px] leading-none" style={{ clipPath: hex, background: "linear-gradient(180deg, #0c0c0c, #050505)", color: formationPick === slot ? "#ffe0b4" : "#e2c294" }}>
                                      <img src={portraitFor(HERO_PORTRAIT_SPRITE[hero as AffinityHero]).src} alt="" style={{ objectPosition: portraitFor(HERO_PORTRAIT_SPRITE[hero as AffinityHero]).position }} className="h-10 w-9 rounded object-cover" />
                                      <span>{hero}</span>
                                      <span className="text-muted tabular-nums">{slot + 1}</span>
                                    </span>
                                  </button>
                                ))}
                              </div>
                            ))}
                          </div>
                        </div>
                      );
                    })()}
                  </div>
                </section>
                <section aria-label="Afinidades">
                  <h3 className="mb-2 text-sm ember-kicker">Afinidades</h3>
                  <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                    {partyHeroes.map(a => (
                      <article key={a} aria-label={`Afinidades de ${a}`} className="ember-slot p-3">
                        <h4 className="mb-2 font-display text-lg leading-tight ember-title">{a}</h4>
                        {partyHeroes.filter(b => b !== a).map(b => {
                          const points = affinityScore(save.affinityScores, a, b);
                          return <div key={`${a}-${b}`} className="mb-3 last:mb-0">
                            <div className="flex justify-between gap-2 text-sm"><span>{b}</span><span className="text-muted tabular-nums">{affinityGrade(points)} · {points.toLocaleString("pt-BR", { maximumFractionDigits: 1 })}/100</span></div>
                            <div role="progressbar" aria-label={`Afinidade de ${a} com ${b}`} aria-valuemin={0} aria-valuemax={100} aria-valuenow={points} className="my-1 h-1.5 overflow-hidden rounded-full bg-border">
                              <div className="h-full bg-accent" style={{ width: `${Math.max(0, Math.min(100, points))}%` }} />
                            </div>
                            <AffinityBonusHelp points={points} duo={canUseAffinityDuo(save.affinityScores, a, b)} />
                          </div>;
                        })}
                        {partyHeroes.length < 2 && <p className="text-sm text-muted">As afinidades aparecerão quando outro personagem entrar no grupo.</p>}
                      </article>
                    ))}
                  </div>
                  {partyHeroes.flatMap((a, i, heroes) => heroes.slice(i + 1).flatMap((b, j) => heroes.slice(i + j + 2).filter(c => canUseAffinityUltimate(save.affinityScores, [a, b, c])).map(c => <p key={`${a}-${b}-${c}`} className="mt-2 text-sm text-accent">Ultimate disponível: {a} + {b} + {c}</p>)))}
                  <p className="mt-2 text-xs text-muted">25: +2% · 50: +5% · 80: skill de dupla · 90: +8% · 100 nas três relações: Ultimate de trio</p>
                </section>
              </div>
              )}
            </div>
          </section>
        </div>
      )}
      {hint && (
        <div className="absolute z-30 top-24 left-1/2 -translate-x-1/2 ember-plate px-3 py-1.5 text-xs">
          {hint}
        </div>
      )}
      {/* One-time tip once A Ponte de Pedra is cleared: points the player at Party → Skills. */}
      {onSeenTravelTrainingHint && save.completed.includes("thebridge") && !save.flags?.includes(TRAVEL_TRAINING_HINT_FLAG) && !affinityOpen && (
        <aside role="note" aria-label="Dica: treino na estrada" className="absolute z-30 top-36 left-1/2 w-[min(30rem,calc(100%-2rem))] -translate-x-1/2 ember-panel p-4">
          <h3 className="font-display text-lg ember-title">Dica · Treino na estrada</h3>
          <p className="mt-1 text-sm text-muted">Abra o menu <strong className="text-accent">Party</strong> e explore a aba <strong className="text-accent">Skills</strong>. Toque em “Treinar” numa skill de cada personagem: ela sobe sozinha enquanto o grupo viaja, +{SKILL_GAIN.toLocaleString("pt-BR")} a cada {TRAVEL_TRAINING_HOURS}h de estrada. Uma skill por vez — nada treina até você escolher.</p>
          <div className="mt-3 flex justify-end gap-2">
            <button type="button" onClick={() => onSeenTravelTrainingHint()} className="ember-btn ember-btn-sm ember-btn-ghost">Entendi</button>
            <button type="button" onClick={() => { onSeenTravelTrainingHint(); setFormationSaved(null); setPartyTab("skills"); setAffinityOpen(true); }} className="ember-btn ember-btn-sm ember-btn-primary">Abrir Party · Skills</button>
          </div>
        </aside>
      )}

      <div className="map-party-panel absolute z-20 bottom-4 left-4 rounded-lg border border-border p-3 max-w-[calc(100%-6rem)]">
        <p className="text-xs text-muted mb-2" aria-live="polite">
          {!test && wispForestUncleared(save) && overworldPos.col === wispForestHex()?.x && overworldPos.row === wispForestHex()?.y
            ? "Limpe a Wisp Forest para poder seguir viagem para o leste."
            : movementOpen ? "Escolha um hexágono · o tempo depende do terreno" : `Clique em ${leader} para mover`}
        </p>
        <div className="flex gap-3">
          {([['Kael', 'kaelFinal'], ['Neera', 'neera'], ['Voss', 'voss'], ['Salazar', 'salazar'], ['Aldric', 'aldric'], ['Malrec', 'conjurer']] as const).filter(([name]) => test || heroRecruited(name, save.completed, save.flags)).map(([name, sprite]) => (
            <div key={name} className="w-10" title={name}>
              <button type="button" aria-label={`Inventário de ${name}`} onClick={() => setInventoryHero(name)} className="min-h-11">
                <img src={portraitFor(sprite).src} alt={name} style={{ objectPosition: portraitFor(sprite).position }} className="w-10 h-12 object-cover rounded" />
              </button>
              <HungerBar name={name} value={heroHunger[name]} travel />
              <LifeBar name={name} hp={save.unitHp[name] ?? mapHeroMaxHp(name, save)} maxHp={mapHeroMaxHp(name, save)} poisonTier={poisonTierOf(save.heroPoisons?.[name])} diseased={!!save.heroDiseases?.[name]} />
            </div>
          ))}
        </div>
        {standingOn && <button className="text-xs text-accent mt-2 min-h-11" onClick={() => enterLocation(standingOn, status(standingOn))}>Explorar {standingOn.name}</button>}
      </div>

      {event && (
        <div className="absolute inset-0 z-40 ember-veil flex items-center justify-center p-4" role="dialog" aria-modal="true" aria-label="Evento da viagem">
          <div className="relative w-full max-w-sm ember-panel px-5 py-4 text-sm">
            <p>{event.text}</p>
            <button type="button" onClick={onDismissEvent} className="mt-4 h-11 w-full ember-btn ember-btn-primary text-sm">
              OK
            </button>
          </div>
        </div>
      )}


      {artOk && (
        <div className="absolute z-20 bottom-[max(1rem,env(safe-area-inset-bottom))] right-4 flex flex-col gap-1 ember-plate p-1">
          <button
            type="button"
            onClick={() => {
              captureCenterFrac();
              setZoomIdx((i) => Math.min(ZOOM_STOPS.length - 1, i + 1));
            }}
            disabled={zoomIdx >= ZOOM_STOPS.length - 1}
            className="size-11 grid place-items-center rounded-md disabled:opacity-30 active:bg-surface-2"
            aria-label="Aproximar"
          >
            <ZoomIn className="size-5" />
          </button>
          <div className="text-center text-[10px] tabular-nums text-muted py-0.5">
            {ZOOM_STOPS.length - zoomIdx}/{ZOOM_STOPS.length}
          </div>
          <button
            type="button"
            onClick={() => {
              captureCenterFrac();
              setZoomIdx((i) => Math.max(0, i - 1));
            }}
            disabled={zoomIdx <= 0}
            className="size-11 grid place-items-center rounded-md disabled:opacity-30 active:bg-surface-2"
            aria-label="Afastar"
          >
            <ZoomOut className="size-5" />
          </button>
        </div>
      )}

      <MapLoadingOverlay progress={mapLoading.progress} visible={mapLoading.visible} />

      {questLogOpen && (
        <QuestLogPanel save={save} onClose={() => setQuestLogOpen(false)} />
      )}

      {open && (
        <LocationPanel
          location={open}
          missions={missionsOf(open)}
          missionStatus={missionStatus}
          test={test}
          onPassThrough={() => setOpen(null)}
          onPick={(id) => {
            setOpen(null);
            onPick(id);
          }}
          onClose={() => setOpen(null)}
        />
      )}
      {inventoryHero && (
        <PartyInventoryOverlay
          heroName={inventoryHero}
          classId={mapHeroClass(inventoryHero, save)}
          save={save}
          test={test}
          initialView={inventoryRequestView ?? "backpack"}
          onUseRation={onUseRation}
          onUseRationAll={onUseRationAll}
          onEquipWeapon={onEquipWeapon}
          onEquipItem={onEquipItem}
          onUsePotion={onUsePotion}
          onDiscardWeapon={onDiscardWeapon}
          onDiscardEquipment={onDiscardEquipment}
          onDiscardRation={onDiscardRation}
          onDiscardBagItem={onDiscardBagItem}
          onOpenStatus={(hero) => {
            setInventoryHero(null);
            onOpenStatus(hero);
          }}
          onClose={() => setInventoryHero(null)}
        />
      )}
    </section>
  );
}

function QuestLogPanel({ save, onClose }: { save: SaveData; onClose: () => void }) {
  // Only Inn quests are listed here (see quests.ts): accepted ones under "ativas", handed-in
  // ones under "concluídas". A quest that hasn't been accepted yet doesn't appear at all.
  const entries = QUESTS.map((quest) => ({ quest, status: questStatus(save, quest) }));
  const active = entries.filter(({ status }) => status === "active" || status === "ready");
  const completed = entries.filter(({ status }) => status === "done");
  const sections = [
    { title: "Missões ativas", list: active, empty: "Nenhuma missão ativa. Fale com o pessoal da Estalagem." },
    { title: "Missões concluídas", list: completed, empty: "Nenhuma missão concluída ainda." },
  ];

  return (
    <div
      className="absolute inset-0 z-50 flex items-center justify-center bg-bg/65 p-4 backdrop-blur-sm"
      role="presentation"
      onClick={(event) => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <section role="dialog" aria-modal="true" aria-labelledby="quest-log-title" className="relative flex max-h-[82dvh] w-full max-w-2xl flex-col overflow-hidden ember-panel">
        <header className="flex items-start justify-between gap-3 border-b border-[#525252]/60 p-4">
          <div>
            <p className="text-xs ember-kicker">Estalagem · {completed.length}/{QUESTS.length} concluídas</p>
            <h2 id="quest-log-title" className="mt-1 font-display text-2xl ember-title">Registro de missões</h2>
          </div>
          <button type="button" onClick={onClose} className="grid size-9 place-items-center ember-icon-btn" aria-label="Fechar registro de missões">
            <X className="size-4" />
          </button>
        </header>
        <div className="overflow-y-auto p-4">
          <div className="flex flex-col gap-4">
            {sections.map(({ title, list, empty }) => (
              <section key={title}>
                <h3 className="mb-2 text-sm ember-kicker">{title} · {list.length}</h3>
                <div className="flex flex-col gap-2">
                  {list.length === 0 && <p className="ember-slot p-3 text-sm text-muted">{empty}</p>}
                  {list.map(({ quest, status }) => {
                    const { have, total } = questProgress(save, quest);
                    return (
                      <article key={quest.id} className="ember-slot p-3">
                        <div className="flex items-center gap-2">
                          {status === "done" ? <Check className="size-4 shrink-0 text-accent" /> : <span aria-hidden="true" className="size-2 shrink-0 rounded-full bg-accent" />}
                          <h4 className="min-w-0 flex-1 font-display text-lg leading-tight ember-title">{quest.title}</h4>
                          <span className="shrink-0 text-xs text-muted">{status === "done" ? "Concluída" : status === "ready" ? "Entregar" : "Em andamento"}</span>
                        </div>
                        <p className="mt-1 pl-6 text-sm text-muted">
                          {quest.kind === "kill" ? `Abater ${quest.targetName}` : `Recolher ${total} itens`} · {quest.place}
                        </p>
                        {status !== "done" && (
                          <p className="mt-1 pl-6 text-xs text-muted tabular-nums">
                            {quest.kind === "kill" ? (have >= total ? "Alvo abatido — entregar na Estalagem" : "Alvo ainda vivo") : `Encontrados ${have} / ${total}`} · Recompensa {quest.reward} Gold{quest.rewardPotions.map((kind) => ` + ${POTIONS[kind].name}`).join("")}
                          </p>
                        )}
                      </article>
                    );
                  })}
                </div>
              </section>
            ))}
          </div>
        </div>
      </section>
    </div>
  );
}

const MAP_HERO_CLASS = { Kael: "swordsman", Neera: "archer", Voss: "mage", Salazar: "healer", Aldric: "aldric", Malrec: "conjurer" } as const;

function mapHeroClass(hero: string, save: SaveData) {
  return save.promotions[hero] ?? MAP_HERO_CLASS[hero as keyof typeof MAP_HERO_CLASS] ?? "swordsman";
}

/** Max HP for the world-map life bar — same formula as GameApp's heroMaxHp (class and level,
 * plus gear, less the hunger penalty). */
function mapHeroMaxHp(hero: string, save: SaveData): number {
  const stats = statsFor(mapHeroClass(hero, save), save.levels[hero] ?? 1);
  const gear = gearStatBonus(Object.values(save.equipment[hero] ?? {}));
  return Math.round((stats.hp + gear.hp) * (1 - hungerPenaltyFor(save.hungerStreak)));
}

function LocationPanel({
  location,
  missions,
  missionStatus,
  test,
  onPassThrough,
  onPick,
  onClose,
}: {
  location: WorldLocation;
  missions: Mission[];
  missionStatus: (missionId: string) => LocationStatus;
  test: boolean;
  onPassThrough: () => void;
  onPick: (id: string) => void;
  onClose: () => void;
}) {
  const [flashId, setFlashId] = useState<string | null>(null);
  return (
    <div
      className="absolute inset-0 z-40 bg-bg/45 backdrop-blur-[3px] flex items-center justify-center p-4"
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="relative w-full max-w-md max-h-[80dvh] overflow-y-auto ember-panel p-5">
        <div className="flex items-start justify-between gap-3 mb-4">
          <p className="font-display text-xl leading-tight ember-title">{location.name}</p>
          <button type="button" onClick={onClose} className="size-8 grid place-items-center ember-icon-btn" aria-label="Fechar">
            <X className="size-4" />
          </button>
        </div>
        <ol className="flex flex-col gap-2">
          {missions.map((m, i) => {
            const st = missionStatus(m.id);
            const optionalCrossing = st === "done" && isCrossingDungeon(m);
            return (
              <li key={m.id}>
                <div className="ember-slot p-3">
                  <button
                    type="button"
                    disabled={optionalCrossing}
                    onClick={() => {
                      if (st === "locked") {
                        setFlashId(m.id);
                        window.setTimeout(() => setFlashId((f) => (f === m.id ? null : f)), 500);
                        return;
                      }
                      onPick(m.id);
                    }}
                    aria-label={st === "locked" ? `${m.title} (bloqueado)` : undefined}
                    className={`w-full text-left rounded-md px-1 py-1 ${
                      st === "locked" ? `opacity-40 ${m.id === flashId ? "locked-flash" : ""}` : optionalCrossing ? "cursor-default" : ""
                    }`}
                  >
                    <p className="text-sm ember-kicker flex items-center gap-1.5">
                      {st === "locked" && <Lock className="size-3" />}
                      {st === "done" && <Check className="size-3 text-accent" />}
                      {String(i + 1).padStart(2, "0")} · {m.place}
                      {st === "done" ? " · feito" : ""}
                    </p>
                    <p className="font-display text-2xl ember-title">{m.title}</p>
                    <p className="text-base text-muted">{m.objective}</p>
                    {optionalCrossing && <p className="mt-2 text-sm text-accent">Travessia concluída · escolha como seguir</p>}
                  </button>
                  {optionalCrossing && (
                    <div className="mt-3 grid grid-cols-2 gap-2">
                      <button type="button" onClick={onPassThrough} className="min-h-10 ember-btn ember-btn-sm ember-btn-ghost px-3 text-sm">
                        Passar sem entrar
                      </button>
                      <button type="button" onClick={() => onPick(m.id)} className="min-h-10 ember-btn ember-btn-sm ember-btn-primary px-3 text-sm">
                        Explorar de novo
                      </button>
                    </div>
                  )}
                </div>
              </li>
            );
          })}
        </ol>
        {test && <p className="mt-3 text-xs text-muted">Modo teste: todos os capítulos estão abertos.</p>}
      </div>
    </div>
  );
}

function AffinityBonusHelp({ points, duo }: { points: number; duo: boolean }) {
  const [open, setOpen] = useState(false);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const show = () => { if (timer.current) clearTimeout(timer.current); timer.current = setTimeout(() => setOpen(true), 500); };
  const hide = () => { if (timer.current) clearTimeout(timer.current); timer.current = null; setOpen(false); };
  useEffect(() => () => { if (timer.current) clearTimeout(timer.current); }, []);
  return <div className="relative" onMouseEnter={show} onMouseLeave={hide}>
    <button type="button" className="text-xs text-muted text-left" aria-label="Ajuda dos bônus de afinidade" aria-expanded={open} onFocus={show} onBlur={hide} onClick={() => { if (timer.current) clearTimeout(timer.current); setOpen(value => !value); }}>
      Bônus por adjacência: +{Math.round(affinityBonus(points) * 100)}% ⓘ{duo ? " · Skill de dupla desbloqueada" : ""}
    </button>
    {open && <div role="tooltip" className="absolute top-full left-0 z-50 mt-1 w-64 max-w-[70vw] rounded-lg border border-border bg-surface p-3 shadow-xl text-xs text-fg">
      <p className="mb-2">Quando dois aliados ficam em hexes adjacentes, a afinidade aumenta ataque, magia, defesa e resistência. Vale o maior bônus entre os aliados ao lado.</p>
      <p>25 pontos: +2% · 50 pontos: +5% · 90 pontos: +8%.</p>
      <p className="mt-2">80 pontos libera a skill de dupla. A Ultimate de trio exige 100 pontos entre cada um dos três pares. As habilidades serão criadas depois.</p>
      <p className="mt-2">Ação adjacente: +0,1. Cura ou poção em um aliado: +1 adicional. Dano colateral de magia ou skill: −1. Resposta de diálogo: +3, 0 ou −3.</p>
    </div>}
  </div>;
}
