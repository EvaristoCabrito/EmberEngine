import type { EmberMapDraft } from '../map/emberMap';
import { MAP_BACKGROUNDS, mapBackgroundUrl, selectedMapBackground } from './mapBackgrounds';

export type BackgroundChoice = 'auto' | 'none' | string;

/** Per-map selector for imported Ember background images. */
export class BackgroundTools {
  private readonly choices = new Map<string, HTMLButtonElement>();
  private readonly modeButtons = new Map<'auto' | 'none', HTMLButtonElement>();
  private draft: EmberMapDraft | null = null;
  private selected = 'none';

  constructor(private readonly panel: HTMLDetailsElement, private readonly choose: (choice: BackgroundChoice) => void) {
    this.render();
  }

  setDraft(draft: EmberMapDraft): void {
    this.draft = draft;
    this.selected = selectedMapBackground(draft);
    this.syncSelection();
  }

  private render(): void {
    this.panel.innerHTML = '';
    const summary = document.createElement('summary');
    const battleCount = MAP_BACKGROUNDS.filter((item) => item.kind === 'battle').length;
    const briefingCount = MAP_BACKGROUNDS.filter((item) => item.kind === 'mission').length;
    summary.textContent = `Fundos de batalha · ${battleCount} · briefings ${briefingCount}`;
    this.panel.appendChild(summary);

    const modes = document.createElement('div'); modes.className = 'tools-row background-modes';
    for (const [id, label] of [['none', 'Vazio preto']] as const) {
      const button = document.createElement('button'); button.type = 'button'; button.textContent = label;
      button.addEventListener('click', () => this.choose(id));
      this.modeButtons.set(id, button); modes.appendChild(button);
    }
    this.panel.appendChild(modes);

    const catalog = document.createElement('div'); catalog.className = 'background-catalog';
    for (const item of MAP_BACKGROUNDS) {
      const button = document.createElement('button'); button.type = 'button'; button.className = 'background-choice';
      const typeLabel = item.kind === 'battle'
        ? 'Fundo de batalha'
        : item.kind === 'campaign' ? 'Mapa de campanha' : 'Imagem de briefing';
      button.title = `${item.label} · ${typeLabel}`;
      const image = document.createElement('img'); image.src = mapBackgroundUrl(item); image.alt = ''; image.loading = 'lazy';
      const caption = document.createElement('span'); caption.textContent = item.label;
      button.append(image, caption);
      button.addEventListener('click', () => this.choose(item.id));
      this.choices.set(item.id, button); catalog.appendChild(button);
    }
    this.panel.appendChild(catalog);
    const hint = document.createElement('div'); hint.className = 'muted tools-hint background-hint';
    hint.textContent = 'Os fundos de batalha são aplicados automaticamente aos mapas correspondentes do Ember. Escolhas manuais ficam salvas no mapa; as imagens de briefing aparecem separadas no catálogo.';
    this.panel.appendChild(hint);
  }

  private syncSelection(): void {
    for (const [id, button] of this.modeButtons) button.classList.toggle('on', id === this.selected);
    for (const [id, button] of this.choices) button.classList.toggle('on', id === this.selected);
    const item = MAP_BACKGROUNDS.find((candidate) => candidate.id === this.selected);
    this.panel.querySelector('summary')!.textContent = item
      ? `Fundo do mapa · ${item.label}`
      : `Fundos de mapa · ${MAP_BACKGROUNDS.length}`;
  }
}
