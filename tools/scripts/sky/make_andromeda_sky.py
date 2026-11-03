"""Céu de Andrômeda, star look 005 (2026-11-03; the user: "eu quero um céu de vista vindo de Andrômeda e que seja RICO").

The night sky of a world inside the Andromeda galaxy, made from real images (looks/stars/README.md, look 005):

  galaxy-v01-N.png   ESA/Gaia/DPAC (CC BY-SA 3.0 IGO, acknowledgement A. Moitinho): Gaia EDR3's flux map of the whole sky,
                     16000 x 8000, galactic coordinates (l = 0 in the middle, l growing to the left). A spiral galaxy seen
                     from inside. What only the Earth's sky has is painted out with the sky beside it at the same latitude:
                     the two Magellanic clouds and the smudges of Andromeda and Triangulum (they stand elsewhere in this sky).
                     The shader reads it mirrored (no pattern of the Earth's sky survives). Written as four tiles (N = 0..3:
                     top-left, top-right, bottom-left, bottom-right), each 8064 x 4064 with a 32-px gutter.
  milkyway-v01.png   ESA/Gaia/DPAC, Stefan Payne-Wardenaar (CC BY-SA 3.0 IGO): "The best Milky Way map, by Gaia", face-on;
                     the galaxy cut round, its black lifted to zero, 1024 x 1024. The shader tilts it (seen from Andromeda).
  triangulum-v01.png ESO (CC BY 4.0): M33 by the VST (eso1424a, 4K); the sky's black lifted to zero, faded out round the
                     galaxy, 1024 wide (the stars in front of it kept).
  ../andromeda-stars.json  the world's own bright stars: the real sky's 225 brightest (content/sky/stars.json) with their
                     magnitudes and colours kept and their places dealt anew, evenly over the sphere (seed 31): the same
                     brightness mix as a real sky, constellations of their own.

Run: python -I tools/scripts/sky/make_andromeda_sky.py build/sky/source content/sky/andromeda
"""

import json
import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

Image.MAX_IMAGE_PIXELS = None


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# --- the galaxy map --------------------------------------------------------------------------------------------------------
# (name, l, b, radius in degrees): painted out with the sky `shift` degrees of longitude away, same latitude.
PAINT_OUT = [
    ("LMC", 280.47, -32.89, 9.0, -22.0),
    ("SMC", 302.80, -44.30, 5.0, 14.0),
    ("M31", 121.17, -21.57, 2.6, 10.0),
    ("M33", 133.61, -31.33, 1.4, -8.0),
]


def galaxy_map(src: Path, out: Path):
    im = np.asarray(Image.open(src / "Gaia_EDR3_flux_cartesian_16k.png").convert("RGB")).copy()
    h, w, _ = im.shape
    for name, l, b, radius, shift in PAINT_OUT:
        ll = l if l <= 180 else l - 360
        cx, cy = (0.5 - ll / 360) * w, (0.5 - b / 180) * h
        span_y = radius / 180 * h * 1.05
        span_x = span_y / max(math.cos(math.radians(abs(b) + radius)), 0.2)
        x0, x1 = int(cx - span_x), int(cx + span_x) + 1
        y0, y1 = int(cy - span_y), int(cy + span_y) + 1
        ys, xs = np.mgrid[y0:y1, x0:x1]
        lon = np.radians((0.5 - (xs + 0.5) / w) * 360)
        lat = np.radians((0.5 - (ys + 0.5) / h) * 180)
        lc, bc = math.radians(ll), math.radians(b)
        cos_d = np.sin(lat) * math.sin(bc) + np.cos(lat) * math.cos(bc) * np.cos(lon - lc)
        dist = np.degrees(np.arccos(np.clip(cos_d, -1, 1)))
        mask = (1.0 - smoothstep(radius * 0.7, radius, dist))[..., None]
        dx = int(round(-shift / 360 * w))
        target = im[y0:y1, x0:x1].astype(np.float32)
        source = im[y0:y1, (np.arange(x0, x1) + dx) % w].astype(np.float32)
        im[y0:y1, x0:x1] = np.clip(target * (1 - mask) + source * mask + 0.5, 0, 255).astype(np.uint8)
        print(f"galaxy: {name} painted out (radius {radius} deg, from {shift:+} deg of longitude)")
    # Four tiles of 8000 x 4000 (+ a 32-px gutter from the neighbours, wrapping round in longitude): one mip of the whole map
    # (128 MB of BC7) is more than the upload ring holds (UploadQueue: 64 MB); the gutter keeps the seams out of the filter.
    g = TILE_GUTTER
    for row in range(2):
        for col in range(2):
            xs = np.arange(col * w // 2 - g, (col + 1) * w // 2 + g) % w
            ys = np.clip(np.arange(row * h // 2 - g, (row + 1) * h // 2 + g), 0, h - 1)
            tile = im[ys][:, xs]
            path = out / f"galaxy-v01-{row * 2 + col}.png"
            Image.fromarray(tile).save(path, compress_level=6)
            print("galaxy:", path, tile.shape[1], "x", tile.shape[0])


TILE_GUTTER = 32  # atmosphere.hlsli andromedaSky's kGalaxyGutter


# --- the Milky Way, seen from outside --------------------------------------------------------------------------------------
def milky_way(src: Path, out: Path):
    im = Image.open(src / "The_best_Milky_Way_map_by_Gaia.jpg").convert("RGB")
    im = im.resize((1024, 1024), Image.LANCZOS)
    a = np.asarray(im, dtype=np.float32) / 255.0
    # The black: the darkest corner's level (the image's own background), lifted to zero.
    corners = np.concatenate([a[:96, :96].reshape(-1, 3), a[:96, -96:].reshape(-1, 3), a[-96:, :96].reshape(-1, 3), a[-96:, -96:].reshape(-1, 3)])
    black = np.percentile(corners, 50, axis=0)
    a = np.clip((a - black) / (1 - black), 0, 1)
    ys, xs = np.mgrid[0:1024, 0:1024]
    r = np.hypot(xs + 0.5 - 512, ys + 0.5 - 512) / 512
    a *= (1.0 - smoothstep(0.82, 0.99, r))[..., None]
    Image.fromarray((a * 255 + 0.5).astype(np.uint8)).save(out / "milkyway-v01.png")
    print("milky way: black", np.round(black * 255, 1), "->", out / "milkyway-v01.png")


# --- Triangulum (M33) -----------------------------------------------------------------------------------------------------
def triangulum(src: Path, out: Path):
    im = Image.open(src / "eso1424a-4k.tif").convert("RGB")
    w = 2048
    im = im.resize((w, round(im.height * w / im.width / 4) * 4), Image.LANCZOS)
    a = np.asarray(im, dtype=np.float32) / 255.0
    h = a.shape[0]
    # The stars in front of the galaxy stay: the galaxy is itself a sea of resolved stars (every filter tried ate its arms:
    # median, top-hat, compact-peak removal, 2026-11-03), and stars of the near galaxy in front of a far one belong in any sky.
    filled = a
    # The sky's black: the frame's border, lifted to zero.
    border = np.concatenate([filled[:64].reshape(-1, 3), filled[-64:].reshape(-1, 3), filled[:, :64].reshape(-1, 3), filled[:, -64:].reshape(-1, 3)])
    black = np.percentile(border, 40, axis=0)
    filled = np.clip((filled - black) / (1 - black), 0, 1)
    # Faded out round the galaxy: an ellipse along its disc (centre, half axes and tilt read off the image).
    ys, xs = np.mgrid[0:h, 0:w]
    cx, cy, tilt = 0.505 * w, 0.475 * h, math.radians(-25.0)  # the disc rises to the right (image y down)
    u = (xs - cx) * math.cos(tilt) + (ys - cy) * math.sin(tilt)
    v = -(xs - cx) * math.sin(tilt) + (ys - cy) * math.cos(tilt)
    r = np.hypot(u / (0.48 * w), v / (0.30 * w))
    filled *= (1.0 - smoothstep(0.7, 1.0, r))[..., None]
    out_im = Image.fromarray((filled * 255 + 0.5).astype(np.uint8)).resize((1024, round(h * 1024 / w / 4) * 4), Image.LANCZOS)
    out_im.save(out / "triangulum-v01.png")
    print("triangulum: black", np.round(black * 255, 1), "->", out / "triangulum-v01.png", out_im.size)


# --- the world's own bright stars ------------------------------------------------------------------------------------------
def stars(out: Path):
    real = json.load(open("content/sky/stars.json", encoding="utf-8"))
    rng = random.Random(31)
    world = []
    for i, s in enumerate(real["stars"]):
        ra = rng.uniform(0.0, 24.0)
        dec = math.degrees(math.asin(rng.uniform(-1.0, 1.0)))
        world.append([f"A{i + 1:03d}", round(ra, 4), round(dec, 3), s[3], s[4]])
    doc = {
        "_doc": "Céu de Andrômeda (star look 005): the world's own bright stars. The real sky's 225 brightest (content/sky/stars.json) "
                "with their magnitudes and B-V colours kept and their places dealt anew, evenly over the sphere (seed 31), by "
                "tools/scripts/sky/make_andromeda_sky.py. [name, ra hours, dec degrees, magnitude, B-V]; the galaxy's frame is "
                "StarSky's galactic pole / centre.",
        "latitude": real["latitude"],
        "lstAtMidnight": real["lstAtMidnight"],
        "clouds": [],
        "stars": world,
    }
    path = out.parent / "andromeda-stars.json"
    json.dump(doc, open(path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("stars:", len(world), "->", path)


if __name__ == "__main__":
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    stars(out)
    milky_way(src, out)
    triangulum(src, out)
    galaxy_map(src, out)
