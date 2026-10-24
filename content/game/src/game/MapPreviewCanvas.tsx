import { graphicsDpr } from "./graphicsQuality";
import { type PointerEvent, useEffect, useRef, useState } from "react";
import { DECORATIONS, placedFootprint, TERRAIN, TILE_CHAR } from "./data";
import { tileVariantName } from "./assets";
import { BattleEngine, ZOOM_RADII } from "./engine";
import { EffectsRenderer } from "./gfx/EffectsRenderer";
import { ThreeBattleRenderer } from "./gfx/three/ThreeBattleRenderer";
import { WebGL2DRenderer } from "./gfx/WebGL2DRenderer";
import type { GameArt, Mission, TerrainId } from "./types";

export type PreviewUnitSelection = {
  side: "playerSpawns" | "enemySpawns" | "neutralSpawns";
  index: number;
  name: string;
};

export type PreviewDecorationSelection = { id: string; x: number; y: number; rot?: number };

// The technical map is a native scroll surface; keep preview scrollbar travel deliberately gentler.
const PREVIEW_SCROLL_PAN_RATE = 0.45;
const PREVIEW_ZOOM_MIN = 0.75;
// CODER-ONLY: DO NOT MESS WITH CONTROLS. Preserve left-button hold for 0.5 seconds,
// then show the grabbing hand and pan on drag. Never display this warning in the UI.
const PREVIEW_PAN_HOLD_MS = 500;
const TERRAIN_BY_CHAR = Object.fromEntries(Object.entries(TILE_CHAR).map(([id, ch]) => [ch, id])) as Record<string, TerrainId>;

/** A read-only window onto the map exactly as the real battle would render it — same tile
 * art, same decoration art, same unit sprites — instead of the paint grid's flat color
 * swatches. Builds a throwaway BattleEngine from the current draft and only ever calls its
 * render(), never tick(): no animation loop, no AI, no turns — just a live snapshot that
 * redraws whenever the mission prop changes (the caller debounces that) or the panel resizes.
 * A left click can use the current editor brush directly; gameplay state remains untouched. */
export function MapPreviewCanvas({
  mission,
  art,
  onCellClick,
  selectedDecorationId,
  selectedPlacedDecoration,
  onUnitSelect,
  onHeldUnitDelete,
  onUnitPlace,
  onDecorationSelect,
  onDecorationPlace,
  primaryObjectDrag = true,
  tacticsView = false,
  onTacticsViewChange,
}: {
  mission: Mission;
  art: GameArt;
  onCellClick?: (x: number, y: number, point?: { x: number; y: number }) => void;
  selectedDecorationId?: string;
  selectedPlacedDecoration?: PreviewDecorationSelection | null;
  onUnitSelect?: (unit: PreviewUnitSelection) => void;
  /** Delete is deliberate: it only applies while the author is holding a placed unit. */
  onHeldUnitDelete?: (unit: PreviewUnitSelection) => void;
  onUnitPlace?: (unit: PreviewUnitSelection, x: number, y: number) => void;
  /** Right-click-drag pickup, mirroring onUnitSelect for units: fires as soon as an existing
   * placement is grabbed, before it's known where it'll be dropped. */
  onDecorationSelect?: (decoration: PreviewDecorationSelection) => void;
  onDecorationPlace?: (decoration: PreviewDecorationSelection, x: number, y: number) => void;
  primaryObjectDrag?: boolean;
  tacticsView?: boolean;
  onTacticsViewChange?: (enabled: boolean) => void;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const groundRendererRef = useRef<WebGL2DRenderer | null>(null);
  const unitsRendererRef = useRef<WebGL2DRenderer | null>(null);
  const architectureRendererRef = useRef<ThreeBattleRenderer | null>(null);
  const rendererFxKeyRef = useRef("");
  const highlightRef = useRef({ selectedDecorationId, selectedPlacedDecoration });
  highlightRef.current = { selectedDecorationId, selectedPlacedDecoration };
  const onCellClickRef = useRef(onCellClick);
  onCellClickRef.current = onCellClick;
  const fxCanvasRef = useRef<HTMLCanvasElement>(null);
  const pixelFxCanvasRef = useRef<HTMLCanvasElement>(null);
  const unitsCanvasRef = useRef<HTMLCanvasElement>(null);
  const viewportRef = useRef<HTMLDivElement>(null);
  const scrollContentRef = useRef<HTMLDivElement>(null);
  const engineRef = useRef<BattleEngine | null>(null);
  const redrawRef = useRef<(() => void) | null>(null);
  const armTimerRef = useRef<number | null>(null);
  const dragRef = useRef<{ pointerId: number; x: number; y: number; startX: number; startY: number; armed: boolean; moved: boolean } | null>(null);
  // moved stays false for a plain right-click (press and release without dragging) — that's
  // a pick-up-and-hold, not a move, so releasing the button must not drop the unit back onto
  // the board. Only a real drag past the threshold (see onPointerMove) arms a drop on release.
  const unitDragRef = useRef<{ pointerId: number; unit: PreviewUnitSelection; startX: number; startY: number; moved: boolean } | null>(null);
  const decorationDragRef = useRef<{ pointerId: number; decoration: PreviewDecorationSelection; startX: number; startY: number; moved: boolean } | null>(null);
  const cameraRef = useRef<{ x: number; y: number; missionId: string; tile: number; viewW: number; viewH: number } | null>(null);
  /** CSS pixels divided by this value become preview-engine logical pixels. This preserves
   * the requested zoom while keeping the engine on one of its supported tile sizes. */
  const renderScaleRef = useRef(1);
  const verticalScrollTopRef = useRef(0);
  const horizontalScrollLeftRef = useRef(0);
  const verticalScrollInitializedRef = useRef(false);
  // Start at a true 75% scale. Large maps extend beyond the viewport and can be panned.
  const [zoom, setZoom] = useState(PREVIEW_ZOOM_MIN);
  const [viewYaw, setViewYaw] = useState(30);
  const [viewTilt, setViewTilt] = useState(45);
  const [isPanning, setIsPanning] = useState(false);
  const [isDragging, setIsDragging] = useState(false);
  // Hover readout: the painted tile under the cursor (terrain name + art file). Display only.
  const [hoverTile, setHoverTile] = useState<{ key: string; label: string; left: number; top: number } | null>(null);
  // Map the percentage control to an actual rendered tile size relative to the game's 34px
  // default. The engine renders at its nearest supported radius; the canvas scale supplies
  // the exact percentage between those discrete sizes.
  const targetTileRadius = 34 * zoom;
  const previewZoomLevel = ZOOM_RADII.reduce((best, radius, index) =>
    Math.abs(radius - targetTileRadius) < Math.abs(ZOOM_RADII[best]! - targetTileRadius) ? index : best, 0);
  const previewTileRadius = ZOOM_RADII[previewZoomLevel]!;
  const architectureActive = !!DECORATIONS[selectedDecorationId ?? ""]?.model3d;
  const previewRenderScale = targetTileRadius / previewTileRadius;
  const previewBoardWidth = Math.ceil(previewTileRadius * Math.sqrt(3) * (mission.cols + 0.5) * previewRenderScale);
  // Keep this in step with BattleEngine.boardSize, including vertical breathing room.
  const previewBoardHeight = Math.ceil(previewTileRadius * (1.5 * (mission.rows - 1) + 4.4) * previewRenderScale);
  const unitAt = (x: number, y: number): PreviewUnitSelection | null => {
    const groups = [
      { side: "playerSpawns" as const, units: mission.playerSpawns },
      { side: "enemySpawns" as const, units: mission.enemySpawns },
      { side: "neutralSpawns" as const, units: mission.neutralSpawns ?? [] },
    ];
    for (const group of groups) {
      const index = group.units.findIndex((unit) => unit.x === x && unit.y === y);
      if (index >= 0) return { side: group.side, index, name: group.units[index]!.name };
    }
    return null;
  };
  const decorationAt = (x: number, y: number): PreviewDecorationSelection | null => {
    const hits = (mission.decorations ?? []).filter((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));
    const hit = hits.find(p => !!DECORATIONS[p.id]?.exitKind) ?? hits[0];
    return hit ? { id: hit.id, x: hit.x, y: hit.y, rot: hit.rot } : null;
  };

  useEffect(() => {
    const canvas = canvasRef.current;
    const viewport = viewportRef.current;
    if (!canvas || !viewport) return;
    // Same WebGL2D wrapper BattleCanvas renders through (see WebGL2DRenderer) rather than a
    // native CanvasRenderingContext2D — the engine's render methods lean on wrapper-only
    // extensions like drawImageLit for sprite relighting that a plain 2d context doesn't have.
    let ctx: WebGL2DRenderer;
    try {
      ctx = groundRendererRef.current ?? (groundRendererRef.current = new WebGL2DRenderer(canvas));
    } catch {
      return;
    }
    // See BattleCanvas for why: units/foreground decorations get their own transparent
    // canvas above the FX layer, so a Water/Fire/etc placement can't paint over them
    // regardless of draw order.
    const unitsCanvas = unitsCanvasRef.current;
    let unitsCtx: WebGL2DRenderer | null = null;
    if (unitsCanvas) {
      try {
        unitsCtx = unitsRendererRef.current ?? (unitsRendererRef.current = new WebGL2DRenderer(unitsCanvas));
      } catch {
        unitsCtx = null;
      }
    }

    let engine: BattleEngine;
    try {
      // The editor is an authoring surface, not a play session: even when a draft opts into
      // fog for the actual battle, its preview must expose every tile, prop, and spawn.
      // Keep this override local to the throwaway preview engine so playtests and gameplay
      // still honor the mission's saved fog setting.
      engine = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels: {} }, 1);
      // Keep the canvas the size of the window. The BattleEngine owns the real
      // camera, so dragging moves the board rather than an oversized empty canvas.
      engine.setZoom(previewZoomLevel);
      engine.tacticsCamera = tacticsView;
      engine.cameraTilt = tacticsView ? viewTilt : 0;
      engine.cameraTiltSide = tacticsView ? viewYaw : 0;
      // Keep a small edge rim so the preview can pan without opening onto a half-window of
      // empty space when the party's starting hex is near the board boundary.
      engine.setPreviewPanMargin(3);
      engineRef.current = engine;
    } catch {
      return;
    }
    let needsCameraRestore = cameraRef.current?.missionId === mission.id;
    let needsInitialCenter = !needsCameraRestore;

    // Live preview of any elemental FX placed on this map (see the editor's "FX" mode) —
    // same pipeline BattleCanvas uses, spawned once here as persistent instances so the
    // author can see exactly what will play once the mission loads for real.
    let fx: EffectsRenderer | null = null;
    const fxCanvas = fxCanvasRef.current;
    if (fxCanvas) {
      try {
        fx = new EffectsRenderer(fxCanvas);
        for (const p of engine.elementalFxPlacements) if (p.family !== "procedural_pixel") {
          const water = p.kind === "water" || p.kind === "water2" || p.kind === "water3" || p.kind === "water4" || p.kind === "water5" || p.kind === "shore" || p.kind === "shore2";
          const radius = p.radiusTiles ?? (p.kind === "water2" ? 1.7 : 1);
          const source = engine.effectAnchor(p.x, p.y);
          const coveredByBuilding = water && tacticsView && mission.decorations?.some(placement =>
            !!DECORATIONS[placement.id]?.model3d && placedFootprint(placement).some(cell => {
              const building = engine.effectAnchor(placement.x + cell.dx, placement.y + cell.dy);
              return Math.hypot(source.worldX - building.worldX, source.worldY - building.worldY) < source.tile * (radius + 0.45);
            }));
          if (coveredByBuilding) continue;
          fx.spawnEffect(p.kind, p.x, p.y, { radiusTiles: p.radiusTiles, rotation: p.rotation });
        }
      } catch {
        fx = null;
      }
    }
    const pixelFxCanvas = pixelFxCanvasRef.current;
    // The procedural family needs the same lit terrain, props and point-light pool as battle.
    // Render that battle scene for procedural placements and architectural geometry.
    let pixelRenderer: ThreeBattleRenderer | null = null;
    const pixelPlacements = engine.elementalFxPlacements.filter((placement) => placement.family === "procedural_pixel" && placement.element);
    const hasArchitecture = architectureActive || engine.decorations.some(p => !!DECORATIONS[p.id]?.model3d);
    if (pixelFxCanvas && (tacticsView || pixelPlacements.length || hasArchitecture || !!mission.waterPatches?.length || mission.waterLevels?.some(level => level != null) || mission.terrainElevations?.some(level => level > 0) || engine.tiles.some(id => id !== "void" && (TERRAIN[id]?.height ?? 0) > 0))) {
      try {
        const fxKey = JSON.stringify(pixelPlacements);
        if (architectureRendererRef.current && rendererFxKeyRef.current !== fxKey) {
          architectureRendererRef.current.dispose();
          architectureRendererRef.current = null;
        }
        rendererFxKeyRef.current = fxKey;
        pixelRenderer = architectureRendererRef.current ?? new ThreeBattleRenderer(pixelFxCanvas, engine);
        pixelRenderer.setPreviewEngine(engine);
        architectureRendererRef.current = pixelRenderer;
        engine.architectureRenderedInThree = true;
      } catch (error) {
        console.error("Procedural Pixel preview could not start", error);
      }
    }
    let lastFrame = performance.now();

    const draw = () => {
      const dpr = graphicsDpr();
      const w = Math.max(1, Math.floor(viewport.clientWidth));
      const h = Math.max(1, Math.floor(viewport.clientHeight));
      if (w <= 0 || h <= 0) return;
      // The camera can travel from one three-hex margin to the other, with the map itself
      // between them. Convert that full camera range into native-scroll distance while
      // preserving the preview's deliberately gentler scroll feel. Including the viewport
      // size matters: without it the thumb reaches its end before the camera reaches the
      // far edge on large maps, and small maps cannot expose their allowed edge margin.
      const edgeMargin = 3 * targetTileRadius;
      if (scrollContentRef.current) {
        const horizontalCameraRange = Math.max(0, previewBoardWidth - w) + edgeMargin * 2;
        const verticalCameraRange = Math.max(0, previewBoardHeight - h) + edgeMargin * 2;
        scrollContentRef.current.style.width = `${Math.ceil(w + horizontalCameraRange / PREVIEW_SCROLL_PAN_RATE)}px`;
        scrollContentRef.current.style.height = `${Math.ceil(h + verticalCameraRange / PREVIEW_SCROLL_PAN_RATE)}px`;
      }
      // Keep the requested zoom percentage independent of map dimensions. The canvas is
      // rendered at the engine's nearest tile size, then scaled to the exact selected zoom.
      const renderW = Math.ceil(w / previewRenderScale);
      const renderH = Math.ceil(h / previewRenderScale);
      const now = performance.now();
      const dt = Math.min(0.08, Math.max(0, (now - lastFrame) / 1000));
      lastFrame = now;
      renderScaleRef.current = previewRenderScale;
      const bufferW = Math.max(1, Math.floor(renderW * dpr));
      const bufferH = Math.max(1, Math.floor(renderH * dpr));
      if (canvas.width !== bufferW) canvas.width = bufferW;
      if (canvas.height !== bufferH) canvas.height = bufferH;
      canvas.style.width = `${w}px`;
      canvas.style.height = `${h}px`;
      ctx.setSize(canvas.width, canvas.height);
      if (unitsCanvas && unitsCtx) {
        if (unitsCanvas.width !== bufferW) unitsCanvas.width = bufferW;
        if (unitsCanvas.height !== bufferH) unitsCanvas.height = bufferH;
        unitsCanvas.style.width = `${w}px`;
        unitsCanvas.style.height = `${h}px`;
        unitsCtx.setSize(unitsCanvas.width, unitsCanvas.height);
      }
      const drawGroundAndUnits = () => {
        ctx.clear();
        engine.renderGround(ctx, renderW, renderH, dpr);
        if (tacticsView) {
          unitsCtx?.clear();
          return;
        }
        if (unitsCtx && unitsCanvas) {
          unitsCtx.clear();
          unitsCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
          unitsCtx.clearRect(0, 0, renderW, renderH);
          const drawDecorationsOverFx = !!pixelRenderer && !hasArchitecture && !!fx?.hasEffects();
          engine.renderUnitsAndOverlays(
            unitsCtx, renderW, renderH, undefined,
            !!pixelRenderer && !drawDecorationsOverFx,
            !!pixelRenderer && !drawDecorationsOverFx,
            !!pixelRenderer && !drawDecorationsOverFx,
            !!pixelRenderer,
            !!pixelRenderer && !drawDecorationsOverFx,
          );
        }
      };
      drawGroundAndUnits();
      if (needsCameraRestore) {
        const savedCamera = cameraRef.current;
        if (savedCamera) {
          const centerX = (savedCamera.x + savedCamera.viewW / 2) / savedCamera.tile;
          const centerY = (savedCamera.y + savedCamera.viewH / 2) / savedCamera.tile;
          engine.restoreCamera({
            x: centerX * previewTileRadius - renderW / 2,
            y: centerY * previewTileRadius - renderH / 2,
          });
          drawGroundAndUnits();
        }
        needsCameraRestore = false;
      } else if (needsInitialCenter) {
        // First-ever mount for this draft: the draw() above just ran the engine's own
        // first-render focus (a spawn unit, or nowhere at all on a still-empty draft) —
        // override it so the preview opens at the party's starting location, not the board
        // edge or a stale camera from another map. Only runs once —
        // drawGroundAndUnits() can rerun many times after this (resize, elemental-FX
        // animation frames) and must never re-center over panning the author already did.
        engine.centerOnStartingParty();
        drawGroundAndUnits();
        needsInitialCenter = false;
      }
      // Drawn on the units canvas (top layer) so the highlight stays visible over units too,
      // matching where it used to land back when everything shared one canvas.
      const highlightCtx = unitsCtx ?? ctx;
      const highlight = highlightRef.current;
      if (!tacticsView && highlight.selectedPlacedDecoration) engine.drawDecorationHighlight(highlightCtx, highlight.selectedPlacedDecoration.id, highlight.selectedPlacedDecoration);
      else if (!tacticsView && highlight.selectedDecorationId) engine.drawDecorationHighlight(highlightCtx, highlight.selectedDecorationId);
      if (pixelFxCanvas && pixelRenderer) {
        pixelFxCanvas.style.width = `${w}px`;
        pixelFxCanvas.style.height = `${h}px`;
        pixelFxCanvas.style.display = "block";
        // Keep 3D materials sharp when exact editor zoom scales the canvas up.
        pixelRenderer.setSize(renderW, renderH, dpr * Math.max(1, previewRenderScale));
        const drawDecorationsOverFx = !tacticsView && !hasArchitecture && !!fx?.hasEffects();
        pixelRenderer.setSpritesAndDecorationsVisible(!drawDecorationsOverFx, !drawDecorationsOverFx);
        pixelRenderer.render(renderW, renderH);
      } else if (pixelFxCanvas) pixelFxCanvas.style.display = "none";
      if (fx && fxCanvas) {
        if (fx.hasEffects()) {
          fxCanvas.style.width = `${w}px`;
          fxCanvas.style.height = `${h}px`;
          fx.resize(renderW, renderH, dpr);
          fxCanvas.style.display = "block";
          // When a procedural-pixel placement is present, ThreeBattleRenderer owns the visible
          // ground canvas. Composite regular 2D FX over that rendered scene; using the base
          // Canvas2D map here would cover the pixel layer with a stale copy of the map.
          fx.render(pixelRenderer && pixelFxCanvas ? pixelFxCanvas : canvas, dt, (col, row) => {
            const anchor = engine.effectAnchor(col, row);
            if (!pixelRenderer) return anchor;
            // Match BattleCanvas: elemental FX follow the rendered camera and terrain,
            // using logical preview pixels before the canvas's editor zoom scaling.
            const point = pixelRenderer.projectFlatScreen(anchor.x, anchor.y, renderW, renderH, true);
            return { ...anchor, x: point.x, y: point.y };
          });
        } else {
          fxCanvas.style.display = "none";
        }
      }
    };

    redrawRef.current = draw;
    draw();
    // Placements are static in this editor preview (no camera-independent trigger redraws
    // them), so a small self-sustaining loop keeps their animation running; it's a no-op
    // draw() call once fx.hasEffects() goes false, and stops itself right after.
    let fxRaf = 0;
    let lastAnimatedFrame = 0;
    const animateFx = (time: number) => {
      if (!fx?.hasEffects() && !(pixelRenderer && pixelPlacements.length > 0)) return;
      // Leave input processing room between expensive editor frames. Direct edits,
      // selections and pan gestures still redraw immediately.
      if (time - lastAnimatedFrame >= 1000 / 30) { draw(); lastAnimatedFrame = time; }
      fxRaf = requestAnimationFrame(animateFx);
    };
    if (fx?.hasEffects() || (pixelRenderer && pixelPlacements.length > 0)) fxRaf = requestAnimationFrame(animateFx);
    // A texture can arrive after the first static editor draw. Refresh once loaded
    // instead of paying for a permanent full-scene render loop on a wall-only map.
    const textureRefresh = window.setTimeout(draw, 1200);
    if (!verticalScrollInitializedRef.current) {
      requestAnimationFrame(() => {
        const centeredTop = Math.round(Math.max(0, viewport.scrollHeight - viewport.clientHeight) / 2);
        const centeredLeft = Math.round(Math.max(0, viewport.scrollWidth - viewport.clientWidth) / 2);
        verticalScrollTopRef.current = centeredTop;
        horizontalScrollLeftRef.current = centeredLeft;
        viewport.scrollTop = centeredTop;
        viewport.scrollLeft = centeredLeft;
        verticalScrollInitializedRef.current = true;
      });
    }
    const ro = new ResizeObserver(draw);
    ro.observe(viewport);
    return () => {
      ro.disconnect();
      window.clearTimeout(textureRefresh);
      if (fxRaf) cancelAnimationFrame(fxRaf);
      fx?.dispose();
      const camera = engine.cameraPosition();
      const scale = renderScaleRef.current;
      cameraRef.current = {
        ...camera,
        missionId: mission.id,
        tile: ZOOM_RADII[engine.zoom]!,
        viewW: (viewportRef.current?.clientWidth ?? 1) / scale,
        viewH: (viewportRef.current?.clientHeight ?? 1) / scale,
      };
      if (engineRef.current === engine) engineRef.current = null;
      if (redrawRef.current === draw) redrawRef.current = null;
    };
  }, [mission, art, zoom, architectureActive, tacticsView, viewYaw, viewTilt]);

  useEffect(() => () => {
    architectureRendererRef.current?.dispose();
    architectureRendererRef.current = null;
  }, []);

  useEffect(() => { redrawRef.current?.(); }, [selectedDecorationId, selectedPlacedDecoration]);

  useEffect(() => {
    const deleteHeldUnit = (event: KeyboardEvent) => {
      if (event.key !== "Delete") return;
      if ((event.target as HTMLElement | null)?.closest("input, textarea, select, [contenteditable=true]")) return;
      const held = unitDragRef.current;
      if (!held) return;
      event.preventDefault();
      onHeldUnitDelete?.(held.unit);
      const viewport = viewportRef.current;
      if (viewport?.hasPointerCapture(held.pointerId)) viewport.releasePointerCapture(held.pointerId);
      unitDragRef.current = null;
      setIsDragging(false);
    };
    window.addEventListener("keydown", deleteHeldUnit, true);
    return () => window.removeEventListener("keydown", deleteHeldUnit, true);
  }, [onHeldUnitDelete]);

  const previewCellAt = (px: number, py: number) => {
    const engine = engineRef.current;
    const canvas = canvasRef.current;
    if (!engine || !canvas) return null;
    const renderer = architectureRendererRef.current;
    if (tacticsView && renderer) {
      const scale = renderScaleRef.current;
      const point = renderer.screenToFlatScreen(px, py, canvas.clientWidth / scale, canvas.clientHeight / scale);
      return engine.cellAt(point.x, point.y);
    }
    return engine.cellAt(px, py);
  };
  const panPreview = (dx: number, dy: number) => {
    const engine = engineRef.current;
    const renderer = architectureRendererRef.current;
    const canvas = canvasRef.current;
    if (tacticsView && renderer && canvas) {
      const scale = renderScaleRef.current;
      const w = canvas.clientWidth / scale, h = canvas.clientHeight / scale;
      const a = renderer.screenToFlatScreen(w / 2, h / 2, w, h, false);
      const b = renderer.screenToFlatScreen(w / 2 + dx, h / 2 + dy, w, h, false);
      engine?.panBy(b.x - a.x, b.y - a.y);
    } else engine?.panBy(dx, dy);
  };
  const onPointerDown = (event: PointerEvent<HTMLDivElement>) => {
    const viewport = viewportRef.current;
    if (!viewport) return;
    // Leave native scrollbar gutters available to the scrollbar thumb. Capturing those
    // pointer events as map gestures made it impossible to drag the view back from an edge.
    const bounds = viewport.getBoundingClientRect();
    const contentLeft = bounds.left + viewport.clientLeft;
    const contentTop = bounds.top + viewport.clientTop;
    if (
      event.clientX < contentLeft ||
      event.clientX >= contentLeft + viewport.clientWidth ||
      event.clientY < contentTop ||
      event.clientY >= contentTop + viewport.clientHeight
    ) return;
    // Units are deliberately picked up with the secondary button. The primary button stays
    // available for the map itself: a held left-drag pans, while an ordinary left click uses
    // the active paint brush.
    if (event.button === 2 || (event.button === 0 && primaryObjectDrag)) {
      event.preventDefault();
      const canvas = canvasRef.current;
      const engine = engineRef.current;
      if (!canvas || !engine) return;
      viewport.focus({ preventScroll: true });
      const rect = canvas.getBoundingClientRect();
      const scale = renderScaleRef.current;
      const px = (event.clientX - rect.left) / scale;
      const py = (event.clientY - rect.top) / scale;
      const cell = previewCellAt(px, py);
      // Try the exact hex first, then fall back to anywhere on the unit's drawn sprite — a
      // sprite commonly extends well beyond its own hex on screen (tall creatures especially),
      // which otherwise makes some units hard to grab.
      let unit = cell ? unitAt(cell.x, cell.y) : null;
      if (!unit) {
        const spriteUnit = engine.unitSpriteAt(px, py);
        if (spriteUnit) unit = unitAt(spriteUnit.x, spriteUnit.y);
      }
      if (unit) {
        unitDragRef.current = { pointerId: event.pointerId, unit, startX: event.clientX, startY: event.clientY, moved: false };
        viewport.setPointerCapture(event.pointerId);
        onUnitSelect?.(unit);
        setIsDragging(true);
        return;
      }
      const architecture = architectureRendererRef.current?.pickArchitecture(px, py, canvas.clientWidth / scale, canvas.clientHeight / scale);
      const groundDecoration = cell ? decorationAt(cell.x, cell.y) : null;
      const decoration = groundDecoration && DECORATIONS[groundDecoration.id]?.exitKind
        ? groundDecoration : architecture ?? groundDecoration;
      if (event.button === 0 && decoration &&
          DECORATIONS[highlightRef.current.selectedDecorationId ?? ""]?.model3d &&
          DECORATIONS[decoration.id]?.model3d &&
          highlightRef.current.selectedDecorationId !== decoration.id) {
        // Painting a door onto a wall replaces its module at the wall's anchor.
        // Picking the wall here would otherwise change the brush before placement.
        onCellClickRef.current?.(decoration.x, decoration.y);
        return;
      }
      if (decoration) {
        unitDragRef.current = null;
        decorationDragRef.current = { pointerId: event.pointerId, decoration, startX: event.clientX, startY: event.clientY, moved: false };
        viewport.setPointerCapture(event.pointerId);
        onDecorationSelect?.(decoration);
        setIsDragging(true);
        return;
      }
      // Right-clicking empty ground is intentionally inert: map panning belongs to the
      // primary-button hold gesture below, so it never competes with moving a unit.
      if (event.button === 2) return;
    }
    if (event.button !== 0) return;
    dragRef.current = {
      pointerId: event.pointerId,
      x: event.clientX,
      y: event.clientY,
      startX: event.clientX,
      startY: event.clientY,
      armed: false,
      moved: false,
    };
    viewport.setPointerCapture(event.pointerId);
    armTimerRef.current = window.setTimeout(() => {
      const drag = dragRef.current;
      if (drag?.pointerId !== event.pointerId || drag.moved) return;
      drag.armed = true;
      // The hand cursor confirms that the hold-to-pan gesture is armed.
      setIsPanning(true);
    }, PREVIEW_PAN_HOLD_MS);
  };
  const updateHoverTile = (event: PointerEvent<HTMLDivElement>) => {
    const canvas = canvasRef.current;
    const engine = engineRef.current;
    const outer = viewportRef.current?.parentElement;
    if (!canvas || !engine || !outer) return;
    const rect = canvas.getBoundingClientRect();
    const scale = renderScaleRef.current;
    const cell = previewCellAt((event.clientX - rect.left) / scale, (event.clientY - rect.top) / scale);
    const terrain = cell ? TERRAIN_BY_CHAR[mission.layout[cell.y]?.[cell.x] ?? ""] : undefined;
    if (!cell || !terrain) {
      setHoverTile(null);
      return;
    }
    const variant = mission.tileVariants?.[cell.y * mission.cols + cell.x] ?? 0;
    const outerRect = outer.getBoundingClientRect();
    setHoverTile({
      key: `${cell.x},${cell.y}`,
      label: `${TERRAIN[terrain].name} — ${tileVariantName(terrain, variant)} · Nível ${mission.terrainElevations?.[cell.y * mission.cols + cell.x] ?? TERRAIN[terrain].height ?? 0}`,
      left: event.clientX - outerRect.left + 14,
      top: event.clientY - outerRect.top + 16,
    });
  };
  const onPointerMove = (event: PointerEvent<HTMLDivElement>) => {
    updateHoverTile(event);
    const unitDrag = unitDragRef.current;
    if (unitDrag?.pointerId === event.pointerId) {
      // Same 6px threshold as the pan gesture below: past it, this is a deliberate drag to a
      // new hex, not a pick-up-and-hold — see endDrag, which only drops the unit if moved.
      if (!unitDrag.moved && Math.hypot(event.clientX - unitDrag.startX, event.clientY - unitDrag.startY) >= 6) {
        unitDrag.moved = true;
      }
      return;
    }
    const decorationDrag = decorationDragRef.current;
    if (decorationDrag?.pointerId === event.pointerId) {
      if (Math.hypot(event.clientX - decorationDrag.startX, event.clientY - decorationDrag.startY) >= 6) decorationDrag.moved = true;
      return;
    }
    const viewport = viewportRef.current;
    const drag = dragRef.current;
    if (!viewport || !drag || drag.pointerId !== event.pointerId) return;
    const dx = event.clientX - drag.x;
    const dy = event.clientY - drag.y;
    // The brush owns normal clicks and drags. Pan begins only after the 0.5-second hold
    // timer arms it, then a real movement, so it can never auto-activate.
    if (!drag.moved) {
      const movedFarEnough = Math.hypot(event.clientX - drag.startX, event.clientY - drag.startY) >= 6;
      if (drag.armed && movedFarEnough) {
        drag.moved = true;
      }
    }
    if (drag.moved) {
      const scale = renderScaleRef.current;
      const panX = -dx / scale;
      const panY = -dy / scale;
      panPreview(panX, panY);
      // Keep the scrollbar thumb in step with pointer panning. Updating the refs first means
      // the resulting scroll event won't apply the same camera movement a second time.
      const scrollLeft = Math.max(0, Math.min(viewport.scrollWidth - viewport.clientWidth, viewport.scrollLeft + panX * scale / PREVIEW_SCROLL_PAN_RATE));
      const scrollTop = Math.max(0, Math.min(viewport.scrollHeight - viewport.clientHeight, viewport.scrollTop + panY * scale / PREVIEW_SCROLL_PAN_RATE));
      horizontalScrollLeftRef.current = scrollLeft;
      verticalScrollTopRef.current = scrollTop;
      viewport.scrollLeft = scrollLeft;
      viewport.scrollTop = scrollTop;
      redrawRef.current?.();
    }
    drag.x = event.clientX;
    drag.y = event.clientY;
  };

  const endDrag = (event: PointerEvent<HTMLDivElement>, cancelled = false) => {
    const viewport = viewportRef.current;
    if (!viewport) return;
    const unitDrag = unitDragRef.current;
    if (unitDrag?.pointerId === event.pointerId) {
      if (viewport.hasPointerCapture(event.pointerId)) viewport.releasePointerCapture(event.pointerId);
      // An ordinary right-click — press and release without dragging — is a pick-up-and-hold,
      // not a move: releasing the button must not drop the unit back onto the board, and the
      // selection has to survive the release so Delete still works afterward (a real mouse
      // click is a near-instant press+release; requiring Delete to land before the button
      // comes back up made the whole gesture impossible to actually perform). Only a real
      // drag (unitDrag.moved) or a cancel ends the hold here.
      if (!unitDrag.moved) {
        if (cancelled) unitDragRef.current = null;
        setIsDragging(false);
        return;
      }
      if (!cancelled) {
        const canvas = canvasRef.current;
        const engine = engineRef.current;
        if (canvas && engine) {
          const rect = canvas.getBoundingClientRect();
          const scale = renderScaleRef.current;
          const cell = previewCellAt((event.clientX - rect.left) / scale, (event.clientY - rect.top) / scale);
          if (cell) onUnitPlace?.(unitDrag.unit, cell.x, cell.y);
        }
      }
      unitDragRef.current = null;
      setIsDragging(false);
      return;
    }
    const decorationDrag = decorationDragRef.current;
    if (decorationDrag?.pointerId === event.pointerId) {
      if (!cancelled && decorationDrag.moved) {
        const canvas = canvasRef.current;
        const engine = engineRef.current;
        if (canvas && engine) {
          const rect = canvas.getBoundingClientRect();
          const scale = renderScaleRef.current;
          const cell = previewCellAt((event.clientX - rect.left) / scale, (event.clientY - rect.top) / scale);
          if (cell) onDecorationPlace?.(decorationDrag.decoration, cell.x, cell.y);
        }
      }
      if (viewport.hasPointerCapture(event.pointerId)) viewport.releasePointerCapture(event.pointerId);
      decorationDragRef.current = null;
      setIsDragging(false);
      return;
    }
    const drag = dragRef.current;
    if (!drag || drag.pointerId !== event.pointerId) return;
    if (armTimerRef.current !== null) {
      window.clearTimeout(armTimerRef.current);
      armTimerRef.current = null;
    }
    // Normal left click remains the terrain/decorations brush. Panning is still hold + drag.
    if (!cancelled && !drag.moved && event.button === 0) {
      const canvas = canvasRef.current;
      const engine = engineRef.current;
      if (canvas && engine) {
        const rect = canvas.getBoundingClientRect();
        const scale = renderScaleRef.current;
        const cell = previewCellAt((event.clientX - rect.left) / scale, (event.clientY - rect.top) / scale);
        if (cell) {
          const px = (event.clientX - rect.left) / scale, py = (event.clientY - rect.top) / scale;
          const renderer = architectureRendererRef.current;
          const flat = tacticsView && renderer ? renderer.screenToFlatScreen(px, py, canvas.clientWidth / scale, canvas.clientHeight / scale) : { x: px, y: py };
          const tile = ZOOM_RADII[engine.zoom]!;
          onCellClickRef.current?.(cell.x, cell.y, { x: (flat.x + engine.camX) / tile, y: (flat.y + engine.camY) / tile });
        }
      }
    }
    if (viewport.hasPointerCapture(event.pointerId)) viewport.releasePointerCapture(event.pointerId);
    dragRef.current = null;
    setIsPanning(false);
  };
  const onViewportScroll = () => {
    const viewport = viewportRef.current;
    if (!viewport) return;
    const deltaY = viewport.scrollTop - verticalScrollTopRef.current;
    const deltaX = viewport.scrollLeft - horizontalScrollLeftRef.current;
    verticalScrollTopRef.current = viewport.scrollTop;
    horizontalScrollLeftRef.current = viewport.scrollLeft;
    if (!deltaX && !deltaY) return;
    const scale = renderScaleRef.current;
    panPreview(deltaX * PREVIEW_SCROLL_PAN_RATE / scale, deltaY * PREVIEW_SCROLL_PAN_RATE / scale);
    redrawRef.current?.();
  };

  return (
    <div className="relative h-full w-full overflow-hidden bg-black">
      <div className="absolute top-2 left-2 z-30 flex gap-1 text-xs">
        <button type="button" className="rounded border border-border bg-bg px-2 py-1" onClick={() => onTacticsViewChange?.(!tacticsView)}>{tacticsView ? "Vista 3D" : "Vista superior"}</button>
        {tacticsView && <>
          <button type="button" aria-label="Girar prévia à esquerda" className="rounded border border-border bg-bg px-2" onClick={() => setViewYaw(v => (v - 60 + 360) % 360)}>↶</button>
          <button type="button" aria-label="Girar prévia à direita" className="rounded border border-border bg-bg px-2" onClick={() => setViewYaw(v => (v + 60) % 360)}>↷</button>
          <button type="button" aria-label="Diminuir inclinação da prévia" className="rounded border border-border bg-bg px-2" onClick={() => setViewTilt(v => Math.max(25, v - 10))}>↓</button>
          <button type="button" aria-label="Aumentar inclinação da prévia" className="rounded border border-border bg-bg px-2" onClick={() => setViewTilt(v => Math.min(55, v + 10))}>↑</button>
          <button type="button" className="rounded border border-border bg-bg px-2" onClick={() => { setViewYaw(30); setViewTilt(45); }}>Reset</button>
        </>}
      </div>
      <div className="absolute right-2 top-2 z-10 flex overflow-hidden rounded border border-border bg-surface shadow-md">
        <button
          type="button"
          className="h-7 w-7 text-base text-fg hover:bg-surface-2 disabled:cursor-not-allowed disabled:opacity-40"
          aria-label="Diminuir zoom da prévia"
          title="Diminuir zoom"
          disabled={zoom <= PREVIEW_ZOOM_MIN}
          onClick={() => setZoom((value) => Math.max(PREVIEW_ZOOM_MIN, Number((value - 0.25).toFixed(2))))}
        >
          −
        </button>
        <button
          type="button"
          className="min-w-12 border-x border-border px-1 text-[10px] font-semibold text-fg hover:bg-surface-2"
          aria-label="Restaurar zoom da prévia"
          title="Restaurar zoom"
          onClick={() => setZoom(PREVIEW_ZOOM_MIN)}
        >
          {Math.round(zoom * 100)}%
        </button>
        <button
          type="button"
          className="h-7 w-7 text-base text-fg hover:bg-surface-2 disabled:cursor-not-allowed disabled:opacity-40"
          aria-label="Aumentar zoom da prévia"
          title="Aumentar zoom"
          disabled={zoom >= 2.5}
          onClick={() => setZoom((value) => Math.min(2.5, Number((value + 0.25).toFixed(2))))}
        >
          +
        </button>
      </div>
      <div
        ref={viewportRef}
        tabIndex={0}
        // cursor-default forces the game pointer with !important, which would hide the grab hand.
        className={`h-full w-full bg-black ember-scrollbar overflow-x-auto overflow-y-scroll${isDragging || isPanning ? " preview-pan-grab" : " cursor-default"}`}
        style={{
          scrollbarGutter: "stable both-edges",
        }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={(event) => endDrag(event, true)}
        onLostPointerCapture={(event) => { if (event.buttons !== 0) endDrag(event, true); }}
        onPointerLeave={() => setHoverTile(null)}
        onContextMenu={(event) => event.preventDefault()}
        onScroll={onViewportScroll}
      >
        <div ref={scrollContentRef} style={{ width: "100%", minHeight: "100%" }}>
          <div className="sticky left-0 top-0 w-max">
            <div className="relative">
              <canvas ref={canvasRef} className="block" />
              <canvas ref={pixelFxCanvasRef} className="pointer-events-none absolute inset-0 block" style={{ display: "none" }} />
              <canvas ref={fxCanvasRef} className="pointer-events-none absolute inset-0 block" style={{ display: "none" }} />
              <canvas ref={unitsCanvasRef} className="pointer-events-none absolute inset-0 block" />
            </div>
          </div>
        </div>
      </div>
      {hoverTile && (
        <div
          className="pointer-events-none absolute z-10 whitespace-nowrap rounded border border-border bg-surface px-2 py-1 text-xs text-fg shadow-md"
          style={{ left: hoverTile.left, top: hoverTile.top }}
        >
          {hoverTile.label}
        </div>
      )}
    </div>
  );
}
