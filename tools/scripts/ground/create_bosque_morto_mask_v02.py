"""Bake the approved Bosque Morto v05 ground transition into one map-wide texture.

The board keeps its authored terrain types and movement rules. All four painted
ground types sample this same continuous texture, so terrain-cell edges cannot
turn the transition into a camouflage pattern.
"""

from pathlib import Path
import math

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "content/ground/bosque-morto-ground-mask-v02.png"
PREVIEW = Path(__file__).with_name("bosque011-ground-mask-preview-v05.png")

COLS, ROWS = 18, 16
REPEAT = 36.0
TEX_SIZE = 3072
SQRT3 = math.sqrt(3.0)
BOARD_WIDTH = SQRT3 * (COLS + 0.5)  # includes the outer pointy-top hex corners
BOARD_DEPTH = 1.5 * (ROWS - 1) + 2.0


def smoothstep(low: float, high: float, value: np.ndarray) -> np.ndarray:
    t = np.clip((value - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def cover(path: Path, size: tuple[int, int]) -> np.ndarray:
    image = Image.open(path).convert("RGB")
    width, height = size
    scale = max(width / image.width, height / image.height)
    image = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
    left, top = (image.width - width) // 2, (image.height - height) // 2
    return np.asarray(image.crop((left, top, left + width, top + height)), dtype=np.float32)


def bake_board_field() -> Image.Image:
    width = round(TEX_SIZE * BOARD_WIDTH / REPEAT)
    height = round(TEX_SIZE * BOARD_DEPTH / REPEAT)
    ground = cover(ROOT / "content/backdrops/bosque-ground-v01.jpg", (width, height))
    leaves = cover(ROOT / "content/ground/bosque-morto-litter-v01.png", (width, height))
    moss = cover(ROOT / "content/ground/bosque-morto-moss-v01.png", (width, height))

    x = (np.arange(width, dtype=np.float32) + 0.5) / width
    y = (np.arange(height, dtype=np.float32) + 0.5) / height
    xx, yy = np.meshgrid(x, y)
    progress = 0.52 * xx + 0.48 * yy
    leaf_weight = (0.04 + 0.72 * smoothstep(0.12, 0.88, progress))[..., None]
    moss_weight = (0.02 + 0.38 * smoothstep(0.45, 1.05, progress))[..., None]

    field = ground * (1.0 - leaf_weight) + leaves * leaf_weight
    field = field * (1.0 - moss_weight) + moss * moss_weight
    return Image.fromarray(np.clip(field, 0, 255).astype(np.uint8), "RGB")


def sample_bilinear(image: np.ndarray, sx: np.ndarray, sy: float) -> np.ndarray:
    height, width = image.shape[:2]
    gx = np.clip(sx, 0.0, 1.0) * (width - 1)
    gy = np.clip(sy, 0.0, 1.0) * (height - 1)
    x0 = np.floor(gx).astype(np.int32)
    x1 = np.minimum(x0 + 1, width - 1)
    y0 = int(math.floor(gy))
    y1 = min(y0 + 1, height - 1)
    fx = (gx - x0)[:, None]
    fy = gy - y0
    top = image[y0, x0] * (1.0 - fx) + image[y0, x1] * fx
    bottom = image[y1, x0] * (1.0 - fx) + image[y1, x1] * fx
    return top * (1.0 - fy) + bottom * fy


def pack_for_world_uv(field: Image.Image) -> Image.Image:
    """Place the centered board into the sampler's repeating world-UV tile.

    World coordinates are centered at zero. The texture therefore stores each
    half-board at the corresponding side of the u/v wrap seam. The untouched
    texture area is filled by clamping to the board edge to keep mip filtering clean.
    """
    image = np.asarray(field, dtype=np.float32)
    span_x, span_y = BOARD_WIDTH / REPEAT, BOARD_DEPTH / REPEAT
    us = (np.arange(TEX_SIZE, dtype=np.float32) + 0.5) / TEX_SIZE
    sx = np.empty(TEX_SIZE, dtype=np.float32)
    left = us < span_x * 0.5
    right = us > 1.0 - span_x * 0.5
    sx[left] = 0.5 + us[left] / span_x
    sx[right] = 0.5 + (us[right] - 1.0) / span_x
    middle = ~(left | right)
    middle_start, middle_end = span_x * 0.5, 1.0 - span_x * 0.5
    middle_t = (us[middle] - middle_start) / (middle_end - middle_start)
    sx[middle] = 1.0 - middle_t

    out = np.empty((TEX_SIZE, TEX_SIZE, 3), dtype=np.uint8)
    vs = (np.arange(TEX_SIZE, dtype=np.float32) + 0.5) / TEX_SIZE
    for row, v in enumerate(vs):
        if v < span_y * 0.5:
            sy = 0.5 - v / span_y  # north/top half
        elif v > 1.0 - span_y * 0.5:
            sy = 0.5 - (v - 1.0) / span_y  # south/bottom half
        else:
            # Continue the field smoothly through the off-board repeat band.
            sy = (v - span_y * 0.5) / (1.0 - span_y)
        out[row] = np.clip(sample_bilinear(image, sx, sy), 0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGB")


def main() -> None:
    if OUT.exists():
        raise FileExistsError(f"Refusing to overwrite existing version: {OUT}")
    if not PREVIEW.exists():
        raise FileNotFoundError(f"Approved preview is missing: {PREVIEW}")
    # The authored preview's proportions match the 18x16 field closely; use it
    # as the approved field so the saved project asset matches what was reviewed.
    board = Image.open(PREVIEW).convert("RGB").resize(
        (round(TEX_SIZE * BOARD_WIDTH / REPEAT), round(TEX_SIZE * BOARD_DEPTH / REPEAT)),
        Image.Resampling.LANCZOS,
    )
    packed = pack_for_world_uv(board)
    packed.save(OUT, format="PNG", optimize=True)
    print(f"wrote {OUT} ({TEX_SIZE}x{TEX_SIZE}, {BOARD_WIDTH:.2f}x{BOARD_DEPTH:.2f} world units)")


if __name__ == "__main__":
    main()
