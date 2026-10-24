import * as THREE from "three";

export type FlipbookElement = "fire" | "frost" | "lightning" | "poison" | "arcane" | "holy" | "shadow" | "ember";
export type FlipbookLayer = "main" | "secondary" | "particles";
export type FlipbookVersion = 1 | 2;

const atlases = new Map<string, Promise<THREE.Texture>>();

/** Cache one CPU-side image per atlas, but return an owned texture wrapper to every emitter.
 * Emitters can now dispose their GPU textures independently after each cast. */
export function loadElementFlipbook(element: FlipbookElement, layer: FlipbookLayer = "main", version: FlipbookVersion = 1): Promise<THREE.Texture> {
  const key = `${version}:${element}:${layer}`;
  const filename = version === 2
    ? `${element}-v2-${layer}-flipbook-4x4.png`
    : layer === "main" ? `${element}-flipbook-4x4.png` : `${element}-${layer}-flipbook-4x4.png`;
  const cached = atlases.get(key);
  if (cached) return cached.then((texture) => texture.clone());
  const loading = new THREE.TextureLoader().loadAsync(`/game/fx/${filename}`).then((texture) => {
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.generateMipmaps = true;
    texture.minFilter = THREE.LinearMipmapLinearFilter;
    texture.magFilter = THREE.LinearFilter;
    texture.wrapS = THREE.ClampToEdgeWrapping;
    texture.wrapT = THREE.ClampToEdgeWrapping;
    return texture;
  }).catch((error) => {
    atlases.delete(key);
    throw error;
  });
  atlases.set(key, loading);
  return loading.then((texture) => texture.clone());
}
