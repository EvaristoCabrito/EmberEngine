"""The gothic parapet kit's textures, cut from the user's two parapet PNGs (decoracoes-2d/bridge-parapet-gothic-statues-001.png =
the deck side, bridge-parapet-tall-001.png = the void side; the same wall, both faces).

  content/textures/walls/gothic-parapet-v01.jpg   the atlas: per-hex panels (deck: window, statue niche, plain; void: tally marks,
                                                  tally + drain hole, plain) and the pilaster's face, each at 2x the art's pixels
  content/textures/walls/gothic-stone-v01.jpg     the plain stone the solid parts repeat (tops, courses, the balustrade, pinnacles)

Prints the atlas regions (u0, v0top, u1, v1bottom in texture space, v down from the top row) for world/Architecture.cpp.
One hex along the wall = 251 px of the art (the card is 11.85 units wide for 2172 px: 1.73 units per hex).
"""
import sys
from PIL import Image, ImageOps

SRC = 'C:/EmberEngine/decoracoes-2d/'
OUT = 'C:/EmberEngine/content/textures/walls/'
S = Image.open(SRC + 'bridge-parapet-gothic-statues-001.png').convert('RGB')
T = Image.open(SRC + 'bridge-parapet-tall-001.png').convert('RGB')
HEX = 251          # px of art per hex along the wall
SCALE = 2          # the atlas keeps the crops at 2x (BC7 blocks softer than the art otherwise)
PAD = 8


def crop(im, x0, y0, x1, y1):
    return im.crop((x0, y0, x1, y1))


def mirror_fill(strip, width):
    """A `width` wide panel made of the strip and its mirror, alternating (seamless)."""
    out = Image.new('RGB', (width, strip.height))
    x = 0
    k = 0
    while x < width:
        piece = strip if k % 2 == 0 else ImageOps.mirror(strip)
        out.paste(piece, (x, 0))
        x += strip.width
        k += 1
    return out


def centred(core, width, pad_strip):
    """`core` centred in a `width` panel, the sides filled with the plain strip (mirrored outwards)."""
    out = Image.new('RGB', (width, core.height))
    left = (width - core.width) // 2
    fill = mirror_fill(pad_strip, width)
    out.paste(fill, (0, 0))
    out.paste(core, (left, 0))
    return out


# --- deck side (the statue PNG): body rows 296..640 -------------------------------------------------------------------
BODY_S = (296, 640)
BODY_T = (304, 650)
plain_s = crop(S, 268, BODY_S[0], 299, BODY_S[1])          # the plain stone between the pilaster and the first window
window = crop(S, 276, BODY_S[0], 500, BODY_S[1])           # the two-light tracery window with its stone margins (224 px; 268-276 is the pilaster's shadow)
niche = crop(S, 500, BODY_S[0], 690, BODY_S[1])            # the hooded guardian in its niche (190 px)
deck_window = centred(window, HEX, plain_s)
deck_niche = centred(niche, HEX, plain_s)
# a plain deck panel: the same stone as the void side's unmarked bay (the 31 px strip mirrored read as a comb of jambs)
void_plain_src = crop(T, 1610, BODY_T[0], 1610 + HEX, BODY_T[1])
deck_plain = void_plain_src.resize((HEX, BODY_S[1] - BODY_S[0]), Image.LANCZOS)
# --- void side (the tall PNG): body rows 304..650 (the drain hole reaches the plinth) ----------------------------------
BODY_T = (304, 650)
void_tally = crop(T, 300, BODY_T[0], 300 + HEX, BODY_T[1])
void_drain = crop(T, 520, BODY_T[0], 520 + HEX, BODY_T[1])
void_plain = void_plain_src
# --- the pilaster's face (the statue PNG): from the cap's foot (150) to the plinth (660) ---------------------------------
pier = crop(S, 144, 150, 264, 660)

panels = {
    'deckWindow': deck_window, 'deckNiche': deck_niche, 'deckPlain': deck_plain,
    'voidTally': void_tally, 'voidDrain': void_drain, 'voidPlain': void_plain,
    'pier': pier,
}
# the atlas: 2048 x 2048, each panel at 2x with an 8 px replicated border
W = H = 2048
atlas = Image.new('RGB', (W, H), (60, 60, 62))
regions = {}
x = PAD
y = PAD
row_h = 0
for name, im in panels.items():
    im2 = im.resize((im.width * SCALE, im.height * SCALE), Image.LANCZOS)
    if x + im2.width + PAD > W:
        x = PAD
        y += row_h + 2 * PAD
        row_h = 0
    # replicated border
    bordered = ImageOps.expand(im2, PAD)
    for dx, dy in ((0, 0),):
        pass
    b = Image.new('RGB', (im2.width + 2 * PAD, im2.height + 2 * PAD))
    b.paste(im2.resize((im2.width + 2 * PAD, im2.height + 2 * PAD)), (0, 0))  # a stretched copy under it (edge colours)
    b.paste(im2, (PAD, PAD))
    atlas.paste(b, (x - PAD, y - PAD))
    regions[name] = (x, y, x + im2.width, y + im2.height)
    x += im2.width + 2 * PAD
    row_h = max(row_h, im2.height)
assert y + row_h + PAD <= H, (y, row_h)
atlas.save(OUT + 'gothic-parapet-v01.jpg', quality=95, subsampling=0)

# --- the plain stone tile: the tall side's unmarked lower course, stacked mirrored to a square ----------------------------
stone = crop(T, 1620, 478, 1850, 620)   # 230 x 142: the course under the string course, no marks, no drain hole, no pilaster (1.59 units of wall)
rows = [stone, ImageOps.flip(stone)]
tile = Image.new('RGB', (230, 284))
yy = 0
for r in rows:
    tile.paste(r, (0, yy))
    yy += r.height
tile = tile.crop((0, 27, 230, 257))
tile = tile.resize((1024, 1024), Image.LANCZOS)
tile.save(OUT + 'gothic-stone-v01.jpg', quality=95, subsampling=0)

print('// atlas regions (gothic-parapet-v01.jpg 2048 x 2048): u0, vTop, u1, vBottom in texture space (v down from the top)')
for name, (x0, y0, x1, y1) in regions.items():
    print(f'  {{"{name}", {x0 / W:.5f}, {y0 / H:.5f}, {x1 / W:.5f}, {y1 / H:.5f}}},  // {x1 - x0} x {y1 - y0} px')
print('stone tile: 230 px = 1.59 units of wall -> textureRepeat 1.59')
