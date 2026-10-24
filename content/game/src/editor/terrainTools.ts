// Terrain editing: elevation brushes (raise, lower, flatten, smooth) and terrain painting, on the
// hex board. Edits Ember's own map fields (terrainElevations, tiles), so saved maps stay Ember maps.

import type { EmberDecoration, EmberMapDraft, TerrainId } from '../map/emberMap';
import { TERRAIN_INFO, LEVEL_MIN, LEVEL_MAX, type Board } from '../map/board';

export type Tool = 'none' | 'raise' | 'lower' | 'flatten' | 'smooth' | 'paint';

const TOOL_LABEL: Record<Tool, string> = { none: 'Ver', raise: 'Elevar', lower: 'Rebaixar', flatten: 'Nivelar', smooth: 'Suavizar', paint: 'Pintar' };
const SIZES = [1, 2, 3, 5];

/** odd-r offset → cube distance */
function hexDist(c1: number, r1: number, c2: number, r2: number): number {
  const q1 = c1 - (r1 - (r1 & 1)) / 2, q2 = c2 - (r2 - (r2 & 1)) / 2;
  const dq = q1 - q2, dr = r1 - r2;
  return (Math.abs(dq) + Math.abs(dr) + Math.abs(dq + dr)) / 2;
}

interface Snapshot { tiles: TerrainId[]; elev: number[]; decorations: EmberDecoration[] }

export class TerrainTools {
  tool: Tool = 'none';
  /** brush radius in hexes from the centre (size 1 = just the hex) */
  size = 1;
  paintType: TerrainId = 'plains';
  private touched = new Set<number>();
  private flatLevel = 0;
  private undoStack: Snapshot[] = [];
  private currentDraft: EmberMapDraft | null = null;
  dirty = false;

  constructor(private readonly panel: HTMLElement, private readonly changed: () => void, private readonly save: () => void, private readonly undoAction: () => void) {
    this.render();
  }

  get active(): boolean { return this.tool !== 'none'; }

  setDraft(draft: EmberMapDraft): void {
    this.currentDraft = draft;
    this.render();
  }

  private elevations(d: EmberMapDraft): number[] {
    if (!d.terrainElevations || d.terrainElevations.length !== d.cols * d.rows) {
      const old = d.terrainElevations ?? [];
      d.terrainElevations = Array.from({ length: d.cols * d.rows }, (_, i) => old[i] ?? 0);
    }
    return d.terrainElevations;
  }

  /** Call before a click or a stroke: remembers the state for undo. */
  begin(b: Board): void {
    const d = b.draft;
    this.currentDraft = d;
    this.undoStack.push({
      tiles: [...d.tiles],
      elev: [...this.elevations(d)],
      decorations: (d.decorations ?? []).map((placement) => ({ ...placement })),
    });
    if (this.undoStack.length > 60) this.undoStack.shift();
    this.touched.clear();
    this.flatLevel = NaN;
  }

  undo(b: Board): boolean {
    const s = this.undoStack.pop();
    if (!s) return false;
    b.draft.tiles = s.tiles;
    b.draft.terrainElevations = s.elev;
    b.draft.decorations = s.decorations;
    this.currentDraft = b.draft;
    this.dirty = true;
    this.render();
    return true;
  }

  /** Apply the brush centred on hex (c, r). Returns true if anything changed. */
  apply(b: Board, c: number, r: number): boolean {
    const d = b.draft, elev = this.elevations(d), n = this.size - 1;
    this.currentDraft = d;
    if (Number.isNaN(this.flatLevel)) this.flatLevel = b.cell(c, r).level;
    const cells: number[] = [];
    for (let rr = Math.max(0, r - n); rr <= Math.min(d.rows - 1, r + n); rr++)
      for (let cc = Math.max(0, c - n - 1); cc <= Math.min(d.cols - 1, c + n + 1); cc++)
        if (hexDist(c, r, cc, rr) <= n) cells.push(rr * d.cols + cc);
    let any = false;
    for (const i of cells) {
      const cell = b.cells[i];
      if (this.tool === 'paint') {
        if (d.tiles[i] !== this.paintType) { d.tiles[i] = this.paintType; any = true; }
        continue;
      }
      // raise/lower step each hex once per stroke; clicks stack
      if ((this.tool === 'raise' || this.tool === 'lower') && this.touched.has(i)) continue;
      this.touched.add(i);
      let next = cell.level;
      if (this.tool === 'raise') next = cell.level + 1;
      else if (this.tool === 'lower') next = cell.level - 1;
      else if (this.tool === 'flatten') next = this.flatLevel;
      else if (this.tool === 'smooth') {
        const cc = i % d.cols, rr = Math.floor(i / d.cols);
        let s = cell.level, k = 1;
        for (let r2 = rr - 1; r2 <= rr + 1; r2++) for (let c2 = cc - 1; c2 <= cc + 1; c2++) {
          if (r2 < 0 || c2 < 0 || r2 >= d.rows || c2 >= d.cols || hexDist(cc, rr, c2, r2) !== 1) continue;
          s += b.cells[r2 * d.cols + c2].level; k++;
        }
        next = Math.round(s / k);
      }
      next = Math.max(LEVEL_MIN, Math.min(LEVEL_MAX, next));
      if (next !== cell.level) { elev[i] = next; any = true; }
    }
    if (any) { this.dirty = true; this.render(); }
    return any;
  }

  render(): void {
    const p = this.panel;
    p.innerHTML = '';
    const title = document.createElement('div'); title.className = 'tools-title'; title.textContent = 'Terreno 3D'; p.appendChild(title);
    const row = (cls = 'tools-row') => { const d = document.createElement('div'); d.className = cls; p.appendChild(d); return d; };
    const btn = (parent: HTMLElement, label: string, on: boolean, fn: () => void, title = '') => {
      const b = document.createElement('button'); b.textContent = label; b.title = title; if (on) b.classList.add('on'); b.addEventListener('click', fn); parent.appendChild(b); return b;
    };
    const tools = row();
    for (const t of Object.keys(TOOL_LABEL) as Tool[]) btn(tools, TOOL_LABEL[t], this.tool === t, () => { this.tool = t; this.render(); this.changed(); });
    const sizes = row();
    const lab = document.createElement('span'); lab.className = 'label'; lab.textContent = 'Tamanho'; sizes.appendChild(lab);
    for (const s of SIZES) btn(sizes, String(s), this.size === s, () => { this.size = s; this.render(); this.changed(); }, `${s} hex${s > 1 ? 'es' : ''} a partir do centro`);
    const catalogHead = row('terrain-catalog-head');
    const catalogTitle = document.createElement('span'); catalogTitle.textContent = 'Catálogo de terrenos'; catalogHead.appendChild(catalogTitle);
    const total = document.createElement('small'); total.textContent = `${Object.keys(TERRAIN_INFO).length} tipos`; catalogHead.appendChild(total);
    const counts = new Map<TerrainId, number>();
    for (const id of this.currentDraft?.tiles ?? []) counts.set(id, (counts.get(id) ?? 0) + 1);
    const catalog = document.createElement('div'); catalog.className = 'terrain-catalog';
    const surfaceNames: Record<string, string> = {
      grass: 'Grama', forest: 'Bosque', pavers: 'Laje', snow: 'Neve', rock: 'Rocha', gravel: 'Cascalho', ash: 'Cinza', void: 'Vazio',
    };
    const surfaceColors: Record<string, string> = {
      grass: 'linear-gradient(135deg,#3d7329,#79ab47 58%,#a39f55)', forest: 'linear-gradient(135deg,#263f1d,#55783a 62%,#7a5a2c)',
      pavers: 'repeating-linear-gradient(0deg,#4a4641 0 8px,#302d2a 8px 10px,#777168 10px 18px)', snow: 'linear-gradient(135deg,#aebfce,#f5f7f8)',
      rock: 'linear-gradient(135deg,#4f4b47,#a19a8f 55%,#5a554e)', gravel: 'radial-gradient(circle at 25% 30%,#aaa397 0 1px,#706b62 2px,#4d4a44 5px)',
      ash: 'radial-gradient(circle at 70% 35%,#ff7a2a 0 1px,#5b534d 3px,#2a2624 9px)', void: '#000000',
      water: 'linear-gradient(145deg,#87c7d8,#2f86a8 58%,#174b69)',
    };
    for (const [id, info] of Object.entries(TERRAIN_INFO) as [TerrainId, typeof TERRAIN_INFO[TerrainId]][]) {
      const button = document.createElement('button'); button.type = 'button'; button.className = 'terrain-choice';
      button.classList.toggle('on', id === this.paintType);
      const materialName = id === 'water' ? 'Água animada' : surfaceNames[info.surface];
      button.title = `${info.name} · ${materialName} · ${info.passable ? 'atravessável' : 'bloqueado'}`;
      if (this.tool !== 'paint') button.classList.add('catalog-view-only');
      const swatch = document.createElement('span'); swatch.className = 'terrain-swatch'; swatch.style.background = surfaceColors[id === 'water' ? 'water' : info.surface];
      const labels = document.createElement('span'); labels.className = 'terrain-choice-labels';
      const name = document.createElement('span'); name.className = 'terrain-choice-name'; name.textContent = info.name;
      const material = document.createElement('small'); material.textContent = materialName;
      labels.append(name, material);
      const count = document.createElement('small'); count.className = 'terrain-count'; count.textContent = String(counts.get(id) ?? 0);
      button.append(swatch, labels, count);
      button.addEventListener('click', () => {
        this.paintType = id; this.tool = 'paint'; this.render(); this.changed();
      });
      catalog.appendChild(button);
    }
    p.appendChild(catalog);
    const catalogNote = document.createElement('div'); catalogNote.className = 'muted terrain-catalog-note';
    catalogNote.textContent = '7 superfícies procedurais, água animada e faces laterais de terra/rocha. Arte antiga de tiles e FX elementais não incluídos.';
    p.appendChild(catalogNote);
    const act = row();
    btn(act, 'Desfazer', false, () => this.undoAction(), 'Ctrl+Z');
    const s = btn(act, this.dirty ? 'Salvar •' : 'Salvar', false, () => this.save());
    if (this.dirty) s.classList.add('dirty');
    const hint = document.createElement('div'); hint.className = 'muted tools-hint';
    hint.textContent = this.active ? 'Clique: aplica (cliques acumulam) · Arrastar: pinta · Segurar 0,5 s e arrastar: mover' : 'Escolha uma ferramenta para editar.';
    p.appendChild(hint);
  }
}
