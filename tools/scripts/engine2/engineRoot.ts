import { resolve } from 'node:path';

// Local game data/runtime snapshot used by EmberEngine's import tools. This keeps map preparation
// independent from a sibling Engine2 checkout; callers may override it for controlled tooling runs.
export const engineRoot = resolve(process.env.EMBER_GAME_DATA_ROOT ?? 'content/game').replaceAll('\\', '/');
