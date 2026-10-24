// Ember's MapPreviewCanvas (src/game/MapPreviewCanvas.tsx), rebuilt on Engine2's 3D map. Same props,
// same gestures, same overlay buttons, so Ember's map editor drives it unchanged:
//  - left click: the editor's current brush on that hex;
//  - hold the left button 0.5 s, then drag: pan (DO NOT MESS WITH CONTROLS — Ember's rule);
//  - right-drag a unit or decoration: move it (Delete while holding a unit removes it);
//  - right-drag empty ground: turn the camera (Engine2's editor spins freely);
//  - wheel: zoom. Ember's top-left buttons turn/tilt the view; "Reset" returns to the normal view.

import { type PointerEvent, useEffect, useRef, useState } from 'react';
import { DECORATIONS, TERRAIN, placedFootprint } from '../../ember/data';
import type { GameArt, Mission } from '../../ember/types';
import { tileVariantName } from '../../ember/tileVariants';
import { DioramaView } from './dioramaView';
export type PreviewUnitSelection = {
  side: 'playerSpawns' | 'enemySpawns' | 'neutralSpawns';
  index: number;
  name: string;
};

export type PreviewDecorationSelection = { id: string; x: number; y: number; rot?: number };

const PREVIEW_ZOOM_MIN = 0.75;

export function MapPreviewCanvas({
  mission,
  onCellClick,
  selectedPlacedDecoration,
  onUnitSelect,
  onHeldUnitDelete,
  onUnitPlace,
  onDecorationSelect,
  onDecorationPlace,
  primaryObjectDrag = true,
}: {
  mission: Mission;
  art?: GameArt;
  onCellClick?: (x: number, y: number, point?: { x: number; y: number }) => void;
  selectedDecorationId?: string;
  selectedPlacedDecoration?: PreviewDecorationSelection | null;
  onUnitSelect?: (unit: PreviewUnitSelection) => void;
  /** Delete is deliberate: it only applies while the author is holding a placed unit. */
  onHeldUnitDelete?: (unit: PreviewUnitSelection) => void;
  onUnitPlace?: (unit: PreviewUnitSelection, x: number, y: number) => void;
  onDecorationSelect?: (decoration: PreviewDecorationSelection) => void;
  onDecorationPlace?: (decoration: PreviewDecorationSelection, x: number, y: number) => void;
  primaryObjectDrag?: boolean;
  tacticsView?: boolean;
  onTacticsViewChange?: (enabled: boolean) => void;
}) {
  const hostRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<DioramaView | null>(null);
  const onCellClickRef = useRef(onCellClick);
  onCellClickRef.current = onCellClick;
  const missionRef = useRef(mission);
  missionRef.current = mission;
  // Same pick-up-and-hold rules as Ember: a plain right-click holds the unit (Delete removes it);
  // only a real drag past 6 px drops it somewhere else on release.
  const unitDragRef = useRef<{ pointerId: number; unit: PreviewUnitSelection; startX: number; startY: number; moved: boolean } | null>(null);
  const decorationDragRef = useRef<{ pointerId: number; decoration: PreviewDecorationSelection; startX: number; startY: number; moved: boolean } | null>(null);
  const [zoom, setZoom] = useState(1);
  const [isDragging, setIsDragging] = useState(false);
  const [hoverTile, setHoverTile] = useState<{ label: string; left: number; top: number } | null>(null);

  useEffect(() => {
    const view = new DioramaView(hostRef.current!);
    view.decor.realModels = true; // the editor shows Ember's real 3D props, not their pictures
    viewRef.current = view;
    view.rig.onClick = (event) => {
      const cell = view.cellAt(event.clientX, event.clientY);
      if (cell) onCellClickRef.current?.(cell.x, cell.y, cell.point);
    };
    return () => { view.dispose(); viewRef.current = null; };
  }, []);

  useEffect(() => { void viewRef.current?.setMission(mission); }, [mission]);
  useEffect(() => { viewRef.current?.setSelection(selectedPlacedDecoration); }, [selectedPlacedDecoration, mission]);
  useEffect(() => { viewRef.current?.setZoom(zoom); }, [zoom]);

  useEffect(() => {
    const deleteHeldUnit = (event: KeyboardEvent) => {
      if (event.key !== 'Delete') return;
      if ((event.target as HTMLElement | null)?.closest('input, textarea, select, [contenteditable=true]')) return;
      const held = unitDragRef.current;
      if (!held) return;
      event.preventDefault();
      onHeldUnitDelete?.(held.unit);
      if (hostRef.current?.hasPointerCapture(held.pointerId)) hostRef.current.releasePointerCapture(held.pointerId);
      unitDragRef.current = null;
      setIsDragging(false);
    };
    window.addEventListener('keydown', deleteHeldUnit, true);
    return () => window.removeEventListener('keydown', deleteHeldUnit, true);
  }, [onHeldUnitDelete]);

  const unitAtCell = (x: number, y: number): PreviewUnitSelection | null => {
    const m = missionRef.current;
    for (const side of ['playerSpawns', 'enemySpawns', 'neutralSpawns'] as const) {
      const index = (m[side] ?? []).findIndex((u) => u.x === x && u.y === y);
      if (index >= 0) return { side, index, name: m[side]![index]!.name };
    }
    return null;
  };
  const decorationAt = (x: number, y: number): PreviewDecorationSelection | null => {
    const hits = (missionRef.current.decorations ?? []).filter((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));
    const hit = hits.find((p) => !!DECORATIONS[p.id]?.exitKind) ?? hits[0];
    return hit ? { id: hit.id, x: hit.x, y: hit.y, rot: hit.rot } : null;
  };

  // Capture phase: objects are picked before the camera rig sees the press, so a right-drag on a
  // unit moves the unit and a right-drag anywhere else turns the camera.
  const onPointerDownCapture = (event: PointerEvent<HTMLDivElement>) => {
    const view = viewRef.current, host = hostRef.current;
    if (!view || !host) return;
    if (!(event.button === 2 || (event.button === 0 && primaryObjectDrag))) return;
    const cell = view.cellAt(event.clientX, event.clientY);
    const sprite = view.unitAt(event.clientX, event.clientY);
    const unit = (cell ? unitAtCell(cell.x, cell.y) : null) ?? (sprite ? { side: sprite.side, index: sprite.index, name: sprite.name } : null);
    if (unit) {
      event.stopPropagation(); event.preventDefault();
      unitDragRef.current = { pointerId: event.pointerId, unit, startX: event.clientX, startY: event.clientY, moved: false };
      host.setPointerCapture(event.pointerId);
      onUnitSelect?.(unit);
      setIsDragging(true);
      return;
    }
    const decoration = cell ? decorationAt(cell.x, cell.y) : null;
    if (decoration) {
      event.stopPropagation(); event.preventDefault();
      decorationDragRef.current = { pointerId: event.pointerId, decoration, startX: event.clientX, startY: event.clientY, moved: false };
      host.setPointerCapture(event.pointerId);
      onDecorationSelect?.(decoration);
      setIsDragging(true);
    }
  };

  const updateHover = (event: PointerEvent<HTMLDivElement>) => {
    const view = viewRef.current, host = hostRef.current;
    if (!view || !host) return;
    const cell = view.cellAt(event.clientX, event.clientY);
    view.setHover(cell);
    const m = missionRef.current;
    const terrain = cell ? view.board?.cell(cell.x, cell.y).type : undefined;
    if (!cell || !terrain || !TERRAIN[terrain]) { setHoverTile(null); return; }
    const i = cell.y * m.cols + cell.x;
    const variant = m.tileVariants?.[i] ?? 0;
    const rect = host.getBoundingClientRect();
    setHoverTile({
      label: `${TERRAIN[terrain].name} — ${tileVariantName(terrain, variant)} · Nível ${m.terrainElevations?.[i] ?? TERRAIN[terrain].height ?? 0}`,
      left: event.clientX - rect.left + 14,
      top: event.clientY - rect.top + 16,
    });
  };

  const onPointerMove = (event: PointerEvent<HTMLDivElement>) => {
    updateHover(event);
    for (const drag of [unitDragRef.current, decorationDragRef.current]) {
      if (drag?.pointerId === event.pointerId && !drag.moved && Math.hypot(event.clientX - drag.startX, event.clientY - drag.startY) >= 6) drag.moved = true;
    }
  };

  const endDrag = (event: PointerEvent<HTMLDivElement>, cancelled = false) => {
    const view = viewRef.current, host = hostRef.current;
    if (!view || !host) return;
    const release = () => { if (host.hasPointerCapture(event.pointerId)) host.releasePointerCapture(event.pointerId); };
    const unitDrag = unitDragRef.current;
    if (unitDrag?.pointerId === event.pointerId) {
      release();
      if (!unitDrag.moved) { if (cancelled) unitDragRef.current = null; setIsDragging(false); return; }
      const cell = cancelled ? null : view.cellAt(event.clientX, event.clientY);
      if (cell) onUnitPlace?.(unitDrag.unit, cell.x, cell.y);
      unitDragRef.current = null;
      setIsDragging(false);
      return;
    }
    const decorationDrag = decorationDragRef.current;
    if (decorationDrag?.pointerId === event.pointerId) {
      release();
      const cell = !cancelled && decorationDrag.moved ? view.cellAt(event.clientX, event.clientY) : null;
      if (cell) onDecorationPlace?.(decorationDrag.decoration, cell.x, cell.y);
      decorationDragRef.current = null;
      setIsDragging(false);
    }
  };

  const view = () => viewRef.current;
  return (
    <div className="relative h-full w-full overflow-hidden bg-black">
      <div className="absolute top-2 left-2 z-30 flex gap-1 text-xs">
        <button type="button" aria-label="Girar prévia à esquerda" className="rounded border border-border bg-bg px-2" onClick={() => view()?.turn(-60)}>↶</button>
        <button type="button" aria-label="Girar prévia à direita" className="rounded border border-border bg-bg px-2" onClick={() => view()?.turn(60)}>↷</button>
        <button type="button" aria-label="Diminuir inclinação da prévia" className="rounded border border-border bg-bg px-2" onClick={() => view()?.tilt(-10)}>↓</button>
        <button type="button" aria-label="Aumentar inclinação da prévia" className="rounded border border-border bg-bg px-2" onClick={() => view()?.tilt(10)}>↑</button>
        <button type="button" className="rounded border border-border bg-bg px-2" title="Voltar ao ângulo normal" onClick={() => view()?.resetAngle()}>Reset</button>
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
          onClick={() => setZoom(1)}
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
        ref={hostRef}
        tabIndex={0}
        className={`h-full w-full bg-black${isDragging ? ' preview-pan-grab' : ''}`}
        onPointerDownCapture={onPointerDownCapture}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={(event) => endDrag(event, true)}
        onPointerLeave={() => { setHoverTile(null); viewRef.current?.setHover(null); }}
        onContextMenu={(event) => event.preventDefault()}
      />
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
