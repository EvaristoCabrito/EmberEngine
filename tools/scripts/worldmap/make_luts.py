"""Mapa-múndi: as tabelas de cor (LUT) por hora do dia (shaders/worldmap.hlsl, render/WorldMapFx.h).

Uma LUT é uma imagem de 32 x 32 x 32 cores: para cada cor da pintura, a cor com que ela aparece àquela hora. Faixa padrão
("strip"): 1024 x 32 pixels, x = vermelho + 32 * azul, y = verde. O shader mistura duas LUTs vizinhas pela hora do relógio.

    python -I tools/scripts/worldmap/make_luts.py [content/worldmap/lut]

Escreve dawn / day / dusk / night -v01.png (uma versão nova nunca sobrescreve a anterior: mude SUFFIX).
"""
import os
import sys

import numpy as np
from PIL import Image

SIZE = 32
SUFFIX = "-v01"
VERSIONS = {"dusk": "-v02"}  # looks changed after their v01 went into the tree (the old file stays)


def luma(c):
    return c[..., 0] * 0.299 + c[..., 1] * 0.587 + c[..., 2] * 0.114


def saturate(c, k):
    l = luma(c)[..., None]
    return l + (c - l) * k


def contrast(c, k, pivot=0.45):
    return (c - pivot) * k + pivot


def lift_gamma_gain(c, lift, gamma, gain):
    c = np.clip(c, 0, 1)
    c = c * (1 - np.asarray(lift)) + np.asarray(lift)
    c = np.power(np.clip(c, 1e-6, 1), 1.0 / np.asarray(gamma))
    return c * np.asarray(gain)


def split_tone(c, shadow_tint, highlight_tint, balance=0.5):
    l = np.clip(luma(c), 0, 1)[..., None]
    w = np.clip((l - balance) * 2 + 0.5, 0, 1)
    tint = np.asarray(shadow_tint) * (1 - w) + np.asarray(highlight_tint) * w
    return c * tint


def dawn(c):
    # manhã fria e enevoada: sombras levantadas e azuladas, altas luzes rosadas, contraste baixo, saturação menor
    c = saturate(c, 0.78)
    c = contrast(c, 0.86)
    c = lift_gamma_gain(c, [0.09, 0.10, 0.13], [1.0, 1.0, 1.02], [0.98, 0.95, 0.97])
    c = split_tone(c, [0.86, 0.92, 1.05], [1.06, 0.94, 0.92], 0.55)
    return c


def day(c):
    # dia plano e acinzentado: neutro, um pouco menos saturado e menos contrastado que a pintura
    c = saturate(c, 0.88)
    c = contrast(c, 0.94)
    c = lift_gamma_gain(c, [0.03, 0.03, 0.035], [1.0, 1.0, 1.0], [0.99, 0.99, 0.98])
    return c


def dusk(c):
    # entardecer vermelho-acastanhado, apagado: altas luzes para o laranja queimado, sombras violeta, contraste um pouco maior
    # (v02: o v01 saía laranja saturado demais sobre o mapa inteiro)
    c = saturate(c, 0.92)
    c = contrast(c, 1.06)
    c = lift_gamma_gain(c, [0.02, 0.015, 0.03], [1.0, 0.98, 0.96], [0.94, 0.80, 0.70])
    c = split_tone(c, [0.84, 0.78, 0.98], [1.06, 0.94, 0.86], 0.5)
    return c


def night(c):
    # noite azul-acinzentada e dessaturada, ainda legível: escura, sombras com um mínimo
    c = saturate(c, 0.42)
    c = contrast(c, 1.05)
    c = lift_gamma_gain(c, [0.025, 0.03, 0.05], [0.92, 0.94, 1.0], [0.30, 0.36, 0.52])
    c = split_tone(c, [0.92, 0.96, 1.12], [1.0, 1.0, 1.0], 0.4)
    return c


def underworld_dormant(c):
    # o Submundo de dia (os cogumelos absorvem): carvão e ardósia, dessaturado, escuro, as sombras com um mínimo frio
    c = saturate(c, 0.35)
    c = contrast(c, 1.06)
    c = lift_gamma_gain(c, [0.03, 0.035, 0.045], [0.95, 0.97, 1.0], [0.24, 0.26, 0.30])
    c = split_tone(c, [0.92, 0.97, 1.08], [1.0, 1.0, 1.02], 0.4)
    return c


def underworld_awakening(c):
    # o despertar: a meio caminho, já esverdeando e frio
    c = saturate(c, 0.75)
    c = contrast(c, 1.04)
    c = lift_gamma_gain(c, [0.03, 0.04, 0.05], [0.97, 1.0, 1.0], [0.40, 0.52, 0.55])
    c = split_tone(c, [0.86, 0.90, 1.10], [0.92, 1.04, 1.02], 0.45)
    return c


def underworld_bloom(c):
    # a floração: teal frio, azul-esverdeado pálido, sombras violeta apagadas; nunca quente (o vermelho só vem do ember)
    c = saturate(c, 1.08)
    c = contrast(c, 1.0)
    c = lift_gamma_gain(c, [0.03, 0.05, 0.06], [1.0, 1.0, 1.0], [0.58, 0.86, 0.84])
    c = split_tone(c, [0.84, 0.80, 1.10], [0.90, 1.06, 1.02], 0.5)
    return c


LOOKS = {"dawn": dawn, "day": day, "dusk": dusk, "night": night,
         "underworld-dormant": underworld_dormant, "underworld-awakening": underworld_awakening, "underworld-bloom": underworld_bloom}


def identity():
    r = np.arange(SIZE) / (SIZE - 1)
    b, g, rr = np.meshgrid(r, r, r, indexing="ij")  # [b][g][r]
    return np.stack([rr, g, b], axis=-1)  # shape (b, g, r, 3)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "content/worldmap/lut"
    os.makedirs(out, exist_ok=True)
    base = identity()
    for name, fn in LOOKS.items():
        c = np.clip(fn(base.copy()), 0, 1)
        strip = np.zeros((SIZE, SIZE * SIZE, 3), dtype=np.float32)
        for b in range(SIZE):
            strip[:, b * SIZE:(b + 1) * SIZE, :] = c[b]  # y = g, x = r + 32 b
        img = Image.fromarray(np.round(strip * 255).astype(np.uint8), "RGB")
        path = os.path.join(out, f"{name}{VERSIONS.get(name, SUFFIX)}.png")
        if os.path.exists(path):
            print(f"kept {path} (exists; change SUFFIX for a new version)")
            continue
        img.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
