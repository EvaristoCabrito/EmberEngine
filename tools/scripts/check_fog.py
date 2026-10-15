"""Fog-of-war acceptance check (ARCHITECTURE §7.0 R7): never-seen must stay black.

Takes pairs of captures of the same frame (Sandbox --map ... --fog 1 --scene-time T --no-ui): the shaded
frame and the same frame with --debug-view 3 (the mask the front surface read: red = darkness, green =
local-light sight, blue = a surface was drawn). Every pixel whose front surface is fully never seen (debug red
255, blue 255) or where nothing was drawn (never-seen surfaces are not drawn; off the board) must be exactly
black in the shaded frame, whatever lights, spells or shadows do.

    python -I tools/scripts/check_fog.py shaded.png debug.png [shaded2.png debug2.png ...]

Prints the pixel counts per state and the never-seen pixels that are not black; exit 1 if any.
"""
import sys

from PIL import Image


def check(shaded_path, debug_path):
    shaded = Image.open(shaded_path).convert("RGB")
    debug = Image.open(debug_path).convert("RGB")
    if shaded.size != debug.size:
        raise SystemExit(f"{shaded_path} and {debug_path} differ in size")
    sp, dp = shaded.load(), debug.load()
    w, h = shaded.size
    never = remembered = sight = background = leaks = 0
    worst = (0, None)
    for y in range(h):
        for x in range(w):
            r, g, b = dp[x, y]
            if b == 0:
                background += 1
                if sp[x, y] != (0, 0, 0):  # off the board under fog: never seen too
                    leaks += 1
                    worst = max(worst, (sum(sp[x, y]), (x, y)))
                continue
            if r == 255 and b == 255:
                never += 1
                if sp[x, y] != (0, 0, 0):
                    leaks += 1
                    worst = max(worst, (sum(sp[x, y]), (x, y)))
            elif r == 0:
                sight += 1
            else:
                remembered += 1
    print(f"{shaded_path}: in sight {sight}, remembered {remembered}, never seen {never}, background {background} px; "
          f"never-seen pixels not black: {leaks}" + (f" (brightest {worst[0]} at {worst[1]})" if leaks else ""))
    # Never-seen ground is not drawn at all, so it shows as background (black under fog).
    return leaks == 0 and never + background > 0 and remembered > 0 and sight > 0


def main():
    args = sys.argv[1:]
    if not args or len(args) % 2:
        raise SystemExit(__doc__)
    ok = all([check(args[i], args[i + 1]) for i in range(0, len(args), 2)])
    print("PASS: never-seen stays black; all three states on screen" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
