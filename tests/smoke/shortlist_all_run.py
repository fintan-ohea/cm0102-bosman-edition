#!/usr/bin/env python3
"""Shortlist All check (fork; docs/specs/2026-10-25-shortlist-all-spec.md).

Usage:
    python3 tests/smoke/shortlist_all_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), at the same time:
    on      shortlist_all = on. Opens Player & Staff Search (the Player Search tab), clicks Shortlist All twice,
            moves to Staff Search and clicks it there, closes the screen with Ok, opens it again and clicks
            Shortlist All on the Player Search tab.
    off     shortlist_all = off. The same clicks.

Checks:
    - on: the first click adds every player the list shows; the second adds nobody, since they are on the shortlist;
      on Staff Search it adds fewer people than the list shows (only players can go on the shortlist); after Ok
      and a new opening of the screen a click adds nobody, so Ok kept the shortlist. No error box.
    - off: the screen has no Shortlist All button: the clicks add nobody, and the run has no error box.
Pictures in <out>/<run>/: search.bmp (before the first click), added.bmp (after it), shortlist.bmp (the Short
List after the new opening).
"""
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

# Positions for 800x600 (classic). The tabs: Short List 160 97, Player Search 307 97, Staff Search 445 97. Shortlist
# All is left of Filters, at x 395 to 525 of the row y 125 to 145. Ok 705 573.
STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 190 302",                   # Player & Staff Search
    "wait screen:0x808690",
    "2000 dump {d}/search.bmp",
    "3000 click 460 135",                   # Shortlist All
    "6000 dump {d}/added.bmp",
    "7000 click 460 135",                   # again
    "10000 click 445 97",                   # Staff Search
    "13000 click 460 135",                  # Shortlist All
    "16000 click 705 573",                  # Ok
    "19000 click 45 165",
    "21000 click 190 302",                  # Player & Staff Search again
    "24000 click 307 97",                   # Player Search
    "27000 click 460 135",                  # Shortlist All
    "30000 click 160 97",                   # Short List
    "33000 dump {d}/shortlist.bmp",
    "34000 quit",
]
ADDED = re.compile(r"recomp: shortlist all: list (\d+), (\d+) shown, (\d+) added")


def play(program, game, folder, out, sets):
    os.makedirs(out, exist_ok=True)
    steps = os.path.join(out, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(STEPS) + "\n")
    code = fixtures.play(program, game, folder, "start.sav", steps, out, "classic", sets, None)
    with open(os.path.join(out, "stderr.txt"), errors="replace") as f:
        return code, f.read()


def main(argv):
    args = argv[1:]
    folder = fixtures.DEFAULT_DIR
    if "--dir" in args:
        k = args.index("--dir")
        folder = args[k + 1]
        del args[k:k + 2]
    if len(args) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(a) for a in args)
    runs = [("on", [("shortlist_all", "on")]), ("off", [("shortlist_all", "off")])]
    with ThreadPoolExecutor(len(runs)) as pool:
        results = dict(zip([r[0] for r in runs],
                           pool.map(lambda r: play(program, game, folder, os.path.join(out, r[0]), r[1]), runs)))
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= passed

    code, log = results["on"]
    clicks = [tuple(int(x) for x in m.groups()) for m in ADDED.finditer(log)]
    check(code == 0 and "Error" not in log, f"on: the run ends without an error box (code {code})")
    check(len(clicks) == 4, f"on: four clicks reach Shortlist All ({clicks})")
    if len(clicks) == 4:
        (l1, shown1, added1), (l2, shown2, added2), (l3, shown3, added3), (l4, shown4, added4) = clicks
        check(l1 == 1 and shown1 > 0 and added1 == shown1, f"on: the first click adds all {shown1} players shown")
        check(l2 == 1 and added2 == 0, "on: the second click adds nobody")
        check(l3 == 2 and 0 < added3 < shown3, f"on: Staff Search adds {added3} of {shown3} (the players among them)")
        check(l4 == 1 and added4 == 0, "on: after Ok and a new opening, the players are still on the shortlist")
    code, log = results["off"]
    check(code == 0 and "Error" not in log and not ADDED.search(log),
          f"off: no Shortlist All button, the clicks add nobody (code {code})")
    print(f"pictures in {out}/<run>/")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
