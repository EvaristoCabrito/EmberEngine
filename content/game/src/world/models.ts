// 3D model registry: every placeable model with its real-world size in Engine2 units
// (human = 2.65 units ≈ 1.8 m, so 1 unit ≈ 0.68 m). Model files are never edited; each model is
// scaled uniformly on load so its height matches, then sits with its base centred on the origin.

import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

export interface ModelDef {
  file: string;
  /** Target height in world units (uniform scale; proportions are never changed). */
  height: number;
}

export const MODELS: Record<string, ModelDef> = {
  // Ember's tavern props, sized to human scale
  'tavern-barrel': { file: 'props/tavern-barrel.glb', height: 1.3 },      // ≈ 0.9 m
  'tavern-table': { file: 'props/tavern-table.glb', height: 2.2 },        // ≈ 1.5 m: twice Ember's 75 cm (user's call)
  'tavern-chair': { file: 'props/tavern-chair.glb', height: 1.45 },       // ≈ 1 m to the top of the back
  'tavern-table-small-v01': { file: 'props/tavern-table-small-v01.glb', height: 1.12 },
  'tavern-table-messhall-v01': { file: 'props/tavern-table-messhall-v01.glb', height: 1.15 },
  'tavern-chair-v04': { file: 'props/tavern-chair-v04.glb', height: 1.47 },
  'tavern-candlestick': { file: 'props/tavern-candlestick.glb', height: 0.4 }, // ≈ 27 cm
  'tavern-mug': { file: 'props/tavern-mug.glb', height: 0.15 },           // ≈ 10 cm
  'watchtower-signal-brazier-v01': { file: 'props/watchtower-signal-brazier-v01.glb', height: 2.43 },
  'watchtower-arms-rack-v01': { file: 'props/watchtower-arms-rack-v01.glb', height: 2.43 },
  'watchtower-command-table-v01': { file: 'props/watchtower-command-table-v01.glb', height: 1.52 },
  'watchtower-signal-brazier-v02': { file: 'props/watchtower-signal-brazier-v02.glb', height: 1.52 },
  'watchtower-arms-rack-v02': { file: 'props/watchtower-arms-rack-v02.glb', height: 2.43 },
  'watchtower-command-table-v02': { file: 'props/watchtower-command-table-v02.glb', height: 1.2 },
  // Ember's trees and grey outcrop, at Ember's own size (hex radii: trees 4.8, outcrop 2.3)
  'broadleaf': { file: 'trees/broadleaf.glb', height: 4.8 },
  'mature-tree-v04-photo': { file: 'forest/mature-tree-v04-photo.glb', height: 8.692 },
  'ember-great-branching-bough': { file: 'nature/great-branching-bough-v01.glb', height: 6.043 },
  'ember-great-branching-bough-leafy': { file: 'nature/great-branching-bough-leafy-v01.glb', height: 6.510 },
  'ember-colossal-dead-oak': { file: 'nature/colossal-dead-oak-v01.glb', height: 6.346 },
  'ember-colossal-dead-oak-leafy': { file: 'nature/colossal-dead-oak-leafy-v01.glb', height: 6.580 },
  'ember-amanita-muscaria-v01': { file: 'mines/amanita-muscaria-v01.glb', height: 0.725 },
  'snowy-pine': { file: 'trees/snowy-pine.glb', height: 4.8 },
  'dead-oak': { file: 'trees/dead-oak.glb', height: 4.8 },
  'dead-oak-v02': { file: 'trees/dead-oak-v02.glb', height: 4.8 },
  'dead-oak-v03-leafy': { file: 'trees/dead-oak-v03-leafy.glb', height: 4.8 },
  'dead-snag': { file: 'trees/dead-snag.glb', height: 4.8 },
  'dead-snag-v02': { file: 'trees/dead-snag-v02.glb', height: 4.8 },
  'twisted-stump': { file: 'trees/twisted-stump.glb', height: 4.8 },
  'twisted-stump-v02': { file: 'trees/twisted-stump-v02.glb', height: 4.8 },
  // Wisp Forest Crossing's versioned Revisited trees and grounded props.
  'wisp-crossing-ancient-dead-oak-v02': { file: 'wisp-crossing-trees/ancient-dead-oak-v02.glb', height: 11.155 },
  'wisp-crossing-weeping-willow-v02': { file: 'wisp-crossing-trees/weeping-willow-v02.glb', height: 11.074 },
  'wisp-crossing-lightning-split-v02': { file: 'wisp-crossing-trees/lightning-split-v02.glb', height: 9.857 },
  'wisp-crossing-hollow-sentinel-v02': { file: 'wisp-crossing-trees/hollow-sentinel-v02.glb', height: 10.272 },
  'wisp-revisited-weeping-willow-v02': { file: 'wisp-forest-revisited/weeping-willow-v02.glb', height: 10.025 },
  'wisp-revisited-giant-dead-oak-v02': { file: 'wisp-forest-revisited/giant-dead-oak-v02.glb', height: 11.54 },
  'wisp-revisited-hollow-snag-v02': { file: 'wisp-forest-revisited/hollow-snag-v02.glb', height: 8.005 },
  'wisp-revisited-uprooted-leaning-v02': { file: 'wisp-forest-revisited/uprooted-leaning-v02.glb', height: 9.382 },
  'wisp-revisited-lightning-split-v02': { file: 'wisp-forest-revisited/lightning-split-v02.glb', height: 9.899 },
  'wisp-revisited-spiral-twisted-v02': { file: 'wisp-forest-revisited/spiral-twisted-v02.glb', height: 9.807 },
  'wisp-revisited-tall-burnt-pine-v02': { file: 'wisp-forest-revisited/tall-burnt-pine-v02.glb', height: 9.592 },
  'wisp-revisited-broken-crown-v02': { file: 'wisp-forest-revisited/broken-crown-v02.glb', height: 9.013 },
  'wisp-revisited-root-arch-v02': { file: 'wisp-forest-revisited/root-arch-v02.glb', height: 8.577 },
  'wisp-revisited-broken-stump': { file: 'wisp-forest-revisited/broken-stump-v01.glb', height: 1.454 },
  'wisp-revisited-rotted-hollow-log': { file: 'wisp-forest-revisited/rotted-hollow-log-v01.glb', height: 1.06 },
  'wisp-revisited-roots-and-fungi': { file: 'wisp-forest-revisited/roots-and-fungi-v01.glb', height: 0.387 },
  'grey-outcrop': { file: 'rocks/grey-outcrop.glb', height: 2.3 },
  'vau-large-stone-v01': { file: 'rocks/vau-large-stone-v01.glb', height: 2.586 },
  'vau-red-outcrop-v01': { file: 'rocks/vau-red-outcrop-v01.glb', height: 2.606 },
};

const loader = new GLTFLoader();
const sources = new Map<string, Promise<THREE.Object3D>>();

/** Load once, scale to its real size, then hand out clones that share geometry and materials. */
export async function loadModel(name: keyof typeof MODELS | string): Promise<THREE.Object3D> {
  const def = MODELS[name];
  if (!def) throw new Error(`unknown model ${name}`);
  let src = sources.get(name);
  if (!src) {
    src = loader.loadAsync(`/game/models/${def.file}`).then(g => {
      const inner = g.scene;
      inner.traverse(o => { if ((o as THREE.Mesh).isMesh) { o.castShadow = true; o.receiveShadow = true; } });
      const box = new THREE.Box3().setFromObject(inner), size = box.getSize(new THREE.Vector3());
      const k = def.height / size.y;
      inner.scale.setScalar(k);
      inner.position.set(-((box.min.x + box.max.x) / 2) * k, -box.min.y * k, -((box.min.z + box.max.z) / 2) * k);
      const root = new THREE.Group();
      root.name = name;
      root.add(inner);
      return root;
    });
    sources.set(name, src);
  }
  return (await src).clone(true);
}

