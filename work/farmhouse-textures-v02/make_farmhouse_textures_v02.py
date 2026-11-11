"""Build visibly photographic v02 Farmlands farmhouse maps from the generated source photographs."""
from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / "out"
DEST = ROOT / "content/models/textures"
S = 1024
MATERIALS = ("wood", "dark-wood", "iron", "linen", "clay", "herbs", "straw")
ROUGH = {
    "wood": (0.78, 0.16, 10.0),
    "dark-wood": (0.84, 0.12, 9.0),
    "iron": (0.76, 0.18, 8.0),
    "linen": (0.94, 0.06, 12.0),
    "clay": (0.52, 0.18, 10.0),
    "herbs": (0.94, 0.06, 8.0),
    "straw": (0.86, 0.12, 9.0),
}


def helpers():
    sys.path.insert(0, str(ROOT / "work/farmhouse-textures-v01"))
    import make_farmhouse_textures as t
    return t


def save_rgb(path, a, quality=95):
    Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(path, quality=quality, subsampling=0)


def process(t, name):
    source_version = "v01" if name == "straw" else "v02"
    src = ROOT / f"Attachments/Farmhouse/farmhouse-{name}-{source_version}-color.jpg"
    if not src.exists():
        raise FileNotFoundError(src)
    c = t.load(src, "RGB", (S, S))
    lr, tb = t.seam_score(c)
    print(f"{name}: source seam left-right {lr:.2f}, top-bottom {tb:.2f}")
    if max(lr, tb) > 1.5:
        c = t.make_tileable(c, 221 + MATERIALS.index(name), tw=24, amp=42, sigma=18)

    g = t.lum(c)
    detail = g - t.blur(g, 1.25)
    detail = np.clip(0.5 + detail / (5.0 * float(detail.std()) + 1e-6), 0, 1)
    strength = ROUGH[name][2]
    normal = t.normal_from_height(t.blur(detail, 0.55), strength)
    rough_mean, rough_spread, _ = ROUGH[name]
    rough = np.clip(rough_mean + rough_spread * (0.5 - (detail - 0.5) * 1.6), 0.2, 1.0)

    paths = {
        "color": OUT / f"farmhouse-{name}-v02-color.jpg",
        "normal": OUT / f"farmhouse-{name}-v02-normal.jpg",
        "roughness": OUT / f"farmhouse-{name}-v02-roughness.jpg",
    }
    if all(p.exists() and (DEST / p.name).exists() for p in paths.values()):
        print(f"{name}: published v02 set already exists; keeping it")
        return paths["color"]
    if any(p.exists() or (DEST / p.name).exists() for p in paths.values()):
        raise FileExistsError(f"Refusing to overwrite an existing v02 texture for {name}")
    save_rgb(paths["color"], c)
    save_rgb(paths["normal"], normal * 0.5 + 0.5)
    Image.fromarray((np.clip(rough, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(paths["roughness"], quality=95)
    t.report(name, c, normal, rough)

    # A compact, repeatable color preview that makes seam breaks easy to spot.
    tile = Image.fromarray((np.clip(c, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").resize((512, 512), Image.Resampling.LANCZOS)
    preview = Image.new("RGB", (1024, 1024), "#202020")
    for x in range(2):
        for y in range(2):
            preview.paste(tile, (x * 512, y * 512))
    preview.save(HERE / f"check-{name}-v02.png")
    return paths["color"]


def contact_sheet():
    labels = MATERIALS
    tile, gutter, pad = 256, 14, 14
    sheet = Image.new("RGB", (pad * 2 + len(labels) * (2 * tile + gutter), 2 * tile + 72), "#202020")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=18)
    for i, name in enumerate(labels):
        version = "v02"
        src = OUT / f"farmhouse-{name}-{version}-color.jpg"
        tile_image = Image.open(src).convert("RGB").resize((tile, tile), Image.Resampling.LANCZOS)
        x = pad + i * (2 * tile + gutter)
        sheet.paste(tile_image, (x, 0)); sheet.paste(tile_image, (x + tile, 0))
        sheet.paste(tile_image, (x, tile)); sheet.paste(tile_image, (x + tile, tile))
        draw.text((x + 2, 2 * tile + 8), f"farmhouse-{name}-{version}-color.jpg (2 x 2)", fill="white", font=font)
    target = HERE / "contact-sheet-final-v02.png"
    sheet.save(target)
    print("contact sheet", target, sheet.size)


def publish():
    OUT.mkdir(parents=True, exist_ok=True); DEST.mkdir(parents=True, exist_ok=True)
    for src in sorted(OUT.glob("farmhouse-*-v02-*.jpg")):
        dst = DEST / src.name
        if dst.exists():
            if dst.read_bytes() == src.read_bytes():
                print("already published", dst.name)
                continue
            raise FileExistsError(f"Refusing to overwrite a different published texture: {dst}")
        dst.write_bytes(src.read_bytes())
        print("published", dst.name)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    t = helpers()
    for material in MATERIALS:
        process(t, material)
    publish()
    contact_sheet()
