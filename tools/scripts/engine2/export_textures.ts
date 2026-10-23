import { engineRoot } from './engineRoot';
// Exports Engine2's procedural ground textures (src/render/textures.ts) exactly as Engine2 generates them,
// for checking EmberEngine's port pixel for pixel. Engine2 paints them on a browser canvas; here a minimal
// stand-in canvas keeps the ImageData bytes. Nothing under content\game is written.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_textures.ts -- <out dir>
// Writes <out>/<layer>.rgba (raw RGBA8) for the nine prototype layers and smooth-plains (1024 x 1024).

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const E2 = engineRoot;
const [outDir] = process.argv.slice(2).filter((a) => a !== '--');
if (!outDir) throw new Error('usage: export_textures.ts -- <out dir>');

class Context2D {
  image: { width: number; height: number; data: Uint8ClampedArray } | null = null;
  createImageData(w: number, h: number) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; }
  putImageData(img: { width: number; height: number; data: Uint8ClampedArray }) { this.image = img; }
  getImageData() { return this.image!; }
}
(globalThis as Record<string, unknown>).document = {
  createElement: () => {
    const ctx = new Context2D();
    return { width: 0, height: 0, getContext: () => ctx, toDataURL: () => '' };
  },
  // three's TextureLoader asks for an <img> for the plains049 maps; it never loads here.
  createElementNS: () => ({ style: {}, addEventListener: () => {}, removeEventListener: () => {}, src: '' }),
};
// Engine2 starts loading its photo sets in the background; they need the browser and are not used here.
process.on('unhandledRejection', (reason) => { if (!String((reason as Error)?.stack ?? reason).includes('groundSets')) console.error('unhandled:', reason); });
globalThis.fetch = (async () => ({ ok: false, status: 404, headers: new Map() })) as unknown as typeof fetch;

const textures = await import(`${E2}/src/render/textures.ts`);
const renderer = { capabilities: { getMaxAnisotropy: () => 1 } };
const ground = textures.groundTextures(renderer);
mkdirSync(outDir, { recursive: true });
const layers: string[] = [...textures.PROTO_LAYERS];
const proto = ground.proto.image as { width: number; height: number; data: Uint8Array };
const layerBytes = proto.width * proto.height * 4;
layers.forEach((name, k) => writeFileSync(join(outDir, `${name}.rgba`), proto.data.subarray(k * layerBytes, (k + 1) * layerBytes)));
const smooth = await textures.smoothPlainsCanvas();
writeFileSync(join(outDir, 'smooth-plains.rgba'), (smooth.getContext('2d') as unknown as Context2D).getImageData().data);
console.log(`exported ${layers.length} prototype layers (${proto.width}x${proto.height}) and smooth-plains`);
