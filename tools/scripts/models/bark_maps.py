"""The relief and roughness maps of a photographed bark tile, made from the photo itself (the user paints no maps).

  python -I tools/scripts/models/bark_maps.py <tile.png> <normal.png> <arm.png> [--strength S]

Height = the tile's luminance, the furrows dark and low, smoothed a little and wrapped at the borders (the tile repeats). Normal
(OpenGL, +Y up, like the Poly Haven maps it replaces) from its slopes; ARM = ambient occlusion from how far a texel sits under its
neighbourhood (R), roughness high on bark, higher in the furrows (G), metallic 0 (B).
"""

import argparse

import numpy as np
from PIL import Image


def blur(a, r):
    """Box blur of radius r, wrapping at the borders, three passes (close to a gaussian)."""
    for _ in range(3):
        for axis in (0, 1):
            acc = np.zeros_like(a)
            for d in range(-r, r + 1):
                acc += np.roll(a, d, axis=axis)
            a = acc / (2 * r + 1)
    return a


def main():
    p = argparse.ArgumentParser()
    p.add_argument("tile")
    p.add_argument("normal")
    p.add_argument("arm")
    p.add_argument("--strength", type=float, default=4.0, help="slope scale of the normal map")
    a = p.parse_args()

    rgb = np.asarray(Image.open(a.tile).convert("RGB"), dtype=np.float64) / 255.0
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
    height = blur(lum, 1)
    height = (height - height.min()) / max(height.max() - height.min(), 1e-6)

    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * a.strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * a.strength
    n = np.stack((-dx, dy, np.ones_like(height)), axis=-1)  # image rows run down: +Y (up) is minus the row slope
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    Image.fromarray(np.uint8(np.round((n * 0.5 + 0.5) * 255)), "RGB").save(a.normal)

    local = blur(height, 12)
    ao = np.clip(1.0 - np.maximum(local - height, 0.0) * 2.5, 0.35, 1.0)
    rough = np.clip(0.97 - height * 0.17, 0.75, 1.0)
    arm = np.stack((ao, rough, np.zeros_like(height)), axis=-1)
    Image.fromarray(np.uint8(np.round(arm * 255)), "RGB").save(a.arm)
    print(f"wrote {a.normal} and {a.arm}: ao {ao.mean():.2f} mean, roughness {rough.mean():.2f} mean")


if __name__ == "__main__":
    main()
