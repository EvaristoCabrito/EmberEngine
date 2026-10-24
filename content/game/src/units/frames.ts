// Full-resolution sprite frames, prepared offline (tools/prepare-sprites.mjs), loaded on demand.
// Quality is never reduced. Video memory is protected instead:
//  - frames decode off the main thread (createImageBitmap) and upload a few per frame (no spikes),
//  - a budget tracks sprite texture memory; least-recently-used poses are released and reloaded later.

import * as THREE from 'three';
import prepared from './prepared.json';
import type { PoseSrc } from './catalog';
import { maxAnisotropy } from '../render/gpu';

export interface PoseFrames {
  key: string;
  textures: THREE.Texture[];
  n: number;
  /** original frame size (Ember's size rules are written against it) */
  natW: number;
  natH: number;
  /** where the prepared (trimmed) frame sits inside the original frame, in pixels */
  crop: { x: number; y: number; w: number; h: number };
  bytes: number;
  lastUsed: number;
  /** centerBody poses: per frame, how far (pixels of the prepared frame) the body sits right of
   * where it is in frame 1 — measured from the opaque legs and torso, not the weapon. */
  bodyShift?: number[];
  /** released by the budget; actors reload it instead of drawing it */
  evicted?: boolean;
}

type Prepared = Record<string, { n: number; natW: number; natH: number; x: number; y: number; w: number; h: number }>;
const PREPARED = prepared as Prepared;

/** Sprite share of a 4 GB card: leaves room for terrain, shadows, post-processing and effects. */
export const SPRITE_BUDGET_BYTES = 1.25 * 1024 ** 3;
const UPLOADS_PER_FRAME = 3;

const loading = new Map<string, Promise<PoseFrames>>();
const live = new Map<string, PoseFrames>();
const uploadQueue: THREE.Texture[] = [];
let renderer: THREE.WebGLRenderer | null = null;
let anisotropy = 8;
let usedBytes = 0;

export function initFrames(r: THREE.WebGLRenderer): void {
  renderer = r;
  anisotropy = maxAnisotropy(r);
}

/** Call once per rendered frame: uploads a few queued textures so a new pose never hitches. */
export function pumpUploads(): void {
  for (let i = 0; i < UPLOADS_PER_FRAME && uploadQueue.length; i++) renderer?.initTexture(uploadQueue.shift()!);
}

export function spriteMemory(): { usedMB: number; budgetMB: number; poses: number } {
  return { usedMB: Math.round(usedBytes / 1048576), budgetMB: Math.round(SPRITE_BUDGET_BYTES / 1048576), poses: live.size };
}

function evictFor(incoming: number, keep: Set<string>): void {
  if (usedBytes + incoming <= SPRITE_BUDGET_BYTES) return;
  // a pose drawn in the last 2 s is on screen: never released (it would re-upload unbudgeted)
  const now = performance.now();
  const order = [...live.values()].filter(p => !keep.has(p.key) && now - p.lastUsed > 2000).sort((a, b) => a.lastUsed - b.lastUsed);
  for (const p of order) {
    if (usedBytes + incoming <= SPRITE_BUDGET_BYTES) break;
    p.evicted = true;
    for (const t of p.textures) t.dispose();
    live.delete(p.key); loading.delete(p.key);
    usedBytes -= p.bytes;
  }
}

async function bitmap(url: string): Promise<ImageBitmap> {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`missing frame ${url}`);
  // premultiplied: fully see-through pixels carry no colour, so leftover red/green under the
  // matte can't bleed into the figure's edge as a coloured border (unit cards blend to match)
  return createImageBitmap(await res.blob(), { imageOrientation: 'flipY', premultiplyAlpha: 'premultiply', colorSpaceConversion: 'none' });
}

/** Body centre per frame: median x of opaque pixels in the bottom quarter of the frame (the
 * legs — arms, a swung weapon and a flaring cape sit higher and would skew it). `perFrame`
 * (centerBody sheets) follows each frame's drift relative to frame 1; otherwise the whole pose
 * gets one fixed shift, its median body centre onto `axis` (the x, prepared-frame pixels, that
 * stands on the hex centre) — per-frame centring of every pose made units slide non-stop.
 * Bitmaps are flipped (flipY), so the bottom is the top rows here. */
function measureBodyShift(bitmaps: ImageBitmap[], axis: number, perFrame: boolean): number[] {
  const centre = (bm: ImageBitmap): number => {
    const c = new OffscreenCanvas(bm.width, bm.height), g = c.getContext('2d', { willReadFrequently: true })!;
    g.drawImage(bm, 0, 0);
    const rows = Math.floor(bm.height * 0.25), d = g.getImageData(0, 0, bm.width, rows).data;
    const xs: number[] = [];
    for (let y = 0; y < rows; y += 2) for (let x = 0; x < bm.width; x++) if (d[(y * bm.width + x) * 4 + 3] > 100) xs.push(x);
    if (!xs.length) return bm.width / 2;
    xs.sort((a, b) => a - b);
    return xs[xs.length >> 1];
  };
  const centres = bitmaps.map(centre);
  if (perFrame) return centres.map(x => x - centres[0]);
  const fixed = [...centres].sort((a, b) => a - b)[centres.length >> 1] - axis;
  return centres.map(() => fixed);
}

export function loadPose(src: PoseSrc, keep: Set<string> = new Set()): Promise<PoseFrames> {
  const key = `${src.dir}|${src.prefix}`;
  const have = live.get(key);
  if (have) { have.lastUsed = performance.now(); return Promise.resolve(have); }
  let p = loading.get(key);
  if (!p) {
    p = (async () => {
      const info = PREPARED[key];
      const base = info ? `/game/sprites-e2/${src.dir}/${src.prefix}` : `/game/sprites/${src.dir}/${src.prefix}`;
      const bitmaps = await Promise.all(Array.from({ length: src.n }, (_, i) => bitmap(src.still ? `${base}.png` : `${base}${i + 1}.png`)));
      const w = bitmaps[0].width, h = bitmaps[0].height;
      // every pose stands its legs on the hex centre (art is rarely centred in its own canvas:
      // Malrec/Conjurer sat 16 px right); one shift per pose, so the animation plays as drawn
      const bodyShift = src.still ? undefined : measureBodyShift(bitmaps, (info?.natW ?? w) / 2 - (info?.x ?? 0), !!src.centerBody);
      const bytes = Math.round(w * h * 4 * 1.34) * src.n; // RGBA + mip chain
      evictFor(bytes, new Set([...keep, key]));
      const textures = bitmaps.map(bm => {
        const t = new THREE.Texture(bm);
        t.flipY = false;
        t.colorSpace = THREE.SRGBColorSpace;
        t.anisotropy = anisotropy;
        t.minFilter = THREE.LinearMipmapLinearFilter;
        t.magFilter = THREE.LinearFilter;
        t.needsUpdate = true;
        uploadQueue.push(t);
        return t;
      });
      const pose: PoseFrames = {
        key, textures, n: src.n,
        natW: info?.natW ?? w, natH: info?.natH ?? h,
        crop: info ? { x: info.x, y: info.y, w: info.w, h: info.h } : { x: 0, y: 0, w, h },
        bytes, lastUsed: performance.now(), bodyShift,
      };
      live.set(key, pose);
      usedBytes += bytes;
      return pose;
    })();
    loading.set(key, p);
    p.catch(() => loading.delete(key));
  }
  return p;
}
