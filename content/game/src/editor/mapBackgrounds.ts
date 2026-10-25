import type { EmberMapDraft } from '../map/emberMap';

// Lazy glob: only the file names are used, so the /public/ paths are never requested
// (an eager glob made Vite warn once per backdrop on every page load).
const IMAGE_URLS = import.meta.glob('/public/game/map-backgrounds/*.{jpg,jpeg,png,webp}', { query: '?url', import: 'default' });

export interface MapBackground {
  id: string;
  label: string;
  file: string;
  kind: 'battle' | 'mission' | 'campaign';
  mapIds?: readonly string[];
}

/** Battle scenes copied from Ember's backdrop routing, followed by preserved briefing art. */
export const MAP_BACKGROUNDS: MapBackground[] = [
  { id: 'scene-watchtower-entry', label: 'Torre de Vigia · Pátio', file: 'watchtower-entry-panorama-v02.png', kind: 'battle', mapIds: ['watchtower-gate-floor'] },
  { id: 'scene-watchtower-dungeon', label: 'Torre de Vigia · Masmorra', file: 'watchtower-dungeon-bg.jpg', kind: 'battle', mapIds: ['watchtower-undercroft', 'watchtower-prison'] },
  { id: 'scene-watchtower-upper', label: 'Torre de Vigia · Andares superiores', file: 'watchtower-entry-panorama-v02.png', kind: 'battle', mapIds: ['watchtower-barracks', 'watchtower-command'] },
  { id: 'scene-watchtower-beacon', label: 'Torre de Vigia · Farol', file: 'watchtower-entry-panorama-v02.png', kind: 'battle', mapIds: ['watchtower-beacon'] },
  { id: 'scene-frozen-tundra', label: 'Travessia da Tundra Congelada', file: 'frozen-tundra-background.jpg', kind: 'battle', mapIds: ['frozen-tundra-crossing'] },
  { id: 'scene-profundezas', label: 'As Profundezas', file: 'profundezas-bg.jpg', kind: 'battle', mapIds: ['profundezas'] },
  { id: 'scene-thebridge', label: 'Ponte de Pedra', file: 'thebridge-bg-clean-v2.png', kind: 'battle', mapIds: ['thebridge'] },
  { id: 'scene-wisp-forest', label: 'Floresta dos Wisps', file: 'wisp-forest-bg.jpg', kind: 'battle', mapIds: ['wisp-forest', 'wisp-forest-2', 'wisp-forest-crossing-boss'] },
  { id: 'scene-wisp-crossing-panorama', label: 'Travessia da Floresta dos Wisps · Panorâmica', file: 'wisp-forest-crossing-panorama-v04-alpha.png', kind: 'battle', mapIds: ['wisp-forest-crossing'] },
  { id: 'scene-cemetery-ground', label: 'Cemitério dos Esquecidos', file: 'CemiteryBackground.jpg', kind: 'battle', mapIds: ['cemiterio-esquecidos', 'cemetery-ground-part-ii', 'cemetery-ground-deep-crypt-1'] },
  { id: 'scene-cemetery-deep-chamber', label: 'Cemitério · Câmara profunda', file: '2Cemitério dos Esquecidos — Câmara Profunda✕.jpg', kind: 'battle', mapIds: ['cemiterio-esquecidos-cripta-2', 'cemetery-ground-deep-crypt-2'] },
  { id: 'scene-inn', label: 'Estalagem do Osso Seco', file: 'INNbackground-v02.png', kind: 'battle', mapIds: ['estalagem'] },
  { id: 'scene-misty-cave', label: 'Profundezas Enevoadas', file: 'As Profundezas EnevoadasBackground2.jpg', kind: 'battle', mapIds: ['misty-cave-dungeon', 'deep-mist', 'misty-root-descent', 'misty-spore-basin', 'misty-blackwater-shelf', 'misty-hollow-earth', 'misty-sporehaven', 'misty-deep-mycelium'] },
  { id: 'scene-cemetery-crypt', label: 'Cemitério · Cripta', file: 'Andar 3Cemitério dos Esquecidos — Cripta✕.jpg', kind: 'battle', mapIds: ['cemiterio-esquecidos-cripta', 'cemetery-ground-deep-crypt-3'] },
  { id: 'scene-cemetery-mausoleum', label: 'Cemitério · Mausoléu', file: 'Andar 4Cemitério dos Esquecidos — Mausoléu✕.jpg', kind: 'battle', mapIds: ['cemiterio-esquecidos-mausoleu', 'cemetery-ground-deep-crypt-4'] },
  { id: 'scene-cemetery-submerged-ruins', label: 'Cemitério · Ruínas submersas', file: 'Andar 5Cemitério dos Esquecidos — Ruínas Submersas.jpg', kind: 'battle', mapIds: ['cemiterio-esquecidos-ruinas'] },
  { id: 'scene-random-1', label: 'Encontro aleatório 1', file: 'random-encounter-1-bg.jpg', kind: 'battle', mapIds: ['random-encounter-1'] },
  { id: 'scene-random-2', label: 'Encontro aleatório 2', file: 'random-encounter-2-bg.jpg', kind: 'battle', mapIds: ['random-encounter-2'] },
  { id: 'scene-random-4', label: 'Encontro aleatório 4', file: 'random-encounter-4-bg.jpg', kind: 'battle', mapIds: ['random-encounter-4'] },
  { id: 'scene-random-5', label: 'Encontro aleatório 5', file: 'random-encounter-5-bg.jpg', kind: 'battle', mapIds: ['random-encounter-5'] },
  { id: 'scene-random-8', label: 'Encontro aleatório 8', file: 'random-encounter-8-bg.jpg', kind: 'battle', mapIds: ['random-encounter-8'] },
  { id: 'scene-random-road', label: 'Encontros · Estrada', file: 'merchant-road-background-001.jpg', kind: 'battle', mapIds: ['random-encounter-11'] },
  { id: 'scene-random-snow-market', label: 'Encontros · Mercado de inverno', file: 'merchant-snow-market-background-001.jpg', kind: 'battle', mapIds: ['random-encounter-14'] },
  { id: 'scene-vau', label: 'O Vau', file: 'vau-1-bg-clean-v2.png', kind: 'battle', mapIds: ['vau'] },
  { id: 'scene-vau-encounter', label: 'Encontro · Vau raso', file: 'vau-bg.jpg', kind: 'battle', mapIds: ['random-encounter-6'] },
  { id: 'scene-aldeia', label: 'Aldeia · Batalha', file: 'aldeia-bg-clean-v2.png', kind: 'battle', mapIds: ['aldeia'] },
  { id: 'scene-bosque', label: 'Bosque · Batalha', file: 'bosque-bg-clean-v2.png', kind: 'battle', mapIds: ['bosque'] },

  // Keep the already imported briefing images available as separate, clearly named choices.
  { id: 'aldeia', label: 'Aldeia · Briefing', file: 'brief-aldeia.jpg', kind: 'mission' },
  { id: 'bosque', label: 'Bosque · Briefing', file: 'brief-bosque.jpg', kind: 'mission' },
  { id: 'colina', label: 'Colina · Briefing', file: 'brief-colina.jpg', kind: 'mission' },
  { id: 'cripta', label: 'Cripta · Briefing', file: 'brief-cripta.jpg', kind: 'mission' },
  { id: 'estalagem', label: 'Estalagem · Briefing', file: 'brief-estalagem.jpg', kind: 'mission' },
  { id: 'fortaleza', label: 'Fortaleza · Briefing', file: 'brief-fortaleza.jpg', kind: 'mission' },
  { id: 'muralha', label: 'Muralha · Briefing', file: 'brief-muralha.jpg', kind: 'mission' },
  { id: 'passagem', label: 'Passagem · Briefing', file: 'brief-passagem.jpg', kind: 'mission' },
  { id: 'portao', label: 'Portão · Briefing', file: 'brief-portao.jpg', kind: 'mission' },
  { id: 'templo', label: 'Templo · Briefing', file: 'brief-templo.jpg', kind: 'mission' },
  { id: 'vau', label: 'Vau · Briefing', file: 'brief-vau.jpg', kind: 'mission' },
  { id: 'vertente', label: 'Vertente · Briefing', file: 'brief-vertente.jpg', kind: 'mission' },
  { id: 'campaign-world-map', label: 'Mapa da campanha', file: 'world-map.jpg', kind: 'campaign' },
];

for (const path of Object.keys(IMAGE_URLS)) {
  const file = path.split('/').pop()!;
  if (!MAP_BACKGROUNDS.some(item => item.file === file)) MAP_BACKGROUNDS.push({ id: file, label: file, file, kind: 'battle' });
}

const BY_ID = new Map(MAP_BACKGROUNDS.map((item) => [item.id, item]));
const BY_MAP_ID = new Map(MAP_BACKGROUNDS.flatMap((item) => (item.mapIds ?? []).map((id) => [id, item] as const)));

/** Automatically select the exact battle backdrop assigned to a map in Ember. */
export function automaticMapBackground(draft: EmberMapDraft): MapBackground | null {
  return BY_MAP_ID.get(draft.id) ?? null;
}

export function resolvedMapBackground(draft: EmberMapDraft): MapBackground | null {
  if (typeof draft.backgroundImage === 'string') {
    return draft.backgroundImage ? BY_ID.get(draft.backgroundImage) ?? null : null;
  }
  // nothing picked by hand: the map's own Ember backdrop (wisp forest crossing → wisp forest, ...)
  return automaticMapBackground(draft);
}

export function selectedMapBackground(draft: EmberMapDraft): string {
  return typeof draft.backgroundImage === 'string' ? (draft.backgroundImage || 'none') : 'auto';
}

export function mapBackgroundUrl(item: MapBackground): string {
  return `/game/map-backgrounds/${encodeURIComponent(item.file)}`;
}
