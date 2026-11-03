"""A faixa de Andrômeda (star look 005, 2026-11-03; the user: "estamos em Andrômeda", "Faixa da Andrômeda").

The night sky of a world inside the Andromeda galaxy, seen from its disc: computed, not photographed (no photograph of it exists,
and the photographs of a band from inside a spiral are of our own galaxy, which the user rejected). Andromeda's own structure
from its published measurements, in kiloparsecs (1 kpc = 3260 light years):

  observer      in the disc's mid-plane, 12 kpc from the centre (Andromeda's disc is ~1.5 x ours: the Sun is at 8)
  bulge         Andromeda's big bright bulge (a third of its light; ours is far smaller): a flattened (0.7) Sérsic-like glow
                of old, gold stars, ~1 kpc
  disc          exponential, scale length 5.3 kpc, scale height 0.45 kpc, old stars, warm white, out to ~30 kpc
  the ring      Andromeda's signature: the ring of young blue stars and dust at 10 kpc (and a fainter one at 15 kpc, an inner
                dust ring at 5); spiral arms are weak in Andromeda (two, pitch ~9 deg), kept faint
  dust          a thin layer (0.12 kpc), mostly in the rings, clumpy: it splits the band down its middle and cuts lanes
  halo          Andromeda's wide faint stellar halo
Clumps of stars and dust come from a 3D noise in kpc (star clouds 0.3-3 kpc), fixed (seed in the hash).

Each pixel of the panorama (galactic longitude l, latitude b of this sky; l = 0 the centre, in the middle; l grows to the right,
as atmosphere.hlsli reads it) integrates the light along its ray, dimmed by the dust in front. Written as an 8-bit image whose
values are the light (sRGB-encoded, cooked as BC7 sRGB: the shader reads linear light), scaled so the band reads 0.2-0.5 and the
bulge's core clips at 1.

Run: python -I tools/scripts/sky/make_andromeda_band.py content/sky/andromeda/band-v01.png [width]   (8192 x 4096: ~1-3 min)
"""

import math
import sys

import numpy as np
from numba import njit, prange
from PIL import Image

R0 = 12.0          # the observer's distance from the centre (kpc)
Z0 = 0.015         # just above the mid-plane (inside the dust layer)


@njit(cache=True, inline="always")
def _hash(ix, iy, iz):
    h = (ix * 374761393 + iy * 668265263 + iz * 2147483647 + 1013904223) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF) / 16777215.0


@njit(cache=True, inline="always")
def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


@njit(cache=True)
def noise3(x, y, z):
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = _smooth(x - ix), _smooth(y - iy), _smooth(z - iz)
    ix, iy, iz = int(ix), int(iy), int(iz)
    c000 = _hash(ix, iy, iz); c100 = _hash(ix + 1, iy, iz)
    c010 = _hash(ix, iy + 1, iz); c110 = _hash(ix + 1, iy + 1, iz)
    c001 = _hash(ix, iy, iz + 1); c101 = _hash(ix + 1, iy, iz + 1)
    c011 = _hash(ix, iy + 1, iz + 1); c111 = _hash(ix + 1, iy + 1, iz + 1)
    x00 = c000 + (c100 - c000) * fx; x10 = c010 + (c110 - c010) * fx
    x01 = c001 + (c101 - c001) * fx; x11 = c011 + (c111 - c011) * fx
    y0 = x00 + (x10 - x00) * fy; y1 = x01 + (x11 - x01) * fy
    return y0 + (y1 - y0) * fz


@njit(cache=True)
def fbm(x, y, z, octaves):
    s, a, f, norm = 0.0, 0.5, 1.0, 0.0
    for _ in range(octaves):
        s += a * noise3(x * f, y * f, z * f)
        norm += a
        a *= 0.5
        f *= 2.03
    return s / norm


@njit(cache=True)
def sample(x, y, z):
    """Emission (old disc + bulge + halo, young ring) and dust opacity per kpc at a point."""
    r = math.sqrt(x * x + y * y)
    az = abs(z)
    # Spiral arms (two, trailing, pitch 9 deg), faint: a factor along each arm's angle.
    phi = math.atan2(y, x)
    arm = 0.0
    if r > 2.0:
        wind = math.log(r / 4.0) / math.tan(math.radians(9.0))
        arm = 0.5 + 0.5 * math.cos(2.0 * (phi - wind))
        arm = arm ** 4
    # Clumps in kpc: star clouds (0.2-3 kpc) and dust clouds (finer), with contrast: the band's star clouds and gaps.
    clump = fbm(x * 1.4 + 11.0, y * 1.4 - 7.0, z * 3.0, 6)
    clump = min(max((clump - 0.3) * 2.2, 0.0), 1.6)
    dust_clump = fbm(x * 2.2 - 3.0, y * 2.2 + 5.0, z * 6.0 + 2.0, 6)
    outer = 1.0 / (1.0 + math.exp((r - 26.0) / 1.5))  # the disc fades past ~26 kpc
    # Old stars: the disc and the bulge (flattened, compact: gone by ~4 kpc), the halo.
    disc = math.exp(-r / 5.3) * math.exp(-az / 0.35) * outer * (0.35 + 0.9 * clump) * (1.0 + 0.4 * arm)
    m = math.sqrt(r * r + (z / 0.7) ** 2)
    bulge = 30.0 * math.exp(-m / 0.5)
    halo = 0.002 * math.exp(-math.sqrt(r * r + z * z) / 12.0)
    # Young stars: the 10-kpc ring (and a fainter 15-kpc one), thin, clumpy, on the arms.
    ring = (math.exp(-((r - 10.0) / 1.3) ** 2) + 0.45 * math.exp(-((r - 15.0) / 1.6) ** 2)) * math.exp(-az / 0.18)
    young = 0.35 * ring * max(clump - 0.35, 0.0) * 3.0 * (0.6 + 0.8 * arm)
    # Dust: thin, mostly in the rings (10, 15 kpc) and an inner ring at 5, clumpy.
    dust_r = 0.25 * math.exp(-r / 7.0) + 1.6 * math.exp(-((r - 10.0) / 1.2) ** 2) + 0.6 * math.exp(-((r - 15.0) / 1.5) ** 2) + \
             0.7 * math.exp(-((r - 5.0) / 0.8) ** 2)
    dust = 3.2 * dust_r * math.exp(-az / 0.12) * outer * max(dust_clump - 0.3, 0.0) * 2.8
    return disc + bulge + halo, young, dust


@njit(cache=True, parallel=True)
def render(width, height):
    out = np.zeros((height, width, 3), dtype=np.float32)
    old_col = (1.0, 0.95, 0.88)
    bulge_tint = (1.0, 0.86, 0.66)
    young_col = (0.72, 0.84, 1.10)
    redden = (0.92, 1.0, 1.08)  # dust dims blue a little more than red (kept mild: no rust)
    for py in prange(height):
        b = (0.5 - (py + 0.5) / height) * math.pi
        cb, sb = math.cos(b), math.sin(b)
        for px in range(width):
            l = ((px + 0.5) / width - 0.5) * 2.0 * math.pi
            # The direction: l = 0 towards the centre (-x from the observer), l > 0 towards -y, b towards +z.
            dx = cb * (-math.cos(l))
            dy = cb * (-math.sin(l))
            dz = sb
            tr = 1.0
            tg = 1.0
            tb = 1.0
            ir = 0.0
            ig = 0.0
            ib = 0.0
            s = 0.02
            while s < 45.0:
                ds = max(0.02, s * 0.04)
                t = s + 0.5 * ds
                x = R0 + dx * t
                y = dy * t
                z = Z0 + dz * t
                if abs(z) > 4.0 and z * dz > 0.0 and x * x + y * y + z * z > 36.0:
                    break  # out of the disc for good and past the bulge
                old, young, dust = sample(x, y, z)
                rr = math.sqrt(x * x + y * y + z * z)
                wb = min(1.0, max(0.0, 1.5 - rr / 2.0))  # near the centre the old light takes the bulge's gold
                cr = old * (old_col[0] * (1.0 - wb) + bulge_tint[0] * wb) + young * young_col[0]
                cg = old * (old_col[1] * (1.0 - wb) + bulge_tint[1] * wb) + young * young_col[1]
                cbl = old * (old_col[2] * (1.0 - wb) + bulge_tint[2] * wb) + young * young_col[2]
                ir += cr * tr * ds
                ig += cg * tg * ds
                ib += cbl * tb * ds
                tau = dust * ds
                tr *= math.exp(-tau * redden[0])
                tg *= math.exp(-tau * redden[1])
                tb *= math.exp(-tau * redden[2])
                if tg < 1e-4:
                    break
                s += ds
            out[py, px, 0] = ir
            out[py, px, 1] = ig
            out[py, px, 2] = ib
    return out


if __name__ == "__main__":
    path = sys.argv[1]
    width = int(sys.argv[2]) if len(sys.argv) > 2 else 8192
    height = width // 2
    light = render(width, height)
    lum = light @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    # The scale: the band 30 deg from the centre (b = 0) at 0.12 of white in light (~0.38 displayed); the bulge clips.
    row = height // 2
    col = int(width * (0.5 + 30.0 / 360.0))
    ref = float(np.median(lum[row - 2:row + 3, col - 40:col + 40]))
    light = light / max(ref, 1e-9) * 0.12
    lum = lum / max(ref, 1e-9) * 0.12
    # The eye's black: the disc's faint glow far from the band (what the sky holds at |b| > 50 deg) is below what a night eye
    # sees as light: taken off the brightness (colour kept), the rest given a little contrast. A night sky, not a grey one.
    floor = float(np.percentile(lum[: height // 8], 60))
    lifted = np.clip(lum - floor, 0.0, None)
    target = 0.12 * np.power(lifted / 0.12, 1.25)
    light = light * (target / np.maximum(lum, 1e-9))[..., None]
    lum = target
    print(f"band: floor {floor:.4f}; centre {float(lum[row, width // 2]):.2f}, band at 30 deg {float(lum[row, col]):.3f}, "
          f"pole {float(lum[2, width // 2]):.4f}, anticentre {float(lum[row, 4]):.3f}")
    encoded = np.where(light <= 0.0031308, light * 12.92, 1.055 * np.power(np.clip(light, 0, 1), 1 / 2.4) - 0.055)
    Image.fromarray((np.clip(encoded, 0, 1) * 255 + 0.5).astype(np.uint8)).save(path)
    print("band:", path, width, "x", height)
