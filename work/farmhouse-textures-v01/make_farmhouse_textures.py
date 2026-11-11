"""Farmlands house props: photographic texture sets (colour + OpenGL normal + roughness), 1024 x 1024, tileable.

Usage (one material per run, the PC is weak):
    python work/farmhouse-textures-v01/make_farmhouse_textures.py wood
    python work/farmhouse-textures-v01/make_farmhouse_textures.py sheet      # contact sheet of what exists
    python work/farmhouse-textures-v01/make_farmhouse_textures.py publish    # copy out/ to content/models/textures (never overwrites)

Everything is written to work/farmhouse-textures-v01/out/ first. Sources are photographs already in the project:
ambientCG (CC0), Poly Haven (CC0) and the user's own wt-* photographs.
"""
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / 'out'
DEST = ROOT / 'content' / 'models' / 'textures'
ACG = ROOT / 'content' / 'textures' / 'ambientcg'
PH = ROOT / 'content' / 'textures' / 'polyhaven'
MT = ROOT / 'content' / 'models' / 'textures'
S = 1024
ASH = np.array([0.60, 0.585, 0.56], np.float32)
MATERIALS = ['wood', 'dark-wood', 'iron', 'linen', 'straw', 'clay', 'herbs']


# ---------------------------------------------------------------- helpers
def load(path, mode='RGB', size=None):
    im = Image.open(path).convert(mode)
    if size:
        im = im.resize(size, Image.LANCZOS)
    return np.asarray(im, dtype=np.float32) / 255.0


def acg(name, kind, mode='RGB', size=None):
    return load(ACG / name / f'{name}_{kind}_2k.jpg', mode, size)


def lum(a):
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def blur(a, s):
    if a.ndim == 3:
        return np.stack([ndi.gaussian_filter(a[..., i], s, mode='wrap') for i in range(a.shape[2])], -1)
    return ndi.gaussian_filter(a, s, mode='wrap')


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def noise(seed, sigma, shape=(S, S)):
    """Tileable soft noise, 0..1 (2nd..98th percentile stretched)."""
    n = ndi.gaussian_filter(np.random.default_rng(seed).standard_normal(shape).astype(np.float32), sigma, mode='wrap')
    lo, hi = np.percentile(n, [2, 98])
    return np.clip((n - lo) / (hi - lo), 0, 1)


def normal_from_height(h, strength):
    """OpenGL tangent normal (+Y up) from a height field; gradients wrap, so a tileable height gives a tileable normal."""
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    n = np.stack([-dx * strength, dy * strength, np.ones_like(h)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def decode_normal(rgb, strength=1.0):
    n = rgb * 2 - 1
    n[..., :2] *= strength
    n[..., 2] = np.maximum(n[..., 2], 0.05)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def add_normals(a, b):
    """Detail-over-base blend (UDN)."""
    n = np.stack([a[..., 0] + b[..., 0], a[..., 1] + b[..., 1], a[..., 2]], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def make_tileable(a, seed, tw=40.0, amp=70.0, sigma=22):
    """Two-pass offset blend (x then y) with a ragged noise edge; the roll is exactly half the size."""
    h, w = a.shape[:2]
    x = np.abs(np.arange(w) - w / 2)[None, :]
    m = smooth((w / 4 + (noise(seed, sigma, (h, w)) - 0.5) * 2 * amp - x) / tw + 0.5)
    m = m[..., None] if a.ndim == 3 else m
    a = a * m + np.roll(a, w // 2, 1) * (1 - m)
    y = np.abs(np.arange(h) - h / 2)[:, None]
    m = smooth((h / 4 + (noise(seed + 1, sigma, (h, w)) - 0.5) * 2 * amp - y) / tw + 0.5)
    m = m[..., None] if a.ndim == 3 else m
    return a * m + np.roll(a, h // 2, 0) * (1 - m)


def grade(c, sat=1.0, mean=None, gamma=1.0, tint=(1, 1, 1)):
    g = lum(c)[..., None]
    c = g + (c - g) * sat
    if gamma != 1.0:
        g2 = lum(c)[..., None]
        c = c * (np.power(np.maximum(g2, 1e-4), gamma) / np.maximum(g2, 1e-4))
    c = c * np.array(tint, np.float32)
    if mean is not None:
        c = c * (mean / max(float(lum(c).mean()), 1e-4))
    return c


def ash_veil(c, r, height, amount, seed):
    """A thin veil of settled grey ash: patchy, a little heavier in the hollows. Returns colour, roughness."""
    hp = height - blur(height, 6)
    cav = smooth(0.5 - hp / (4 * hp.std() + 1e-6))
    m = (0.35 + 0.65 * noise(seed, 60)) * (0.45 + 0.55 * cav) * (0.7 + 0.3 * noise(seed + 7, 5))
    m = np.clip(m * amount, 0, 1)
    c = c * (1 - m[..., None]) + ASH * m[..., None]
    r = np.clip(r + 0.35 * m, 0, 1)
    return c, r


def to_img(a, mode='RGB'):
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), mode)


def save_set(name, colour, normal, rough):
    OUT.mkdir(exist_ok=True)
    to_img(colour).save(OUT / f'farmhouse-{name}-v01-color.jpg', quality=94, subsampling=0)
    to_img(normal * 0.5 + 0.5).save(OUT / f'farmhouse-{name}-v01-normal.jpg', quality=95, subsampling=0)
    to_img(rough, 'L').save(OUT / f'farmhouse-{name}-v01-roughness.jpg', quality=93)
    report(name, colour, normal, rough)


def seam_score(a):
    a = a.reshape(a.shape[0], a.shape[1], -1)
    lr = np.abs(a[:, 0] - a[:, -1]).mean() / (np.abs(a[:, 1:] - a[:, :-1]).mean() + 1e-9)
    tb = np.abs(a[0] - a[-1]).mean() / (np.abs(a[1:] - a[:-1]).mean() + 1e-9)
    return lr, tb


def report(name, colour, normal, rough):
    print(f'{name}: colour mean {np.round(colour.reshape(-1, 3).mean(0), 3)}, rough mean {rough.mean():.2f} '
          f'[{rough.min():.2f}..{rough.max():.2f}], normal z mean {normal[..., 2].mean():.3f}')
    for label, a in (('colour', colour), ('normal', normal), ('rough', rough)):
        lr, tb = seam_score(a)
        print(f'   seam {label}: left-right {lr:.2f}, top-bottom {tb:.2f}  (1.0 = same as any neighbouring pixels)')


# ---------------------------------------------------------------- 1 wood
def wood():
    """Farm furniture boards: ambientCG Planks037A, eight boards at the 2k scale, grain along U."""
    name = 'planks-037a'
    y0, y1, x0 = 122, 1142, 420           # eight whole boards (measured: board gaps every ~127.5 px from y=122)
    def crop(kind, mode):
        a = acg(name, kind, mode)[y0:y1, x0:x0 + S]
        im = to_img(a, mode).resize((S, S), Image.LANCZOS)
        return np.asarray(im, np.float32) / 255
    c, d, r = crop('color', 'RGB'), crop('displacement', 'L'), crop('roughness', 'L')
    n = decode_normal(crop('normalgl', 'RGB'))
    rng = np.random.default_rng(11)
    bounds = [round(i * S / 8) for i in range(9)]
    shifts = [130, 705, 372, 918, 560, 215, 808, 468]   # where each board's new butt joint falls
    groove = np.zeros((S, S), np.float32)
    xs = np.arange(S, dtype=np.float32)
    for i in range(8):
        a, b = bounds[i], bounds[i + 1]
        s = shifts[i]
        for arr in (c, d, r, n):
            arr[a:b] = np.roll(arr[a:b], s, axis=1)
        dist = np.minimum(np.abs(xs - s), S - np.abs(xs - s))
        wob = 1.0 + 0.5 * rng.random()
        groove[a + 2:b - 2] = np.exp(-(dist / (1.5 * wob)) ** 2)[None, :]
    groove = blur(groove, 0.6)
    c = c * (1 - 0.78 * groove[..., None])
    d = d - 0.25 * groove
    r = np.maximum(r, groove)
    n = add_normals(n, normal_from_height(-groove, 2.2) * np.array([1, 1, 0], np.float32))
    # grey-brown, dry: half the saturation, lifted, a cool-warm drift between boards
    c = c + 0.6 * (c - blur(c, 2.5))                     # the grain a little more marked, as on dry scrubbed boards
    c = grade(np.clip(c, 0, 1), sat=0.42, gamma=0.80, tint=(1.03, 1.0, 0.955), mean=0.335)
    drift = noise(21, 90)[..., None]
    c = c * (0.93 + 0.14 * drift)
    r = np.clip(0.62 + 0.38 * r, 0, 1)
    c, r = ash_veil(c, r, d, 0.30, 31)
    save_set('wood', c, n, r)


# ---------------------------------------------------------------- 2 dark wood
def dark_wood():
    """Smoke- and oil-darkened oak: ambientCG Wood060, whole tile, grain along U."""
    name = 'wood-060'
    c = acg(name, 'color', 'RGB', (S, S))
    d = acg(name, 'displacement', 'L', (S, S))
    r = acg(name, 'roughness', 'L', (S, S))
    n = decode_normal(acg(name, 'normalgl', 'RGB', (S, S)), 0.7)
    c = grade(c, sat=0.50, gamma=1.15, tint=(1.04, 0.97, 0.90), mean=0.125)
    # oil and handling polish the ridges; soot stays in the checks
    hp = d - blur(d, 5)
    ridge = smooth(0.5 + hp / (5 * hp.std()))
    c = c * (0.86 + 0.24 * ridge[..., None])
    r = np.clip(0.38 + 0.30 * r + 0.18 * (1 - ridge), 0, 1)
    c, r = ash_veil(c, r, d, 0.20, 41)
    save_set('dark-wood', c, n, r)


# ---------------------------------------------------------------- 3 iron
def hammer_height(seed, count=330):
    """Shallow overlapping hammer facets (relief only), tileable."""
    rng = np.random.default_rng(seed)
    # every blow leaves a shallow dish; the surface is the lower envelope of the dishes, with soft ridges between them
    h = np.full((S, S), 1.0, np.float32)
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    for _ in range(count):
        cx, cy = rng.random(2) * S
        rad = rng.uniform(34, 70)
        ex = rng.uniform(0.75, 1.35)
        dx = (xx - cx + S / 2) % S - S / 2
        dy = (yy - cy + S / 2) % S - S / 2
        h = np.minimum(h, rng.uniform(0.0, 0.35) + (dx / (rad * ex)) ** 2 + (dy / rad) ** 2)
    h = blur(h, 2.5)
    return (h - h.min()) / (h.max() - h.min())


def iron():
    """Hand-forged black iron: ambientCG Metal024 graded to scale-black, its own rust kept faint, hammer facets in the relief."""
    name = 'metal-024'
    c = acg(name, 'color', 'RGB', (S, S))
    r0 = acg(name, 'roughness', 'L', (S, S))
    n = decode_normal(acg(name, 'normalgl', 'RGB', (S, S)), 1.4)
    g = lum(c)
    sat = c.max(-1) - c.min(-1)
    rust = smooth((sat - 0.07) / 0.16) * smooth((c[..., 0] - c[..., 2] - 0.03) / 0.10)
    rust = np.clip(blur(rust, 0.8) * 1.2, 0, 1)
    detail = g - blur(g, 14)
    base = 0.085 + 0.9 * detail + 0.05 * (blur(g, 14) - g.mean())
    hh = hammer_height(53)
    crest = smooth((hh - 0.55) / 0.4)
    base = base * (0.90 + 0.30 * crest) + 0.012 * crest
    c2 = np.clip(base, 0.015, 1)[..., None] * np.array([0.98, 1.0, 1.05], np.float32)
    # fire scale: blue-grey patches in the colour, from the photograph's own low frequencies
    scale = smooth((blur(g, 5) - blur(g, 40)) / 0.03 + 0.5)[..., None]
    c2 = c2 * (1 - 0.18 * scale) + c2 * np.array([0.92, 0.98, 1.10], np.float32) * 1.25 * 0.18 * scale
    rust_col = np.array([0.30, 0.165, 0.09], np.float32) * (0.55 + 1.6 * np.clip(g, 0, 1))[..., None]
    patch = smooth((noise(57, 45) - 0.50) / 0.25)          # rust gathers in a few places, not everywhere
    rm = np.clip(rust * (0.35 + 0.65 * patch) * 1.1, 0, 1)[..., None]
    c2 = c2 * (1 - rm) + rust_col * rm
    r = np.clip(0.50 + 0.28 * r0 - 0.14 * crest + 0.35 * rm[..., 0] + 0.10 * noise(59, 3), 0, 1)
    n = add_normals(normal_from_height(hh, 5.0), n)
    c2, r = ash_veil(c2, r, hh, 0.16, 61)
    save_set('iron', c2, n, r)


# ---------------------------------------------------------------- 4 linen
def find_pitch(g, axis):
    """Thread pitch along an axis from the autocorrelation, refined on a far peak."""
    prof = g - blur(g, 8)
    f = np.fft.rfft(prof, axis=axis)
    ac = np.fft.irfft(np.abs(f) ** 2, axis=axis).mean(1 - axis)
    p = 6 + int(np.argmax(ac[6:30]))
    for m in (2, 4, 8, 16, 24):
        k0 = int(round(p * m))
        k = k0 - 3 + int(np.argmax(ac[k0 - 3:k0 + 4]))
        p = k / m
    return p


def linen():
    """Coarse raw linen / wool: the user's burlap photograph, cut to a whole number of threads, patched and stained."""
    src = load(MT / 'wt-burlap.jpg')
    src = src / np.maximum(blur(src, 45), 1e-3) * src.reshape(-1, 3).mean(0)      # no lighting falloff across the cloth
    g = lum(src)
    px, py = find_pitch(g, 1), find_pitch(g, 0)
    nx = int((src.shape[1] - 8) / px) // 4 * 4
    ny = int((src.shape[0] - 8) / py) // 4 * 4
    print(f'   thread pitch x {px:.2f} px, y {py:.2f} px -> {nx} x {ny} threads in the tile')
    box = (4, 4, 4 + nx * px, 4 + ny * py)
    tile = np.asarray(to_img(src).resize((S, S), Image.LANCZOS, box=box), np.float32) / 255
    tile = make_tileable(tile, 71, tw=30, amp=60)
    sharp = tile + 0.55 * (tile - blur(tile, 1.2))
    c = grade(sharp, sat=0.62, tint=(1.0, 0.995, 0.97), mean=0.47)
    height = lum(sharp)
    height = (height - height.min()) / (height.max() - height.min())
    # patches: the same cloth turned a quarter and sewn on, each a little different in tone
    rng = np.random.default_rng(73)
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    other = np.rot90(grade(sharp, sat=0.62, tint=(1.0, 0.995, 0.97), mean=0.47)).copy()
    other_h = np.rot90(height).copy()
    stitch = np.zeros((S, S), np.float32)
    hem_all = np.zeros((S, S), np.float32)
    occ = np.zeros((S, S), np.float32)
    for (cx, cy, w, h, ang, tone) in ((300, 330, 120, 86, 0.10, (0.84, 0.83, 0.80)),
                                      (760, 690, 78, 104, -0.16, (0.93, 0.89, 0.82)),
                                      (180, 820, 62, 54, 0.30, (0.80, 0.81, 0.82))):
        ca, sa = np.cos(ang), np.sin(ang)
        u = (xx - cx) * ca + (yy - cy) * sa
        v = -(xx - cx) * sa + (yy - cy) * ca
        wob = (noise(int(cx), 9) - 0.5) * 7
        dist = np.maximum(np.abs(u) - w, np.abs(v) - h) + wob     # < 0 inside the patch
        inside = smooth(-dist / 2.0 + 0.5)
        hem = np.exp(-((dist + 5) / 3.5) ** 2)                    # the turned-under edge stands proud
        sh = np.roll(np.roll(other, int(cx) * 3 % S, 1), int(cy) * 5 % S, 0) * np.array(tone, np.float32)
        c = c * (1 - inside[..., None]) + sh * inside[..., None]
        height = height * (1 - inside) + (np.roll(np.roll(other_h, int(cx) * 3 % S, 1), int(cy) * 5 % S, 0) + 0.25) * inside
        hem_all = np.maximum(hem_all, hem)
        # running stitches along the hem line (dist = -5)
        per = 2 * (w + h) * 2
        t = np.where(np.abs(np.abs(u) - w) < np.abs(np.abs(v) - h), v * np.sign(u), -u * np.sign(v))   # along-edge coordinate
        dash = smooth((np.cos(t * 2 * np.pi / 15.0 + rng.random() * 6) - 0.25) / 0.3)
        stitch = np.maximum(stitch, dash * np.exp(-((dist + 6) / 2.3) ** 2))
        occ = np.maximum(occ, np.exp(-((dist - 1.5) / 2.4) ** 2))     # the crease where the patch meets the cloth
    c = c * (1 + 0.05 * hem_all[..., None]) * (1 - 0.30 * occ[..., None])
    height = height + 0.30 * hem_all + 0.5 * stitch - 0.25 * occ
    thread = np.array([0.64, 0.60, 0.52], np.float32) * (0.8 + 0.4 * noise(75, 1.2))[..., None]
    c = c * (1 - 0.9 * stitch[..., None]) + thread * 0.9 * stitch[..., None]
    # stains: old damp rings and handling grime, soft and few
    s1 = smooth((noise(77, 80) - 0.55) / 0.45)
    s2 = smooth((noise(79, 30) - 0.80) / 0.20)
    c = c * (1 - (0.065 * s1 + 0.05 * s2)[..., None] * np.array([0.85, 1.0, 1.25], np.float32))
    r = np.clip(0.97 - 0.10 * smooth((height - 0.55) / 0.4), 0, 1)
    c, r = ash_veil(c, r, height, 0.14, 81)
    n = normal_from_height(blur(height, 0.7), 9.0)
    save_set('linen', c, n, r)


# ---------------------------------------------------------------- 5 straw (waits for its photograph)
def straw(src=None, name='straw'):
    """Woven straw / wicker: no photograph of a weave exists in the archives, so this one waits for the generated
    photograph (prompt in the hand-over note). When it arrives in Attachments/Farmhouse/, this derives the set from it."""
    cands = [ROOT / 'Attachments' / 'Farmhouse' / f'farmhouse-straw-v01-color.{e}' for e in ('jpg', 'png')]
    p = Path(src) if src else next((q for q in cands if q.exists()), None)
    if p is None:
        raise SystemExit('straw: falta a fotografia Attachments/Farmhouse/farmhouse-straw-v01-color.jpg (prompt na nota)')
    c = load(p, 'RGB', (S, S))
    lr, tb = seam_score(c)
    print(f'   source seam: left-right {lr:.2f}, top-bottom {tb:.2f}')
    if max(lr, tb) > 1.5:
        print('   the photograph does not tile by itself: blended (look at the weave across the blend before using it)')
        c = make_tileable(c, 121, tw=30, amp=50)
    g = lum(c)
    height = g - blur(g, 14)
    height = np.clip(0.5 + height / (6 * height.std() + 1e-6), 0, 1)
    r = np.clip(0.86 - 0.20 * smooth((height - 0.5) / 0.4), 0, 1)       # the crowns of the strands are rubbed smoother
    n = normal_from_height(blur(height, 0.8), 9.0)
    save_set(name, c, n, r)


# ---------------------------------------------------------------- 6 clay
def clay():
    """Rustic glazed stoneware: the project's terracotta body under a brown-green glaze, crazed along a Poly Haven
    cracked-mud network, chipped back to the body in a few places."""
    body = load(MT / 'terracotta-photo-v01.png', 'RGB', (S, S))
    body = make_tileable(body, 91)
    bg = lum(body)
    mud = load(PH / 'mud_cracked_dry_03' / 'mud_cracked_dry_03_disp_2k.jpg', 'L', (S, S))
    crack = np.clip((blur(mud, 5) - blur(mud, 0.7)) / 0.035, 0, 1)
    # crazing is a hairline: keep only the centre line of every crack of the photograph (ridge of the distance field)
    pad = 24
    mask = np.pad((blur(mud, 4) - blur(mud, 0.8)) > 0.010, pad, mode='wrap')
    dist = ndi.distance_transform_edt(mask)
    line = (mask & (dist >= ndi.maximum_filter(dist, size=3) - 0.35))[pad:-pad, pad:-pad].astype(np.float32)
    depth = np.clip(dist[pad:-pad, pad:-pad] / 3.0, 0.55, 1.0)                  # the wide cracks give the stronger lines
    craze = np.clip(blur(line * depth, 0.7) * 2.4, 0, 1)
    craze = craze * (0.55 + 0.45 * smooth((noise(93, 50) - 0.25) / 0.4))        # crazing comes in fields
    # glaze: the body's own mottling, softened, mapped onto a brown-green ramp
    mott = blur(bg, 3.5)
    mott = (mott - mott.mean()) / (mott.std() + 1e-6)
    pool = noise(95, 110)[..., None]                                             # where the glaze ran thick it is greener
    brown = np.array([0.285, 0.220, 0.125], np.float32)
    green = np.array([0.245, 0.245, 0.140], np.float32)
    glaze = (brown * (1 - pool) + green * pool) * (1 + 0.16 * mott[..., None])
    speck = np.clip((blur(bg, 0.8) - blur(bg, 3)) / 0.05, -1, 1)[..., None]     # iron specks through the glaze
    glaze = glaze * (1 + 0.045 * speck)
    glaze = glaze * (1 - 0.62 * craze[..., None])
    # chips: a few bites where the glaze is gone, more of them along the big cracks
    seedm = noise(97, 17) + 0.22 * blur(crack, 6) / max(float(blur(crack, 6).max()), 1e-6)
    chip = smooth((seedm - np.percentile(seedm, 98.5)) / 0.03)
    rim = np.clip(blur(chip, 2.0) * 4 * (1 - chip), 0, 1)
    raw = grade(body, sat=0.70, tint=(1.0, 0.97, 0.92), mean=0.30)
    c = glaze * (1 - chip[..., None]) + raw * chip[..., None]
    c = c * (1 + 0.22 * rim[..., None])                                         # thin glaze at the chip edge is paler
    height = 0.5 + 0.10 * (noise(99, 14) - 0.5) + 0.012 * speck[..., 0] - 0.05 * craze - 0.22 * chip
    height = height + chip * 0.06 * (bg - blur(bg, 3)) / 0.05
    r = 0.30 + 0.10 * noise(101, 4) + 0.25 * craze
    r = r * (1 - chip) + 0.92 * chip
    c, r = ash_veil(c, np.clip(r, 0, 1), height, 0.17, 103)
    n = normal_from_height(blur(height, 0.8), 22.0)
    save_set('clay', c, n, r)


# ---------------------------------------------------------------- 7 herbs
def herbs():
    """Dried herb bundles: Poly Haven forest_leaves_03 (a bed of dry overlapping leaves), dried to grey-green and straw."""
    d = PH / 'forest_leaves_03'
    c = load(d / 'forest_leaves_03_diff_2k.jpg', 'RGB', (S, S))
    arm = load(d / 'forest_leaves_03_arm_2k.jpg', 'RGB', (S, S))
    disp = load(d / 'forest_leaves_03_disp_2k.jpg', 'L', (S, S))
    n = decode_normal(load(d / 'forest_leaves_03_nor_gl_2k.jpg', 'RGB', (S, S)))
    g = lum(c)[..., None]
    greenness = smooth((c[..., 1] - c[..., 0] + 0.02) / 0.08)[..., None]        # the leaves that were still green
    mixn = smooth((noise(111, 16) - 0.35) / 0.3)[..., None]
    sage = np.array([0.50, 0.54, 0.40], np.float32)
    straw = np.array([0.70, 0.62, 0.42], np.float32)
    tint = sage * (1 - mixn) + straw * mixn
    tint = tint * (1 - greenness) + np.array([0.46, 0.56, 0.38], np.float32) * greenness
    flat = g / 0.5 * tint
    c2 = flat * 0.78 + c * 0.22 * (float(lum(flat).mean()) / float(g.mean()))
    c2 = grade(c2, gamma=0.92, mean=0.27)
    r = np.clip(0.72 + 0.26 * arm[..., 1], 0, 1)
    c2, r = ash_veil(c2, r, disp, 0.15, 113)
    save_set('herbs', c2, n, r)


# ---------------------------------------------------------------- sheets and publishing
def sheet():
    t, pad, lab = 256, 12, 34
    w = len(MATERIALS) * (2 * t + pad) + pad
    im = Image.new('RGB', (w, 2 * t + 2 * pad + lab), (18, 18, 18))
    dr = ImageDraw.Draw(im)
    font = ImageFont.load_default(size=20)
    for i, m in enumerate(MATERIALS):
        x = pad + i * (2 * t + pad)
        p = OUT / f'farmhouse-{m}-v01-color.jpg'
        if p.exists():
            tile = Image.open(p).convert('RGB').resize((t, t), Image.LANCZOS)
            for a in range(2):
                for b in range(2):
                    im.paste(tile, (x + a * t, pad + b * t))
            dr.text((x + 2, 2 * t + pad + 8), f'farmhouse-{m}-v01-color.jpg (2 x 2)', fill=(235, 235, 235), font=font)
        else:
            dr.rectangle((x, pad, x + 2 * t - 1, pad + 2 * t - 1), outline=(120, 120, 120))
            dr.text((x + 16, pad + t - 8), f'{m}: FALTANDO (ver o prompt na nota)', fill=(235, 190, 120), font=font)
            dr.text((x + 2, 2 * t + pad + 8), f'farmhouse-{m}-v01-color.jpg (falta)', fill=(235, 190, 120), font=font)
    im.save(HERE / 'contact-sheet.png')
    print('contact sheet', im.size)


def lit(name):
    """Check picture for one material: colour 2 x 2, the set lit from the upper left, normal, roughness."""
    c = load(OUT / f'farmhouse-{name}-v01-color.jpg')
    n = load(OUT / f'farmhouse-{name}-v01-normal.jpg') * 2 - 1
    r = load(OUT / f'farmhouse-{name}-v01-roughness.jpg', 'L')
    L = np.array([-0.5, 0.5, 0.72], np.float32)
    L /= np.linalg.norm(L)
    nl = np.clip((n * L).sum(-1), 0, 1)
    hv = L + np.array([0, 0, 1], np.float32)
    hv /= np.linalg.norm(hv)
    spec = np.power(np.clip((n * hv).sum(-1), 0, 1), 2 + 60 * (1 - r) ** 2) * (1 - r) * 0.5
    shaded = np.power(np.power(c, 2.2) * (0.25 + 1.0 * nl[..., None]) + spec[..., None], 1 / 2.2)
    half = lambda a: to_img(a).resize((512, 512), Image.LANCZOS)
    im = Image.new('RGB', (4 * 1024 + 30, 1024), (18, 18, 18))
    for a in range(2):
        for b in range(2):
            im.paste(half(c), (a * 512, b * 512))
            im.paste(half(shaded), (1034 + a * 512, b * 512))
    im.paste(to_img(n * 0.5 + 0.5), (2068, 0))
    im.paste(to_img(np.repeat(r[..., None], 3, -1)), (3102, 0))
    im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
    im.save(HERE / f'check-{name}.png')
    to_img(c[256:768, 256:768]).save(HERE / f'check-{name}-closeup.png')


def publish():
    for p in sorted(OUT.glob('farmhouse-*-v01-*.jpg')):
        q = DEST / p.name
        if q.exists():
            print('exists, not overwritten:', q.name)
            continue
        shutil.copyfile(p, q)
        print('published', q.name)


if __name__ == '__main__':
    what = sys.argv[1]
    funcs = {'wood': wood, 'dark-wood': dark_wood, 'iron': iron, 'linen': linen, 'clay': clay, 'herbs': herbs}
    if what in funcs:
        funcs[what]()
        lit(what)
    elif what == 'straw':
        straw(*sys.argv[2:4])
        lit(sys.argv[3] if len(sys.argv) > 3 else 'straw')
    elif what == 'sheet':
        sheet()
    elif what == 'publish':
        publish()
    else:
        raise SystemExit(f'unknown: {what}')
