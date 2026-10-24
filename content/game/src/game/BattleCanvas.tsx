import { graphicsDpr, subscribeGraphicsQuality } from "./graphicsQuality";
import { useEffect, useRef, useSyncExternalStore } from "react";
import type { BattleEngine } from "./engine";
import { EffectsRenderer } from "./gfx/EffectsRenderer";
import { WebGL2DRenderer } from "./gfx/WebGL2DRenderer";
import { ThreeBattleRenderer } from "./gfx/three/ThreeBattleRenderer";
import { getDevGfx, subscribeDevGfx } from "./gfx/three/devGfx";
import { hasSquareMapBorder } from "./mapFloor";
import type { HudSnapshot } from "./types";

/** Three.js is now the default ground/terrain renderer (see ThreeBattleRenderer's module
 * comment) — everything else (elemental FX, units, HP bars, hover/selection highlight,
 * decorations, mouse interaction) is untouched either way. `?renderer=legacy` falls back to the
 * old Canvas2D-shim WebGL renderer for comparison while the migration is still being verified. */
function useThreeGroundRenderer(): boolean {
  if (typeof window === "undefined") return true;
  return new URLSearchParams(window.location.search).get("renderer") !== "legacy";
}

export function BattleCanvas({
  engine,
  onHud,
  onInspectUnit,
  paused = false,
  onTileReadout,
}: {
  engine: BattleEngine;
  onHud: (hud: HudSnapshot) => void;
  onInspectUnit?: (unitId: string) => void;
  paused?: boolean;
  /** Fires when the pointer has settled on one tile long enough to be asking about it, so
   * the caller can show what that terrain does. With a mouse that is the cursor resting
   * still; on touch, where there is no hover, it is a press held in place. Called with
   * false as soon as the pointer moves off, lifts, or leaves the canvas. */
  onTileReadout?: (showing: boolean) => void;
}) {
  const atmosphericFx = useSyncExternalStore(subscribeDevGfx, () => getDevGfx().atmosphericFx, () => true);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const fxCanvasRef = useRef<HTMLCanvasElement>(null);
  const spellFxCanvasRef = useRef<HTMLCanvasElement>(null);
  const tacticalUnitsCanvasRef = useRef<HTMLCanvasElement>(null);
  const unitsCanvasRef = useRef<HTMLCanvasElement>(null);
  const magicMissileCanvasRef = useRef<HTMLCanvasElement>(null);
  const unitHudCanvasRef = useRef<HTMLCanvasElement>(null);
  const wrapRef = useRef<HTMLDivElement>(null);
  const hudKey = useRef("");
  const pausedRef = useRef(paused);
  pausedRef.current = paused;

  useEffect(() => {
    const canvas = canvasRef.current;
    const wrap = wrapRef.current;
    if (!canvas || !wrap) return;
    const threeGround = useThreeGroundRenderer();
    // Exactly one of these two is ever non-null for the lifetime of this effect — see
    // useThreeGroundRenderer's comment. Both expose just enough surface (a resize call and a
    // per-frame draw call) that the rest of this component barely has to branch on which one
    // it's holding.
    let renderer2D: WebGL2DRenderer | null = null;
    let rendererThree: ThreeBattleRenderer | null = null;
    try {
      if (threeGround) {
        rendererThree = new ThreeBattleRenderer(canvas, engine);
        // Characters render as Three's own lit billboards (MeshLambertMaterial) so real scene
        // lights — map PointLights — illuminate them; the top canvas keeps their HP bars and
        // overlays only (skipUnitSprites below). Their renderOrder keeps them above Fog 2.
        rendererThree.setSpritesAndDecorationsVisible(true, true);
        if (magicMissileCanvasRef.current) rendererThree.attachMagicMissileForeground(magicMissileCanvasRef.current);
      }
      else renderer2D = new WebGL2DRenderer(canvas);
    } catch {
      return;
    }
    // Units, HP bars, particles and foreground decorations get their own transparent canvas
    // stacked ABOVE the FX canvas (see below), instead of being part of the ground canvas the
    // FX layer reads as its "scene" — otherwise a unit or decoration standing on/near a Water
    // placement would already be baked into that snapshot, and the FX canvas (a separate DOM
    // layer stacked on top of everything) would paint straight over it every frame regardless
    // of draw order. Splitting the ground and unit passes onto their own canvases (see
    // BattleEngine.renderGround/renderUnitsAndOverlays) puts a real layer boundary between them.
    const unitsCanvas = unitsCanvasRef.current;
    const tacticalUnitsCanvas = tacticalUnitsCanvasRef.current;
    const tacticalUnitsContext = tacticalUnitsCanvas?.getContext("2d") ?? null;
    const unitHudCanvas = unitHudCanvasRef.current;
    const unitHudContext = unitHudCanvas?.getContext("2d") ?? null;
    let unitsRenderer: WebGL2DRenderer | null = null;
    if (unitsCanvas) {
      try {
        unitsRenderer = new WebGL2DRenderer(unitsCanvas);
      } catch {
        unitsRenderer = null;
      }
    }
    (window as Window & { __emberEngine?: BattleEngine }).__emberEngine = engine;

    // Permanent map-authored elemental FX (lava fire, icy glints, ...) placed in the editor's
    // "FX" mode — a WebGL2 overlay that uploads the ground canvas as its "scene" texture and
    // draws the placements on top of the ground only; units/overlays are drawn afterward on
    // their own canvas above this one. Nothing to do with spell casting: it only ever plays
    // what the map author placed. Degrades to plain 2D (this canvas stays visible, overlay
    // hidden) if WebGL2 isn't available.
    let fx: EffectsRenderer | null = null;
    let spellFx: EffectsRenderer | null = null;
    // Fog of war: an effect only draws while its hex is in the party's sight. Its anchor is
    // parked far off-screen otherwise, so a placement or spell in the dark shows nothing —
    // neither over the black of unexplored ground nor as a hint of what is happening there.
    const OFFSCREEN_ANCHOR = { x: -1e6, y: -1e6, tile: 0, worldX: -1e6, worldY: -1e6 };
    // An edge copy sits one hex outside the board (see the straight-border water below); its
    // sight follows the board cell it extends.
    const fxAnchorRaw = (col: number, row: number) =>
      engine.fogged && !engine.visible(Math.max(0, Math.min(engine.cols - 1, col)), Math.max(0, Math.min(engine.rows - 1, row)))
        ? OFFSCREEN_ANCHOR
        : engine.effectAnchor(col, row);
    const fxAnchor = (col: number, row: number) => {
      const anchor = fxAnchorRaw(col, row);
      if (!rendererThree || anchor === OFFSCREEN_ANCHOR) return anchor;
      const point = rendererThree.projectFlatScreen(anchor.x, anchor.y, wrap.clientWidth, wrap.clientHeight, true);
      return { ...anchor, x: point.x, y: point.y };
    };
    const fxCanvas = fxCanvasRef.current;
    if (fxCanvas) {
      try {
        fx = new EffectsRenderer(fxCanvas);
        // Straight-border maps cut the ground on a rectangle, but hex rows are staggered: edge
        // water/shore would leave half-hex notches against the cut. Each one on the board's edge
        // also gets a copy one hex outside it; the effects mask (architectureFxMaskDataUri) clips
        // everything to the board rectangle, so the river runs flush to the straight edge.
        const squareBorder = !!rendererThree && hasSquareMapBorder(engine.tiles, engine.cols, engine.rows);
        for (const p of engine.elementalFxPlacements) if (p.family !== "procedural_pixel") {
          const water = p.kind === "water" || p.kind === "water2" || p.kind === "water3" || p.kind === "water4" || p.kind === "water5" || p.kind === "shore" || p.kind === "shore2";
          const radius = p.radiusTiles ?? (p.kind === "water2" ? 1.7 : 1);
          if (water && engine.tacticsCamera && rendererThree?.waterFxTouchesArchitecture(p.x, p.y, radius)) continue;
          fx.spawnEffect(p.kind, p.x, p.y, { radiusTiles: p.radiusTiles, rotation: p.rotation });
          if (water && squareBorder) {
            const outside: [number, number][] = [];
            if (p.x === 0) outside.push([-1, p.y]);
            if (p.x === engine.cols - 1) outside.push([engine.cols, p.y]);
            if (p.y === 0) outside.push([p.x, -1]);
            if (p.y === engine.rows - 1) outside.push([p.x, engine.rows]);
            for (const [x, y] of outside) fx.spawnEffect(p.kind, x, y, { radiusTiles: p.radiusTiles, rotation: p.rotation });
          }
        }
      } catch {
        fx = null;
        fxCanvas.style.display = "none";
      }
    }
    const spellFxCanvas = spellFxCanvasRef.current;
    if (spellFxCanvas) {
      try {
        spellFx = new EffectsRenderer(spellFxCanvas, true);
      } catch {
        spellFxCanvas.style.display = "none";
      }
    }

    // Dreaming Web's travelling shot, unlike every other elemental FX here, is spawned/
    // despawned live as the spell itself plays out rather than once at mount from an editor-
    // authored placements list — see the sync inside loop() below. The zone's own persistent
    // floor patch is a real alpha-photo image stamped per-hex in BattleEngine.renderGround now
    // (GameArt.webfloor), not a WebGL effect this canvas owns.
    const blizzardIds = new Map<number, number>();
    let webShotId: number | null = null;

    let raf = 0;
    let last = performance.now();
    let running = true;
    let dragging = false;
    let dragged = false;
    let mouseDown = false;
    let lastX = 0;
    let lastY = 0;
    // Mouse hold-and-grab-to-pan: the button must stay down this long before a drag counts
    // as panning, so a quick click near a unit/tile never gets swallowed by a small
    // incidental jitter. mouseStartX/Y anchor the "moved far enough since the press" check;
    // lastX/Y (above) are updated every move so the pan itself only ever applies one frame's
    // delta, never a jump built up while waiting to arm.
    const MOUSE_PAN_HOLD_MS = 500;
    let mouseArmed = false;
    let mouseStartX = 0;
    let mouseStartY = 0;
    let mouseHoldTimer: number | null = null;
    const restingCursor = canvas.style.cursor;
    const held = new Set<string>();
    const pointers = new Map<number, { x: number; y: number }>();
    // Press-and-hold on a tile reads out its terrain. It has to coexist with dragging the
    // camera, so the timer is armed on every press and cancelled the moment the pointer
    // travels far enough to count as a pan.
    let holdTimer: number | null = null;
    let holding = false;
    const cancelHold = () => {
      if (holdTimer !== null) {
        window.clearTimeout(holdTimer);
        holdTimer = null;
      }
      if (holding) {
        holding = false;
        onTileReadout?.(false);
      }
    };
    const armReadout = (px: number, py: number, delay: number) => {
      cancelHold();
      if (pausedRef.current) return;
      holdTimer = window.setTimeout(() => {
        holdTimer = null;
        holding = true;
        const gp = gamePos({ x: px, y: py });
        engine.pointerMove(gp.x, gp.y); // point the hover at that tile so the HUD describes it
        onTileReadout?.(true);
      }, delay);
    };
    let pinchDist = 0;
    let pinched = false;
    let lastOverlayMatrix = "";

    const resize = () => {
      const dpr = graphicsDpr();
      const w = wrap.clientWidth;
      const h = wrap.clientHeight;
      const pw = Math.max(1, Math.floor(w * dpr));
      const ph = Math.max(1, Math.floor(h * dpr));
      canvas.style.width = `${w}px`;
      canvas.style.height = `${h}px`;
      if (rendererThree) {
        // Owns canvas.width/height itself (device pixels, via its own dpr bookkeeping) —
        // see ThreeBattleRenderer.setSize.
        rendererThree.setSize(w, h, dpr);
      } else if (renderer2D) {
        canvas.width = pw;
        canvas.height = ph;
        renderer2D.setSize(pw, ph);
      }
      if (fxCanvas) {
        fx?.resize(w, h, dpr);
        fxCanvas.style.width = `${w}px`;
        fxCanvas.style.height = `${h}px`;
      }
      if (spellFxCanvas) {
        spellFx?.resize(w, h, dpr);
        spellFxCanvas.style.width = `${w}px`;
        spellFxCanvas.style.height = `${h}px`;
      }
      if (unitsCanvas && unitsRenderer) {
        unitsCanvas.width = pw;
        unitsCanvas.height = ph;
        unitsCanvas.style.width = `${w}px`;
        unitsCanvas.style.height = `${h}px`;
        unitsRenderer.setSize(pw, ph);
      }
      if (tacticalUnitsCanvas) {
        tacticalUnitsCanvas.width = pw;
        tacticalUnitsCanvas.height = ph;
        tacticalUnitsCanvas.style.width = `${w}px`;
        tacticalUnitsCanvas.style.height = `${h}px`;
      }
      if (unitHudCanvas) {
        unitHudCanvas.width = pw;
        unitHudCanvas.height = ph;
        unitHudCanvas.style.width = `${w}px`;
        unitHudCanvas.style.height = `${h}px`;
      }
    };
    resize();
    const unsubscribeQuality = subscribeGraphicsQuality(resize);
    // Resizing a canvas wipes it, and ResizeObserver runs after layout but before paint — so
    // when the board area changes size (e.g. the bottom HUD growing as a dialog closes and a
    // unit gets selected) the browser would paint the wiped, black canvas for one frame before
    // the next animation frame redraws it. Redraw right here instead (zero elapsed time, so the
    // game doesn't advance), replacing the pending frame so the loop stays a single chain.
    const ro = new ResizeObserver(() => {
      resize();
      if (!running) return;
      cancelAnimationFrame(raf);
      loop(last);
    });
    ro.observe(wrap);

    // Tells GameApp the loading curtain can come down (see its battleLoading).
    let battleReadySent = false;
    const loop = (now: number) => {
      if (!running) return;
      const dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      if (!pausedRef.current) {
        const speed = 520;
        let px = 0;
        let py = 0;
        if (held.has("ArrowLeft") || held.has("KeyA")) px -= 1;
        if (held.has("ArrowRight") || held.has("KeyD")) px += 1;
        if (held.has("ArrowUp") || held.has("KeyW")) py -= 1;
        if (held.has("ArrowDown") || held.has("KeyS")) py += 1;
        if (px || py) {
          const mag = Math.hypot(px, py) || 1;
          const screenX = (px / mag) * speed * dt;
          const screenY = (py / mag) * speed * dt;
          const azimuth = (engine.cameraTiltSide * Math.PI) / 180;
          const pitch = (Math.min(55, engine.cameraTilt) * Math.PI) / 180;
          const cosAzimuth = Math.cos(azimuth);
          const sinAzimuth = Math.sin(azimuth);
          const verticalScale = 1 / Math.max(0.1, Math.cos(pitch));
          engine.panBy(
            screenX * cosAzimuth + screenY * sinAzimuth * verticalScale,
            -screenX * sinAzimuth + screenY * cosAzimuth * verticalScale,
          );
        }
        engine.tick(dt);
      }
      const dpr = graphicsDpr();
      // Elemental FX is a DOM canvas between the Three scene and the normal unit overlay.
      // Keep actor sprites above it in either camera mode. Tactical billboards use their own
      // upright projected overlay; ordinary decorations move up only on sprite-only maps, while
      // projected architecture masks keep 3D props visible. Include pending requests so the
      // first frame of a spell is layered too.
      const fxPending = !!fx && (fx.hasEffects() || engine.elementalFxRequests.length > 0 || !!engine.webShotBeam());
      const drawUnitsOverFx = !!rendererThree && !engine.tacticsCamera && fxPending;
      const drawTacticalUnitsOverFx = !!rendererThree && engine.tacticsCamera && fxPending;
      const drawSpritesOverFx = drawUnitsOverFx || drawTacticalUnitsOverFx;
      const drawDecorationsOverFx = !!rendererThree && drawUnitsOverFx && !rendererThree.hasArchitecture();
      if (rendererThree) {
        // Move actor sprites above the FX canvas. Decorations also move there on sprite-only
        // maps, in the painter order rear decor → characters → foreground decor.
        rendererThree.setSpritesAndDecorationsVisible(!drawSpritesOverFx, !drawDecorationsOverFx);
        // Movement/attack/spell-range highlight and the active-turn ring are drawn as part of
        // this call now (see ThreeBattleRenderer.syncOverlay) — real world-space hex meshes
        // ordered between terrain and decorations, not a separate 2D overlay, so a blocking
        // decoration or a unit standing on a highlighted hex stays visible on top of it instead
        // of the highlight's tint painting over it.
        rendererThree.render(wrap.clientWidth, wrap.clientHeight, pausedRef.current);
        if (!battleReadySent && rendererThree.isWarm()) {
          battleReadySent = true;
          window.dispatchEvent(new CustomEvent("ember:battle-ready"));
        }
        if (unitsCanvas) {
          const matrix = rendererThree.overlayTransform(wrap.clientWidth, wrap.clientHeight);
          const cssTransform = `matrix(${matrix.join(",")})`;
          if (cssTransform !== lastOverlayMatrix) {
            lastOverlayMatrix = cssTransform;
            unitsCanvas.style.transformOrigin = "0 0";
            unitsCanvas.style.transform = cssTransform;
          }
        }
      } else if (renderer2D) {
        renderer2D.clear();
        engine.renderGround(renderer2D, wrap.clientWidth, wrap.clientHeight, dpr);
      }
      // Without the WebGL renderer there is nothing to warm: ready after the first drawn frame.
      if (!battleReadySent && !rendererThree) {
        battleReadySent = true;
        window.dispatchEvent(new CustomEvent("ember:battle-ready"));
      }
      const spellEffects = spellFx ?? fx;
      if (spellEffects) {
        for(const request of engine.frostVfxRequests.splice(0))spellEffects.spawnFrost({...request,onImpact:cell=>rendererThree?.pulseBlizzardLight(cell.x,cell.y,0,0,.55)});
        {
          const live=new Set(engine.iceStormZones.map(zone=>zone.createdAt));
          for(const[key,id]of blizzardIds)if(!live.has(key)){spellEffects.removeBlizzard(id);blizzardIds.delete(key);}
          for(const zone of engine.iceStormZones)if(!blizzardIds.has(zone.createdAt)){
            const cells=[...zone.cells].map(key=>{const[x,y]=key.split(",").map(Number);return{x,y};}).filter(cell=>!engine.fogged||engine.visible(cell.x,cell.y));
            if(!cells.length)continue;
            const id=spellEffects.spawnBlizzard({cells,duration:9,seed:Math.floor(zone.createdAt*1000),particleDensity:1.3,groundScaleY:Math.cos(engine.cameraTilt*Math.PI/180),onImpact:(cell,dx,dy,strength)=>rendererThree?.pulseBlizzardLight(cell.x,cell.y,dx,dy,strength)});
            blizzardIds.set(zone.createdAt,id);
          }
        }
        // Dreaming Web's shot: one "webShot" beam, repositioned every frame via updateOverride
        // to follow the travelling missile's own timing (see BattleEngine.webShotBeam) — it
        // can't use the fixed getAnchor(col,row) model every other effect here relies on.
        const beam = engine.webShotBeam();
        if (beam) {
          if (webShotId === null) webShotId = spellEffects.spawnEffect("webShot", 0, 0, { radiusTiles: 0.01 });
          const screen = rendererThree?.projectFlatScreen(beam.x, beam.y, wrap.clientWidth, wrap.clientHeight, true);
          spellEffects.updateOverride(webShotId, {
            x: screen?.x ?? beam.x,
            y: screen?.y ?? beam.y,
            worldX: beam.worldX,
            worldY: beam.worldY,
            tile: beam.tile,
            halfLengthPx: Math.max(1, beam.length / 2),
            // Wide enough that the tangled multi-strand shader (see shaders.ts's WEB_SHOT,
            // whose strands spread out to about |v_local.y| = 0.85 near the tail) actually
            // reads as a comet-like cluster instead of a thin line.
            halfWidthPx: beam.tile * 0.4,
            rotation: beam.angle,
          });
        } else if (webShotId !== null) {
          spellEffects.removeEffect(webShotId);
          webShotId = null;
        }
        if (engine.elementalFxRequests.length) {
          for (const req of engine.elementalFxRequests.splice(0)) {
            spellEffects.spawnEffect(req.kind, req.x, req.y, { duration: req.duration });
          }
        }
      }
      if (fx) {
        // Skip the rest of the FX pipeline (scene upload, light/effects/bloom FBO passes)
        // whenever nothing — editor-placed or live spell FX — is actually active, so an
        // ordinary fight never pays for it.
        if (fx.hasEffects()) {
          if (fxCanvas) {
            const architectureMask = rendererThree?.architectureFxMaskDataUri(wrap.clientWidth, wrap.clientHeight) ?? "none";
            fxCanvas.style.setProperty("mask-image", architectureMask);
            fxCanvas.style.setProperty("-webkit-mask-image", architectureMask);
            fxCanvas.style.setProperty("mask-size", "100% 100%");
            fxCanvas.style.setProperty("-webkit-mask-size", "100% 100%");
            fxCanvas.style.setProperty("mask-repeat", "no-repeat");
            fxCanvas.style.setProperty("-webkit-mask-repeat", "no-repeat");
            fxCanvas.style.display = "block";
          }
          fx.render(canvas, pausedRef.current ? 0 : dt, fxAnchor);
        } else if (fxCanvas) {
          fxCanvas.style.display = "none";
          fxCanvas.style.setProperty("mask-image", "none");
          fxCanvas.style.setProperty("-webkit-mask-image", "none");
        }
      }
      if (tacticalUnitsCanvas && tacticalUnitsContext) {
        tacticalUnitsContext.setTransform(dpr, 0, 0, dpr, 0, 0);
        tacticalUnitsContext.clearRect(0, 0, wrap.clientWidth, wrap.clientHeight);
        if (drawTacticalUnitsOverFx) rendererThree?.renderTacticalUnitSprites(tacticalUnitsContext, wrap.clientWidth, wrap.clientHeight);
      }
      // Drawn on its own transparent canvas above the FX layer, so units/HP-bars/foreground
      // decorations always read in front of a Water/Fire/etc placement instead of being
      // whatever the FX's snapshot happened to catch underneath it.
      if (unitsRenderer && unitsCanvas) {
        unitsRenderer.setTransform(dpr, 0, 0, dpr, 0, 0);
        unitsRenderer.clear();
        engine.renderUnitsAndOverlays(
          unitsRenderer,
          wrap.clientWidth,
          wrap.clientHeight,
          fx ? (px: number, py: number) => fx.lightBoostAt(px, py, fxAnchorRaw) : undefined,
          // Three normally owns its decorations and sprites. During active elemental FX, move
          // sprites above the effect on every non-tactical map; redraw decorations there only
          // when there is no architecture whose depth order needs to stay in the Three scene.
          !!rendererThree && !drawDecorationsOverFx,
          !!rendererThree && !drawUnitsOverFx,
          !!rendererThree && !drawSpritesOverFx,
          !!rendererThree,
          !!rendererThree && !drawDecorationsOverFx,
          !!rendererThree,
          !!rendererThree,
          !!rendererThree && engine.tacticsCamera,
        );
      }
      if (spellFx) {
        if (spellFx.hasEffects()) {
          if (spellFxCanvas) spellFxCanvas.style.display = "block";
          spellFx.render(canvas, pausedRef.current ? 0 : dt, fxAnchor);
        } else if (spellFxCanvas) spellFxCanvas.style.display = "none";
      }
      // Spell foreground is a distinct canvas above both water and character surfaces.
      rendererThree?.renderMagicMissileForeground(wrap.clientWidth, wrap.clientHeight);
      if (unitHudCanvas && unitHudContext) {
        unitHudContext.setTransform(dpr, 0, 0, dpr, 0, 0);
        unitHudContext.clearRect(0, 0, wrap.clientWidth, wrap.clientHeight);
        rendererThree?.renderUnitHealthHud(unitHudContext, wrap.clientWidth, wrap.clientHeight);
        if (engine.tacticsCamera) rendererThree?.renderFloatingText(unitHudContext, wrap.clientWidth, wrap.clientHeight);
      }
      const hud = engine.getHud();
      const k = [
        hud.mode,
        hud.phase,
        hud.selected?.id,
        hud.canAttack,
        hud.banner,
        hud.result,
        hud.turn,
        hud.playerAlive,
        hud.enemyAlive,
        hud.forecast?.defender,
        hud.forecast?.dmgOut,
        hud.inspected?.id,
        hud.pendingFoe?.id,
        hud.selected?.hp,
        hud.selected?.fullness,
        hud.inspected?.fullness,
        hud.inspected?.hp,
        hud.selected?.bag.mid,
        hud.selected?.bag.weak,
        hud.selected?.bag.potent,
        hud.selected?.bag.disease,
        hud.tip,
        // Terrain inspection changes while the cursor rests over the board. It must be part
        // of the HUD identity; otherwise React keeps the first hovered hex (usually plains)
        // even though the engine has already resolved the actual tile underneath the cursor.
        hud.terrain
          ? `${hud.terrain.id}:${hud.terrain.name}:${hud.terrain.moveCost}:${hud.terrain.def}:${hud.terrain.atk}:${hud.terrain.passable ? 1 : 0}:${hud.terrain.blocksShot ? 1 : 0}:${hud.terrain.hazard ?? ""}:${hud.terrain.note ?? ""}:${hud.terrain.spellZone ? `${hud.terrain.spellZone.kind}:${hud.terrain.spellZone.roundsLeft}` : ""}`
          : "no-terrain",
        hud.zoom,
        hud.speedMode,
        hud.winAvailable,
        hud.spellReady,
        hud.spellArmed,
        hud.spellHitChance,
        hud.turnQueue.find((q) => q.active)?.id,
        hud.turnQueue.map((q) => (q.acted ? "1" : "0")).join(""),
        // Which enemies are listed changes as fog reveals/hides them.
        hud.turnQueue.map((q) => q.id).join(","),
        hud.chestLoot ? `${hud.chestLoot.unitName}:${hud.chestLoot.ember}:${hud.chestLoot.items.map((i) => i.name).join(",")}` : null,
        hud.pendingDialog ? `${hud.pendingDialog.id}` : null,
      ].join("|");
      if (k !== hudKey.current) {
        hudKey.current = k;
        onHud(hud);
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);

    const pointerPos = (clientX: number, clientY: number) => {
      const r = canvas.getBoundingClientRect();
      return { x: clientX - r.left, y: clientY - r.top };
    };
    const pos = (e: MouseEvent) => pointerPos(e.clientX, e.clientY);
    const gamePos = (p: { x: number; y: number }) => rendererThree
      ? rendererThree.screenToFlatScreen(p.x, p.y, canvas.clientWidth, canvas.clientHeight)
      : p;
    const panByScreen = (from: { x: number; y: number }, to: { x: number; y: number }) => {
      const oldPoint = rendererThree ? rendererThree.screenToFlatScreen(from.x, from.y, canvas.clientWidth, canvas.clientHeight, false) : from;
      const newPoint = rendererThree ? rendererThree.screenToFlatScreen(to.x, to.y, canvas.clientWidth, canvas.clientHeight, false) : to;
      engine.panBy(oldPoint.x - newPoint.x, oldPoint.y - newPoint.y);
    };
    const onDown = (e: PointerEvent) => {
      if (pausedRef.current) return;
      if (e.pointerType === "mouse" && e.button !== 0) return;
      const p = pos(e);
      if (e.pointerType === "mouse") {
        if (engine.getHud().mode === "awaitSpell") {
          const gp = gamePos(p);
          engine.pointerDown(gp.x, gp.y, "click");
          return;
        }
        // Deferred to pointerup, same as touch: a held-and-dragged mouse pans the
        // camera (see onMove/onUp) instead of immediately acting on the down-press.
        mouseDown = true;
        dragging = true;
        dragged = false;
        mouseArmed = false;
        mouseStartX = e.clientX;
        mouseStartY = e.clientY;
        lastX = e.clientX;
        lastY = e.clientY;
        canvas.setPointerCapture(e.pointerId);
        if (mouseHoldTimer !== null) window.clearTimeout(mouseHoldTimer);
        mouseHoldTimer = window.setTimeout(() => {
          mouseHoldTimer = null;
          mouseArmed = true;
          canvas.style.cursor = "url('/game/cursors/medieval-gauntlet-grab-small.svg') 13 13, grabbing";
        }, MOUSE_PAN_HOLD_MS);
        return;
      }
      pointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
      canvas.setPointerCapture(e.pointerId);
      if (pointers.size >= 2) {
        const pts = [...pointers.values()];
        pinchDist = Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
        pinched = true;
        dragging = false;
        dragged = true;
        return;
      }
      dragging = true;
      dragged = false;
      lastX = e.clientX;
      lastY = e.clientY;
      armReadout(p.x, p.y, 800);
      if (engine.getHud().mode === "awaitSpell") {
        const gp = gamePos(p);
        engine.pointerMove(gp.x, gp.y);
      }
    };
    const onMove = (e: PointerEvent) => {
      if (pointers.has(e.pointerId)) pointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
      if (pointers.size >= 2 && pinchDist > 0) {
        const pts = [...pointers.values()];
        const d = Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
        if (d > pinchDist * 1.22) {
          engine.cycleZoom(1);
          pinchDist = d;
        } else if (d < pinchDist * 0.82) {
          engine.cycleZoom(-1);
          pinchDist = d;
        }
        return;
      }
      const p = pos(e);
      const spell = engine.getHud().mode === "awaitSpell";
      // Resting the cursor on a tile asks about it. Every movement restarts the clock, so
      // this only fires once the pointer actually stops — no button involved.
      if (e.pointerType === "mouse" && !mouseDown) armReadout(p.x, p.y, 1200);
      if (e.pointerType === "mouse" && mouseDown) {
        if (!dragged && mouseArmed && Math.hypot(e.clientX - mouseStartX, e.clientY - mouseStartY) > 3) {
          dragged = true;
          cancelHold();
        }
        if (dragged) panByScreen(pointerPos(lastX, lastY), pointerPos(e.clientX, e.clientY));
        lastX = e.clientX;
        lastY = e.clientY;
        return;
      }
      if (dragging && e.pointerType !== "mouse" && !spell) {
        const dx = e.clientX - lastX;
        const dy = e.clientY - lastY;
        if (Math.abs(dx) + Math.abs(dy) > 3) {
          dragged = true;
          cancelHold();
        }
        if (dragged) {
          panByScreen(pointerPos(lastX, lastY), pointerPos(e.clientX, e.clientY));
          lastX = e.clientX;
          lastY = e.clientY;
        }
      } else {
        if (dragging && e.pointerType !== "mouse" && spell) {
          const dx = e.clientX - lastX;
          const dy = e.clientY - lastY;
          if (Math.abs(dx) + Math.abs(dy) > 10) dragged = true;
        }
        const gp = gamePos(p);
        engine.pointerMove(gp.x, gp.y);
      }
    };
    const onUp = (e: PointerEvent) => {
      const wasHolding = holding;
      cancelHold();
      if (e.pointerType === "mouse") {
        canvas.style.cursor = restingCursor;
        if (mouseHoldTimer !== null) {
          window.clearTimeout(mouseHoldTimer);
          mouseHoldTimer = null;
        }
        mouseArmed = false;
        if (!mouseDown) return;
        mouseDown = false;
        dragging = false;
        if (!dragged && !pausedRef.current && !wasHolding) {
          const p = pos(e);
          const gp = gamePos(p);
          engine.pointerDown(gp.x, gp.y, "click");
        }
        dragged = false;
        return;
      }
      pointers.delete(e.pointerId);
      if (pointers.size < 2) pinchDist = 0;
      if (pinched) {
        if (pointers.size === 0) pinched = false;
        dragging = false;
        return;
      }
      if (!dragging) return;
      dragging = false;
      if (!dragged && !pausedRef.current && !wasHolding) {
        const p = pos(e);
        const gp = gamePos(p);
        engine.pointerDown(gp.x, gp.y, "tap");
      }
    };
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      engine.cycleZoom(e.deltaY > 0 ? -1 : 1);
    };
    const onKey = (e: KeyboardEvent) => {
      if (pausedRef.current) return;
      const trap = [
        "ArrowLeft",
        "ArrowRight",
        "ArrowUp",
        "ArrowDown",
        "Space",
        "Enter",
        "Escape",
        "KeyE",
        "KeyZ",
        "KeyW",
        "KeyA",
        "KeyS",
        "KeyD",
      ];
      if (trap.includes(e.code)) e.preventDefault();
      held.add(e.code);
      if (
        e.code === "ArrowLeft" ||
        e.code === "ArrowRight" ||
        e.code === "ArrowUp" ||
        e.code === "ArrowDown" ||
        e.code === "KeyW" ||
        e.code === "KeyA" ||
        e.code === "KeyS" ||
        e.code === "KeyD"
      ) {
        return;
      }
      engine.keyDown(e.code);
    };
    const onKeyUp = (e: KeyboardEvent) => {
      held.delete(e.code);
    };

    const onMenu = (e: MouseEvent) => {
      e.preventDefault();
      if (pausedRef.current) return;
      const p = pos(e);
      const inspectedUnitId = engine.inspectAt(p.x, p.y);
      if (inspectedUnitId) {
        onInspectUnit?.(inspectedUnitId);
        return;
      }
      const hud = engine.getHud();
      const showAct =
        hud.mode === "awaitAction" || hud.mode === "awaitAttack" || hud.mode === "selected" || hud.mode === "awaitSpell" || hud.mode === "awaitOffHand";
      if (!showAct || hud.busy) return;
      // Free exploration: right-click never undoes movement — deselect in place instead.
      if (engine.mission.explore && (hud.mode === "selected" || hud.mode === "awaitAction")) {
        engine.deselect(true);
        return;
      }
      engine.cancel();
    };
    canvas.addEventListener("pointerdown", onDown);
    canvas.addEventListener("pointermove", onMove);
    canvas.addEventListener("pointerup", onUp);
    canvas.addEventListener("pointercancel", onUp);
    canvas.addEventListener("pointerleave", cancelHold);
    canvas.addEventListener("wheel", onWheel, { passive: false });
    canvas.addEventListener("contextmenu", onMenu);
    window.addEventListener("keydown", onKey);
    window.addEventListener("keyup", onKeyUp);

    return () => {
      running = false;
      canvas.style.cursor = restingCursor;
      cancelAnimationFrame(raf);
      unsubscribeQuality();
      ro.disconnect();
      cancelHold();
      if (mouseHoldTimer !== null) window.clearTimeout(mouseHoldTimer);
      canvas.removeEventListener("pointerdown", onDown);
      canvas.removeEventListener("pointermove", onMove);
      canvas.removeEventListener("pointerup", onUp);
      canvas.removeEventListener("pointerleave", cancelHold);
      canvas.removeEventListener("pointercancel", onUp);
      canvas.removeEventListener("wheel", onWheel);
      canvas.removeEventListener("contextmenu", onMenu);
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("keyup", onKeyUp);
      const w = window as Window & { __emberEngine?: BattleEngine };
      if (w.__emberEngine === engine) delete w.__emberEngine;
      fx?.dispose();
      spellFx?.dispose();
      rendererThree?.dispose();
    };
  // Keep the renderer and its warmed GPU resources mounted when a briefing/dialog pauses
  // the board. Rebuilding this effect on pause changes caused a visible renderer reset as
  // soon as the dialog closed, after the loading curtain had already gone away.
  }, [engine, onHud, onInspectUnit, ThreeBattleRenderer, WebGL2DRenderer]);

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
    <div ref={wrapRef} className="relative h-full w-full min-h-0 touch-none">
      <canvas ref={canvasRef} className="block h-full w-full touch-none" />
      <canvas ref={fxCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" style={{ display: "none" }} />
      {/* Tactical unit sprites (drawn here only while elemental FX are on screen) sit BELOW the units/overlay
          canvas, so missiles and other overlays drawn there stay in front of the caster, never behind. */}
      <canvas ref={tacticalUnitsCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" />
      <canvas ref={unitsCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" />
      <canvas ref={spellFxCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" style={{ display: "none" }} />
      <canvas ref={magicMissileCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" style={{ mixBlendMode: "screen" }} />
      <canvas ref={unitHudCanvasRef} className="pointer-events-none absolute inset-0 block h-full w-full touch-none" />
      {/* Diorama color grade + vignette: a subtle warm key-light / cool shadow wash from the
          same upper-left "sun" the unit/decoration relighting and cast shadows use (see
          WebGL2DRenderer's lightDirX/Y and BattleEngine's shadowDirX/Y), plus a soft edge
          vignette. Pure CSS, above every game canvas, non-interactive, and gentle enough
          (soft-light / multiply, low alpha) to never wash out the art or UI underneath. */}
      <div
        className="pointer-events-none absolute inset-0"
        style={{
          background:
            "linear-gradient(135deg, rgba(255,208,150,0.16) 0%, rgba(255,208,150,0) 32%, rgba(48,58,92,0) 55%, rgba(40,52,88,0.22) 100%)",
          mixBlendMode: "soft-light",
        }}
      />
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
    </div>
  );
}
