"""Mapa-múndi e Submundo: as máscaras do look 2D (shaders/worldmap.hlsl), extraídas das próprias pinturas.

O usuário não pinta máscaras (2026-10-29: "eu nunca pintaria nada"): cada uma é feita aqui a partir da cor da pintura e de
regiões conhecidas (em % da imagem), à metade da resolução da pintura. Uma versão nova nunca sobrescreve a anterior (SUFFIX).

    python -I tools/scripts/worldmap/make_masks.py [content/worldmap/masks]

Superfície (world-map-v03.jpg): lights (janelas e tochas nas construções, cor quente), height (relevo: morro central, montanhas,
ravina), water, mist (baixadas). Submundo (underworld-atlas-v03.png): fungal (R = idade / força), plants, windows, ember (R =
força), heat, water, spores.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

SUFFIX = "-v01"


def load(path, scale=0.5):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    im = im.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32) / 255.0
    return a


def luma(a):
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def blur(m, r):
    return np.asarray(Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(r))).astype(np.float32) / 255.0


def save_l(m, path):
    Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8), "L").save(path)
    print("wrote", path)


def save_rgb(m, path):
    Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8), "RGB").save(path)
    print("wrote", path)


def rect(h, w, x0, y0, x1, y1):
    m = np.zeros((h, w), np.float32)
    m[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)] = 1
    return m


def ellipse(h, w, cx, cy, rx, ry, soft=0.35):
    ys, xs = np.mgrid[0:h, 0:w]
    d = np.sqrt(((xs / w - cx) / rx) ** 2 + ((ys / h - cy) / ry) ** 2)
    return np.clip((1.0 - d) / soft, 0, 1)


def paper(a, bright=0.80, satMax=0.10, grow=0):
    """A borda de pergaminho da pintura: claro e sem cor. 1 = papel (nada acontece ali). `grow` alarga a borda (pixels)."""
    l = luma(a)
    sat = a.max(-1) - a.min(-1)
    p = ((l > bright) & (sat < satMax)).astype(np.float32)
    if grow > 0:
        p = np.clip(blur(p, grow) * 3.0, 0, 1)
    return np.clip(blur(p, 3) * 1.5, 0, 1)


def sparse_points(mask, min_dist, seed, count):
    """Até `count` pontos dentro de `mask`, nunca a menos de min_dist um do outro (pixels)."""
    rng = np.random.default_rng(seed)
    ys, xs = np.nonzero(mask > 0.5)
    if len(ys) == 0:
        return []
    order = rng.permutation(len(ys))
    chosen = []
    for k in order:
        y, x = int(ys[k]), int(xs[k])
        if all((y - cy) ** 2 + (x - cx) ** 2 >= min_dist ** 2 for cy, cx in chosen):
            chosen.append((y, x))
            if len(chosen) >= count:
                break
    return chosen


def dot(img, y, x, r, color):
    h, w = img.shape[:2]
    y0, y1, x0, x1 = max(0, y - r), min(h, y + r + 1), max(0, x - r), min(w, x + r + 1)
    ys, xs = np.mgrid[y0:y1, x0:x1]
    d = np.sqrt((ys - y) ** 2 + (xs - x) ** 2)
    k = np.clip(1.0 - (d - r + 1.0), 0, 1)[..., None]
    img[y0:y1, x0:x1] = np.maximum(img[y0:y1, x0:x1], k * np.asarray(color, np.float32))


# --- superfície -----------------------------------------------------------------------------------------------------------
def surface(a, out):
    h, w = a.shape[:2]
    l = luma(a)
    pap = paper(a)
    land = 1.0 - pap
    # Construções (x0, y0, x1, y1 em %, luzes por região, tamanho): aldeia NW, castelo, solar NE, capela central, torre SW, portal SW
    buildings = [
        (0.13, 0.21, 0.34, 0.33, 16, 3),   # a aldeia (só as casas)
        (0.44, 0.02, 0.60, 0.17, 14, 3),   # o castelo
        (0.73, 0.19, 0.88, 0.32, 8, 3),    # o solar
        (0.49, 0.53, 0.53, 0.56, 2, 3),    # a capela do cruzamento
        (0.23, 0.48, 0.26, 0.54, 3, 3),    # a torre sobre a ponte
        (0.10, 0.74, 0.14, 0.80, 1, 3),    # o portal do túnel
        (0.14, 0.35, 0.21, 0.41, 4, 3),    # a quinta murada
    ]
    lights = np.zeros((h, w, 3), np.float32)
    # Só sobre a construção: o ponto cai onde a vizinhança (9 x 9) é escura e densa como um telhado / muro, nunca num ramo solto.
    dense = blur(((l < 0.45) & (l > 0.05)).astype(np.float32), 4)
    for i, (x0, y0, x1, y1, n, r) in enumerate(buildings):
        region = rect(h, w, x0, y0, x1, y1) * ((l > 0.10) & (l < 0.42) & (dense > 0.75)).astype(np.float32) * land
        for y, x in sparse_points(region, max(6, int(w * 0.006)), 11 + i, n):
            warm = (1.0, 0.72 + 0.1 * ((x * 7 + y) % 3) / 3.0, 0.35)
            dot(lights, y, x, r, warm)
    save_rgb(lights, f"{out}/surface-lights{SUFFIX}.png")

    # Relevo: base plana, o morro central, as montanhas do norte com o castelo, a ravina SW baixa, a floresta SE ondulada.
    height = np.full((h, w), 0.40, np.float32)
    height += 0.45 * ellipse(h, w, 0.52, 0.40, 0.24, 0.15, 0.6)
    mountains = rect(h, w, 0.0, 0.0, 1.0, 0.20) * (1.0 - l) * land
    height += 0.5 * blur(mountains, 6)
    height -= 0.35 * ellipse(h, w, 0.20, 0.80, 0.18, 0.20, 0.5)   # a ravina
    height -= 0.12 * ellipse(h, w, 0.45, 0.78, 0.12, 0.06, 0.6)   # o pântano
    fine = blur((1.0 - l) * land, 2) - blur((1.0 - l) * land, 12)
    height += 0.10 * fine
    height = blur(height, 3) * land + 0.4 * pap
    save_l(height, f"{out}/surface-height{SUFFIX}.png")

    # Água: o azul da pintura onde há água (os lagos NW / NE, o pântano, o rio SE, o regato da ravina); os penhascos cinza-azulados
    # da ravina ficam de fora pelas regiões.
    blueish = np.clip((a[..., 2] - a[..., 0] - 0.04) * 8.0, 0, 1) * np.clip((0.75 - l) / 0.3, 0, 1)
    green = np.clip((a[..., 1] - a[..., 2] - 0.02) * 10.0, 0, 1)
    where = rect(h, w, 0.08, 0.13, 0.26, 0.21) + rect(h, w, 0.84, 0.11, 1.0, 0.20) + rect(h, w, 0.34, 0.72, 0.52, 0.86)
    where += rect(h, w, 0.58, 0.76, 0.80, 1.0) + rect(h, w, 0.02, 0.84, 0.34, 1.0)
    water = blueish * (1.0 - green) * land * np.clip(where, 0, 1)
    water = blur(water, 2)
    save_l(np.clip(water * 1.4, 0, 1), f"{out}/surface-water{SUFFIX}.png")

    # Névoa: as baixadas (o pântano, a ravina, as margens do lago) e, leve, as planícies claras; nunca sobre o papel.
    mist = 0.9 * ellipse(h, w, 0.45, 0.78, 0.17, 0.10, 0.9)
    mist = np.maximum(mist, 0.8 * ellipse(h, w, 0.20, 0.80, 0.21, 0.23, 0.9))
    mist = np.maximum(mist, 0.6 * ellipse(h, w, 0.17, 0.17, 0.10, 0.06, 0.9))
    plains = 0.35 * np.clip((l - 0.55) / 0.25, 0, 1) * (1.0 - ellipse(h, w, 0.52, 0.40, 0.26, 0.17, 0.3))
    mist = np.maximum(mist, plains * (1.0 - paper(a, grow=20)))
    mist = blur(mist * land, 8) * land
    save_l(mist, f"{out}/surface-mist{SUFFIX}.png")


# --- Submundo ---------------------------------------------------------------------------------------------------------------
def underworld(a, out):
    h, w = a.shape[:2]
    l = luma(a)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    sat = a.max(-1) - a.min(-1)
    # O pergaminho rasgado em volta do atlas é bege e meio escuro: papel = claro e pouco saturado, alargado; e a terra = onde a
    # pintura é escura / colorida em volta (a borda do pergaminho, quente, nunca vira ember).
    pap = paper(a, bright=0.62, satMax=0.18, grow=25)
    content = ((l < 0.62) | (sat > 0.22)).astype(np.float32)
    land = np.clip(blur(content, 12) * 1.6 - 0.3, 0, 1) * (1.0 - pap)
    land *= 1.0 - rect(h, w, 0.0, 0.0, 1.0, 0.07)  # o céu da superfície no alto do atlas
    # O brilho fúngico: teal / ciano claro (os cogumelos e as casas neles), fora do papel; a força / idade = o brilho local mais a
    # densidade do aglomerado (os gigantes do NW acendem primeiro).
    teal = np.clip(((g + b) * 0.5 - r - 0.06) * 6.0, 0, 1) * np.clip((l - 0.40) / 0.25, 0, 1) * land
    density = blur(teal, 25)
    density = density / max(float(density.max()), 1e-4)
    fungal = np.clip(teal * (0.45 + 0.55 * density), 0, 1)
    save_l(fungal, f"{out}/underworld-fungal{SUFFIX}.png")

    # As plantas que se alimentam: a vegetação verde-escura das Marchas em volta dos cogumelos.
    plants = np.clip((g - np.maximum(r, b) + 0.02) * 10.0, 0, 1) * np.clip((0.62 - l) / 0.25, 0, 1) * np.clip((l - 0.10) / 0.1, 0, 1)
    marches = np.clip(blur(rect(h, w, 0.04, 0.40, 0.56, 0.66), 20) * 1.2, 0, 1)  # as Marchas Fúngicas Pálidas
    near = np.clip(blur(teal, 18) * 4.0, 0, 1)  # e o pé dos cogumelos
    plants = blur(plants, 2) * land * (1.0 - teal) * np.clip(marches + near, 0, 1)
    save_l(np.clip(plants * 2.0, 0, 1), f"{out}/underworld-plants{SUFFIX}.png")

    # As janelas nas casas dos cogumelos: os pontos quentes pequenos dentro das regiões fúngicas (oeste).
    warm = np.clip((r - b - 0.12) * 6.0, 0, 1) * np.clip((l - 0.35) / 0.25, 0, 1) * land
    west = np.clip(blur(fungal, 30) * 6.0, 0, 1)
    windows = np.zeros((h, w, 3), np.float32)
    windows[..., 0] = warm * west
    windows[..., 1] = warm * west * 0.75
    windows[..., 2] = warm * west * 0.45
    save_rgb(windows, f"{out}/underworld-windows{SUFFIX}.png")

    # O ember dos Ferreiros: o quente do leste e da torre (fora das janelas do oeste); R = força, mais fundo nos fornos mais
    # brilhantes e densos.
    east = 1.0 - west
    ember_raw = warm * east
    edens = blur(ember_raw, 20)
    edens = edens / max(float(edens.max()), 1e-4)
    ember = np.clip(ember_raw * (0.5 + 0.5 * edens) + 0.25 * edens * np.clip((l - 0.2) / 0.3, 0, 1) * land * east, 0, 1)
    save_l(ember, f"{out}/underworld-ember{SUFFIX}.png")
    heat = np.clip((blur(ember_raw, 4) - 0.35) * 3.0, 0, 1) * east
    save_l(heat, f"{out}/underworld-heat{SUFFIX}.png")

    # A água: o azul saturado (o Mar de Vidro, os rios, o porto), não o brilho pálido dos cogumelos.
    water = np.clip((b - r - 0.06) * 6.0, 0, 1) * np.clip((sat - 0.08) * 5.0, 0, 1) * np.clip((0.78 - l) / 0.2, 0, 1) * land
    water = water * (1.0 - np.clip(fungal * 2.0, 0, 1))
    water = blur(water, 2)
    save_l(np.clip(water * 1.4, 0, 1), f"{out}/underworld-water{SUFFIX}.png")

    # Os esporos: sobre as Marchas e em volta dos aglomerados fúngicos.
    spores = np.clip(blur(fungal, 40) * 5.0, 0, 1) * 0.7 + 0.6 * np.clip(blur(plants, 20) * 3.0, 0, 1)
    spores = np.clip(spores, 0, 1) * land
    save_l(spores, f"{out}/underworld-spores{SUFFIX}.png")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "content/worldmap/masks"
    os.makedirs(out, exist_ok=True)
    surface(load("content/worldmap/world-map-v03.jpg"), out)
    underworld(load("content/worldmap/underworld-atlas-v03.png"), out)


if __name__ == "__main__":
    main()
