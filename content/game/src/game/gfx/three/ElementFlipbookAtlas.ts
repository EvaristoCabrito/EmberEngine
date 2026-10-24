import * as THREE from "three";

export type FlipbookElement = "fire" | "frost" | "lightning" | "poison" | "arcane" | "holy" | "shadow" | "ember";
export type FlipbookLayer = "main" | "secondary" | "particles";
export type FlipbookVersion = 1 | 2 | 3;

const atlases = new Map<string, Promise<THREE.Texture>>();

/** Load each element/layer 4x4 atlas once; the element emitter chooses the renderer per version. */
export function loadElementFlipbook(element: FlipbookElement, layer: FlipbookLayer = "main", version: FlipbookVersion = 1): Promise<THREE.Texture> {
  const key = `${version}:${element}:${layer}`;
  const filename = version >= 2
    ? `${element}-v${version}-${layer}-flipbook-4x4.png`
    : layer === "main" ? `${element}-flipbook-4x4.png` : `${element}-${layer}-flipbook-4x4.png`;
  const cached = atlases.get(key);
  if (cached) return cached;
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
  return loading;
}
