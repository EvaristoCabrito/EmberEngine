"""As máscaras pintadas das duas telas do mapa (render/WorldMapFx.h), tiradas das próprias pinturas, para o usuário aprovar
antes de entrarem no jogo (content/worldmap/*.json "mask"):

  overworld-mask-v01.png   R = onde a névoa baixa se acumula (pântano, ravinas, entre as árvores: o escuro frio da pintura)
  underworld-mask-v01.png  R = onde a névoa de caverna se acumula (galerias, água, abismos), G = onde há seres que brilham
                           (o saturado e frio: as florestas de fungos azul-esverdeadas)

e uma prévia de cada (a pintura com a máscara por cima, em cor) para olhar:  overworld-mask-v01-preview.jpg etc.

  python -I tools/scripts/worldmap/make_masks.py [out dir = content/worldmap]
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[3]
OVERWORLD = ROOT / "content/game/public/game/assets/world-map.jpg"
UNDERWORLD = ROOT / "content/ui/underworld-atlas-v01.png"


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.float32) / 255.0


def luma(rgb):
    return rgb @ np.array([0.299, 0.587, 0.114], dtype=np.float32)


def smooth(a, radius):
    img = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
    return np.asarray(img.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def disc(shape, x, y, r, soft=0.5):
    """Um disco suave em percentagem da pintura (centro x, y; raio r; borda de r*soft)."""
    h, w = shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    d = np.hypot(xx / w * 100 - x, yy / h * 100 - y) / r
    return np.clip((1.0 - d) / soft + 1.0, 0, 1) if soft > 0 else (d <= 1).astype(np.float32)


# Mapa-múndi (content/engine2/overworld.json: x, y em % da pintura): onde a névoa baixa se junta (pântano, ravinas, bosques) e
# as construções, que não são parte baixa (o escuro delas enganava o corte automático).
LOW_PLACES = [(51.96, 75, 13), (82.27, 82.5, 16), (12.99, 82.5, 13), (34.64, 60, 9), (8.66, 30, 9), (73.61, 52.5, 8), (51.96, 45, 7), (30, 78, 12), (60, 88, 12)]
BUILT_PLACES = [(22, 25, 10), (14, 38, 7), (24, 50, 6), (51.96, 57, 9), (77.94, 30, 9), (51.96, 15, 13), (86, 52, 8), (14, 62, 4)]


def low_fog(rgb):
    """Onde a névoa baixa se junta: o escuro e frio da pintura, só nos lugares baixos, nunca sobre as construções."""
    l = luma(rgb)
    cold = np.clip((np.maximum(rgb[..., 1], rgb[..., 2]) - rgb[..., 0]) * 3.0, 0, 1)
    dark = np.clip((0.38 - l) / 0.38, 0, 1)
    m = dark * (0.5 + 0.5 * cold)
    m = smooth(m, 6)
    m = np.clip((m - 0.06) / 0.45, 0, 1)
    places = np.zeros(rgb.shape[:2], dtype=np.float32)
    for x, y, r in LOW_PLACES:
        places = np.maximum(places, disc(rgb.shape, x, y, r))
    built = np.zeros(rgb.shape[:2], dtype=np.float32)
    for x, y, r in BUILT_PLACES:
        built = np.maximum(built, disc(rgb.shape, x, y, r, 0.35))
    m = m * (0.25 + 0.75 * places) * (1.0 - built)
    return smooth(m, 3)


def cave_fog(rgb):
    """Submundo: entre as galerias, sobre a água e nos abismos: o escuro que não é a rocha vermelha, mais a água."""
    l = luma(rgb)
    red = np.clip((rgb[..., 0] - np.maximum(rgb[..., 1], rgb[..., 2])) * 12.0, 0, 1)  # a rocha vermelha das paredes
    red = smooth(red, 4)
    cold = np.clip((np.maximum(rgb[..., 1], rgb[..., 2]) - rgb[..., 0]) * 3.0, 0, 1)
    # o escuro só conta quando é frio (abismos azul-roxos, o fundo das galerias); as paredes pardas de rocha ficam de fora
    blueish = np.clip((rgb[..., 2] - rgb[..., 0] * 0.95 + 0.05) * 8.0, 0, 1)
    dark = np.clip((0.36 - l) / 0.36, 0, 1) * (1.0 - red) ** 2 * blueish
    water = cold * np.clip((rgb.max(axis=2) - rgb.min(axis=2) - 0.1) * 3.0, 0, 1) * np.clip((0.6 - l) / 0.3, 0, 1)
    m = np.maximum(dark, water * 0.8)
    m = smooth(m, 7)
    m = np.clip((m - 0.08) / 0.45, 0, 1)
    return smooth(m, 3)


def glow_sources(rgb):
    """Os seres que brilham: o claro e frio da pintura (os fungos azul-esverdeados pálidos), nunca o fogo laranja das minas."""
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = mx - mn
    cold = np.clip((np.maximum(rgb[..., 1], rgb[..., 2]) - rgb[..., 0]) * 4.0, 0, 1)
    bright = np.clip((luma(rgb) - 0.5) / 0.3, 0, 1)
    g = np.clip((sat - 0.08) * 5.0, 0, 1) * cold * bright
    g = smooth(g, 2)
    return np.clip(g * 1.6, 0, 1)


def preview(rgb, r, g, path):
    out = rgb.copy() * 0.55
    out[..., 0] += r * 0.9  # névoa em vermelho
    out[..., 1] += g * 0.9  # seres em verde
    Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8)).save(path, quality=88)


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "content/worldmap"
    out.mkdir(parents=True, exist_ok=True)
    ow = load(OVERWORLD)
    r = low_fog(ow)
    mask = np.zeros(ow.shape, dtype=np.float32)
    mask[..., 0] = r
    Image.fromarray((mask * 255).astype(np.uint8)).save(out / "overworld-mask-v01.png")
    preview(ow, r, np.zeros_like(r), out / "overworld-mask-v01-preview.jpg")
    uw = load(UNDERWORLD)
    r2, g2 = cave_fog(uw), glow_sources(uw)
    mask2 = np.zeros(uw.shape, dtype=np.float32)
    mask2[..., 0], mask2[..., 1] = r2, g2
    Image.fromarray((mask2 * 255).astype(np.uint8)).save(out / "underworld-mask-v01.png")
    preview(uw, r2, g2, out / "underworld-mask-v01-preview.jpg")
    print(f"overworld: low fog covers {r.mean() * 100:.1f} % of the painting")
    print(f"underworld: cave fog {r2.mean() * 100:.1f} %, glowing beings {g2.mean() * 100:.1f} %")


if __name__ == "__main__":
    main()
