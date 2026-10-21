#!/usr/bin/env python3
"""Full player search check (spec 2026-10-16-features-0.5.0, slice 5): the option full_player_search.

Usage:
    python3 tests/smoke/player_search_run.py <cm0102_recomp> <game folder> <out folder> [--ref <cm0102_recomp>]
        [--dir <fixtures folder>] [--save start.sav] [--big] [--jobs n]

--ref is a build without the option. The default is the main build of this project:
/Users/max/Projects/cm0102/cm0102-mac/build/cm0102_recomp.

Three runs from the save (default start.sav, Arsenal 14.7.01), at the same time: the Player & Staff Search
screen, Player Search tab, which lists "Famous players in your region", with
    off      full_player_search = off (the default)
    on       full_player_search = on
    ref      the program of --ref
Each run saves the screen as <out>/<run>/search.bmp, and the screen of the start as start.bmp.
(fixtures.py also writes a .png copy of each picture.)

Checks:
    - off and ref give the same picture, byte for byte (off keeps the original's path)
    - on lists Abbey, Ben of Woking (Conference, S C), a player that the manager does not know. He is row 2 of
      page 1 of on and not in off. The test compares the text pixels of the cells: no row of off has
      that name cell, and the cells for the position and the division of that row are the same as
      those of other rows of on that show the same text ("S C" of Abbey, Zema, and "Conference" of
      Abbott, Billy). The rows are in name order, so the rows of off are not the rows of on.
From start.sav, off lists 1678 players (153 pages). The page count of on is higher.

--jobs n: at most n games at the same time (default 3, all at once).

--big (forum report of 2026-10-21): the save allleagues.sav (tests/fixtures/fixtures.py make --all: every league,
Arsenal 14.7.01). There on would list more than 32767 players, the limit of the screen's 16-bit count; before
the fix the screen said "1 player found (N filtered out)" over an empty list. Checks:
    - off and ref give the same picture, byte for byte
    - on fills every row of page 1 (each name cell has text)
    - on gives another picture than off
"""
import os
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402

MAIN_BUILD = "/Users/max/Projects/cm0102/cm0102-mac/build/cm0102_recomp"

STEPS = [
    "1000 dump {d}/start.bmp",
    "2500 click 45 165",                    # the manager's menu
    "4000 click 190 302",                   # Player & Staff Search
    "wait screen:0x808690",
    "2000 dump {d}/search.bmp",
    "3000 quit",
]

# The cells of the list in 800x600 pixels: the rows are 19.2 pixels apart from y = 250.
ROW_Y = [int(250 + 19.2 * i) for i in range(11)]
NAME_X, BASED_X, POSITION_X = (190, 330), (487, 607), (622, 705)


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, _, bits = struct.unpack_from("<iiHH", data, 18)
    assert bits == 24, "a 24 bit picture is expected"
    stride = (width * 3 + 3) // 4 * 4
    return data, offset, stride, height


def cell(picture, x_range, row, colour):
    """The text pixels of one cell: rows y - 8 to y + 8 and columns x_range. The list lies over a photo, so
    the test keeps only the pixels of the text colour: "white", "yellow" or "orange"."""
    data, offset, stride, height = picture
    y = ROW_Y[row]
    out = []
    for yy in range(y - 8, y + 9):
        base = offset + (height - 1 - yy) * stride
        for x in range(x_range[0], x_range[1]):
            b, g, r = data[base + 3 * x], data[base + 3 * x + 1], data[base + 3 * x + 2]
            if colour == "white":
                out.append(r > 220 and g > 220 and b > 220)
            elif colour == "yellow":
                out.append(r > 220 and g > 220 and b < 100)
            else:
                out.append(r > 220 and 100 < g < 200 and b < 80)
    return tuple(out)


def same_text(a, b):
    """Two cells show the same text: they have text pixels, and at most one in ten of them differs."""
    lit = sum(a) + sum(b)
    return lit > 0 and sum(x != y for x, y in zip(a, b)) * 10 <= lit


def play(program, game, folder, save, out, sets):
    steps = os.path.join(out, "steps.txt")
    os.makedirs(out, exist_ok=True)
    with open(steps, "w") as f:
        f.write("\n".join(STEPS) + "\n")
    return fixtures.play(program, game, folder, save, steps, out, "classic", sets, None)


def main(argv):
    args = argv[1:]
    opts = {"--ref": MAIN_BUILD, "--dir": fixtures.DEFAULT_DIR, "--save": "start.sav", "--jobs": "3"}
    big = "--big" in args
    args = [a for a in args if a != "--big"]
    if big:
        opts["--save"] = "allleagues.sav"
    rest = []
    i = 0
    while i < len(args):
        if args[i] in opts:
            opts[args[i]] = args[i + 1]
            i += 2
        else:
            rest.append(args[i])
            i += 1
    if len(rest) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(p) for p in rest)
    if not os.path.exists(opts["--ref"]):
        print(f"FAIL: no build without the option at {opts['--ref']}: use --ref")
        return 2
    runs = [("off", program, [("full_player_search", "off")]), ("on", program, [("full_player_search", "on")]),
            ("ref", os.path.abspath(opts["--ref"]), [])]
    with ThreadPoolExecutor(max(1, min(len(runs), int(opts["--jobs"])))) as pool:
        codes = list(pool.map(lambda r: play(r[1], game, opts["--dir"], opts["--save"], os.path.join(out, r[0]),
                                             r[2]), runs))
    if any(codes):
        print("FAIL: a run failed", codes)
        return 1

    def path(name):
        return os.path.join(out, name, "search.bmp")

    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= passed

    check(open(path("off"), "rb").read() == open(path("ref"), "rb").read(),
          "off gives the picture of the build without the option")
    off, on = read_bmp(path("off")), read_bmp(path("on"))
    if big:
        check(all(any(cell(on, NAME_X, r, "white")) for r in range(11)), "on fills the 11 rows of page 1")
        check(open(path("on"), "rb").read() != open(path("off"), "rb").read(), "on gives another picture than off")
        print(f"pictures in {out}/<run>/search.png (the screen of the Player Search tab)")
        return 0 if ok else 1
    row = 1                                          # Abbey, Ben is row 2 of page 1 of on
    name = cell(on, NAME_X, row, "white")
    check(all(cell(off, NAME_X, r, "white") != name for r in range(11)), "off has no row with the name cell of row 2 of on")
    check(same_text(cell(on, POSITION_X, row, "orange"), cell(on, POSITION_X, 4, "orange")), "row 2 of on shows the position of Abbey, Zema (S C)")
    check(same_text(cell(on, BASED_X, row, "yellow"), cell(on, BASED_X, 7, "yellow")), "row 2 of on shows the division of Abbott, Billy (Conference)")
    check(open(path("on"), "rb").read() != open(path("off"), "rb").read(), "on gives another picture than off")
    print(f"pictures in {out}/<run>/search.png (the screen of the Player Search tab)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
