"""Portraits of every non-hero unit, framed straight (the user, 2026-10-28: "não é capaz de colocar um frame bem cropado dos monstros
sem eu pedir?", "PARE DE CROPAR TORTO", "todos os outros também precisam"): content/assets/game/portraits/sprites/<sprite>.png,
what MapStage::portraitFileOf draws for an enemy / neutral (512 x 768, RGB, the figure on black, like the cuts already there).

The game shows the file through a box: the status sheet's 108 x 130 (BattleStatus coverRounded, fx 0.5, fy 0.3) and the footer's
56 x 80 (imageCover, fx 0.5, fy 0.3). In the 2:3 file the status box keeps the full width and rows 0.06 .. 0.86, so the subject is
centred on x 0.5 and y 0.46 (the middle of those rows) and sized to sit inside them with a margin. The subject:

    "auto"  an upright figure: its head and shoulders, found from the frame's own alpha (the top 24 % of the figure; centred on the
            head, the opaque pixels of the figure's top 4-14 % rows, so a weapon or staff beside the body does not pull it sideways)
    "box"   [x0, y0, x1, y1] as shares of the frame: the face, centred, measured on a labelled grid of the frame (never guessed)
    "keep"  [x0, y0, x1, y1] (optional): what must still show around the centred face (horns, antlers): the crop opens to fit it

A file already there is moved to portraits/sprites/kept-<date>/ first (nothing is thrown away), unless --keep-existing.

    python -I tools/scripts/sprites/monster_portraits.py content/portraits/monster-portraits.json [--only a,b] [--sheet out.png]
"""
import argparse
import datetime
import json
import os

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, "content", "assets", "game", "portraits", "sprites")
W, H = 512, 768
CX, CY = 0.5, 0.46          # the middle of what the status box shows of the file
FIT_W, FIT_H = 0.80, 0.64   # the subject at most this share of the file's width / height (inside rows 0.06 .. 0.86 with margin)


def load(path):
    im = Image.open(os.path.join(ROOT, path)).convert("RGBA")
    return im, np.asarray(im)[..., 3]


def auto_subject(alpha):
    ys, xs = np.nonzero(alpha > 96)
    bottom = ys.max()
    # the figure's top = the first row as wide as a head (5 % of the figure's height), so a spear's or a staff's tip above the
    # head is not taken for it
    rows = np.bincount(ys, minlength=alpha.shape[0])
    rough = bottom - ys.min() + 1
    wide = np.nonzero(rows >= 0.05 * rough)[0]
    top = int(wide.min()) if len(wide) else int(ys.min())
    fig = bottom - top + 1
    keep = ys >= top
    ys, xs = ys[keep], xs[keep]
    band = (ys >= top + 0.04 * fig) & (ys <= top + 0.14 * fig)
    hx = float(np.median(xs[band])) if band.any() else float(np.median(xs))
    sh = 0.24 * fig
    sw = 0.30 * fig
    return hx - sw / 2, top - 0.02 * fig, hx + sw / 2, top + sh


def crop_for(subject, keep=None):
    x0, y0, x1, y1 = subject
    sw, sh = x1 - x0, y1 - y0
    ch = max(sh / FIT_H, sw / FIT_W * H / W)
    if keep:
        # the face stays centred; the crop opens until `keep` (horns, antlers) fits the part the box shows (full width, rows
        # 0.06 .. 0.86 of the file, i.e. 0.40 of the height either side of the centre row 0.46)
        fx, fy = (x0 + x1) / 2, (y0 + y1) / 2
        kx0, ky0, kx1, ky1 = keep
        ch = max(ch, 2 * max(fx - kx0, kx1 - fx) * H / W / 0.96, max(fy - ky0, ky1 - fy) / 0.38)
    cw = ch * W / H
    left = (x0 + x1) / 2 - CX * cw
    top = (y0 + y1) / 2 - CY * ch
    return left, top, cw, ch


def render(im, crop):
    left, top, cw, ch = crop
    bg = Image.new("RGBA", im.size, (0, 0, 0, 255))
    bg.alpha_composite(im)
    src = bg.convert("RGB")
    # supersample: paste the frame on a black canvas the crop's size (past the frame's edge is black), then Lanczos to 512 x 768
    k = max(1.0, W / cw)
    big = src.resize((round(src.width * k), round(src.height * k)), Image.LANCZOS) if k > 1 else src
    canvas = Image.new("RGB", (round(cw * k), round(ch * k)), (0, 0, 0))
    canvas.paste(big, (round(-left * k), round(-top * k)))
    return canvas.resize((W, H), Image.LANCZOS)


def in_box(p, bw=108, bh=130):
    scale = max(bw / W, bh / H)
    vw, vh = bw / scale / W, bh / scale / H
    u0, v0 = (1 - vw) * 0.5, (1 - vh) * 0.3
    return p.crop((round(u0 * W), round(v0 * H), round((u0 + vw) * W), round((v0 + vh) * H))).resize((bw * 2, bh * 2), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("--only", default="")
    ap.add_argument("--sheet", default="")
    ap.add_argument("--dry", action="store_true", help="the sheet only; nothing written into the game")
    a = ap.parse_args()
    spec = json.load(open(os.path.join(ROOT, a.spec), encoding="utf-8"))["portraits"]
    only = set(filter(None, a.only.split(",")))
    kept = os.path.join(OUT, "kept-" + datetime.date.today().strftime("%Y%m%d"))
    # the one portrait table (apps/sandbox/Portraits.h unitPortrait): each monster written here gets its row there
    table_path = os.path.join(ROOT, "content", "data", "portraits.json")
    table = json.load(open(table_path, encoding="utf-8"))
    tiles = []
    for sprite, s in spec.items():
        if only and sprite not in only:
            continue
        im, alpha = load(s["src"])
        if s.get("mirror"):
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
            alpha = alpha[:, ::-1]
        if "box" in s:
            bx = s["box"]
            subject = (bx[0] * im.width, bx[1] * im.height, bx[2] * im.width, bx[3] * im.height)
        else:
            subject = auto_subject(alpha)
        keep = None
        if "keep" in s:
            k = s["keep"]
            keep = (k[0] * im.width, k[1] * im.height, k[2] * im.width, k[3] * im.height)
        p = render(im, crop_for(subject, keep))
        tiles.append((sprite, in_box(p)))
        if a.dry:
            continue
        dst = os.path.join(OUT, sprite + ".png")
        if os.path.exists(dst) and not s.get("ours"):
            os.makedirs(kept, exist_ok=True)
            if not os.path.exists(os.path.join(kept, sprite + ".png")):
                os.replace(dst, os.path.join(kept, sprite + ".png"))
        p.save(dst)
        table["sprites"][sprite] = {"src": f"/game/portraits/sprites/{sprite}.png", "framed": True}
        print(sprite, "->", os.path.relpath(dst, ROOT))
    if not a.dry:
        text = json.dumps(table, indent=1, ensure_ascii=False) + "\n"
        with open(table_path + ".tmp", "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        os.replace(table_path + ".tmp", table_path)
    if a.sheet and tiles:
        cols = 10
        rows = (len(tiles) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * 224, rows * 282), (24, 24, 24))
        d = ImageDraw.Draw(sheet)
        for k, (name, t) in enumerate(tiles):
            x, y = (k % cols) * 224, (k // cols) * 282
            sheet.paste(t, (x, y))
            d.line([(x + 108, y), (x + 108, y + 6)], fill=(255, 0, 0))
            d.text((x + 2, y + 264), name[:32], fill=(220, 220, 220))
        sheet.save(a.sheet)


if __name__ == "__main__":
    main()
