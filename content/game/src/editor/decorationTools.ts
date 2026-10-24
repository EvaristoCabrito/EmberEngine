import {
  DECORATIONS,
  EMBER_CLASSIC_DECOR_IDS,
  EMBER_CLASSIC_EXTRA_DECOR_IDS,
  decorationImage,
  placedFootprint,
} from '../ember/data';
import type { Board } from '../map/board';
import type { EmberDecoration, TerrainId } from '../map/emberMap';

export type DecorationEditMode = 'off' | 'place' | 'erase' | 'rotate';

/** EmberClassic's 2D prop catalog, kept separate from Engine2-only decorations and VFX. */
const CATALOG_IDS = [...EMBER_CLASSIC_DECOR_IDS, ...EMBER_CLASSIC_EXTRA_DECOR_IDS]
  .filter((id) => !!DECORATIONS[id])
  .sort((a, b) => DECORATIONS[a]!.name.localeCompare(DECORATIONS[b]!.name));

export class DecorationTools {
  mode: DecorationEditMode = 'off';
  selectedId = CATALOG_IDS[0] ?? '';
  private readonly modeButtons = new Map<DecorationEditMode, HTMLButtonElement>();
  private readonly itemButtons = new Map<string, HTMLButtonElement>();

  constructor(private readonly panel: HTMLElement, private readonly modeChanged: () => void) {
    this.render();
  }

  get active(): boolean { return this.mode !== 'off'; }

  setMode(mode: DecorationEditMode): void {
    this.mode = mode;
    for (const [key, button] of this.modeButtons) button.classList.toggle('on', key === mode);
    this.modeChanged();
  }

  /** Place, erase, or turn one EmberClassic decoration. Returns whether the draft changed. */
  apply(board: Board, x: number, y: number): boolean {
    if (this.mode === 'off') return false;
    const draft = board.draft;
    const decorations = (draft.decorations ??= []) as EmberDecoration[];
    const hit = decorations.find((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));

    if (this.mode === 'rotate') {
      if (!hit) return false;
      hit.rot = ((hit.rot ?? 0) + 1) % 6;
      return true;
    }

    if (hit) {
      const hitDef = DECORATIONS[hit.id];
      if (hitDef?.tile) {
        const floor: TerrainId = draft.tiles.includes('nave') ? 'nave' : 'plains';
        for (const f of placedFootprint(hit)) {
          const c = hit.x + f.dx, r = hit.y + f.dy;
          const i = c >= 0 && r >= 0 && c < draft.cols && r < draft.rows ? r * draft.cols + c : -1;
          if (i >= 0 && draft.tiles[i] === hitDef.tile) draft.tiles[i] = floor;
        }
      }
      draft.decorations = decorations.filter((p) => p !== hit);
      return true;
    }

    if (this.mode === 'erase') return false;
    const def = DECORATIONS[this.selectedId];
    if (!def) return false;
    const candidate: EmberDecoration = { id: this.selectedId, x, y };
    const cells = placedFootprint(candidate);
    const covered = new Set<string>();
    for (const placed of decorations) for (const f of placedFootprint(placed)) covered.add(`${placed.x + f.dx},${placed.y + f.dy}`);
    if (cells.some((f) => covered.has(`${x + f.dx},${y + f.dy}`))) return false;

    if (def.tile) {
      for (const f of cells) {
        const c = x + f.dx, r = y + f.dy;
        const i = c >= 0 && r >= 0 && c < draft.cols && r < draft.rows ? r * draft.cols + c : -1;
        if (i >= 0) draft.tiles[i] = def.tile;
      }
    }
    draft.decorations = [...decorations, candidate];
    return true;
  }

  private render(): void {
    this.panel.innerHTML = '';
    const summary = document.createElement('summary');
    summary.textContent = `Decorações EmberClassic · ${CATALOG_IDS.length}`;
    this.panel.appendChild(summary);

    const modes = document.createElement('div');
    modes.className = 'tools-row decor-modes';
    const modeLabels: [DecorationEditMode, string][] = [
      ['off', 'Ver'], ['place', 'Colocar'], ['erase', 'Apagar'], ['rotate', 'Girar'],
    ];
    for (const [mode, label] of modeLabels) {
      const button = document.createElement('button');
      button.type = 'button'; button.textContent = label; button.classList.toggle('on', mode === this.mode);
      button.title = mode === 'place' ? 'Clique num hex vazio para colocar; clique numa decoração para removê-la.'
        : mode === 'erase' ? 'Clique em qualquer hex ocupado pela decoração para removê-la.'
          : mode === 'rotate' ? 'Clique numa decoração para girá-la 60°.' : 'Desativa a edição de decorações.';
      button.addEventListener('click', () => this.setMode(mode));
      this.modeButtons.set(mode, button); modes.appendChild(button);
    }
    this.panel.appendChild(modes);

    const search = document.createElement('input');
    search.type = 'search'; search.placeholder = 'Buscar decoração…'; search.className = 'decor-search';
    this.panel.appendChild(search);
    const list = document.createElement('div'); list.className = 'decor-list';
    this.itemButtons.clear();
    for (const id of CATALOG_IDS) {
      const def = DECORATIONS[id]!;
      const button = document.createElement('button');
      button.type = 'button'; button.className = 'decor-choice'; button.dataset.id = id;
      button.title = `${def.name} · ${def.footprint.length} hex${def.footprint.length === 1 ? '' : 'es'}`;
      button.classList.toggle('on', this.selectedId === id);
      const image = document.createElement('img'); image.src = decorationImage(id); image.alt = ''; image.loading = 'lazy';
      const label = document.createElement('span'); label.textContent = def.name;
      button.append(image, label);
      button.addEventListener('click', () => {
        this.selectedId = id;
        for (const [choiceId, choice] of this.itemButtons) choice.classList.toggle('on', choiceId === id);
      });
      this.itemButtons.set(id, button); list.appendChild(button);
    }
    search.addEventListener('input', () => {
      const query = search.value.trim().toLocaleLowerCase();
      for (const [id, button] of this.itemButtons) {
        button.hidden = !`${id} ${DECORATIONS[id]!.name}`.toLocaleLowerCase().includes(query);
      }
    });
    this.panel.appendChild(list);
    const hint = document.createElement('div'); hint.className = 'muted tools-hint';
    hint.textContent = 'A área ocupada e o terreno-base seguem a definição do objeto.';
    this.panel.appendChild(hint);
  }
}
