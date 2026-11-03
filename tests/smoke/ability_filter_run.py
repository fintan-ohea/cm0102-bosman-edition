#!/usr/bin/env python3
"""Ability filter check (fork; docs/specs/2026-10-30-hidden-tab-spec.md, "Search by ability").

Usage:
    python3 tests/smoke/ability_filter_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), at the same time:
    on      hidden_tab = on, shortlist_all = on. Opens Player Search, then Filters, Attributes: Ability + ten times
            (100), Ok, and Shortlist All, which writes how many players the list shows. Then Potential + fourteen
            times (140), Ok, Shortlist All. Then Ability - ten times and Potential - fourteen times, Ok, Shortlist
            All.
    off     hidden_tab = off, shortlist_all = on. The same clicks.

Checks:
    - on: the minimums go to 100, then 140 for potential, then back to none; the list shrinks with each minimum and
      is whole again without them.
    - off: no minimum is set and every list is whole.
Pictures in <out>/<run>/: attributes.bmp (the page with Ability 100 and Potential 140).
"""
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

# Positions for 800x600 (classic). Search: Filters 600 135, Shortlist All 460 135. Filters: Attributes 305 125, Ok
# 690 524; the row after Technique: Ability - 532 474, + 587 474; Potential - 690 474, + 744 474.
ABILITY_UP, ABILITY_DOWN, POTENTIAL_UP, POTENTIAL_DOWN = "click 587 474", "click 532 474", "click 744 474", "click 690 474"


def clicks(start, what, n):
    return [f"{start + 300 * k} {what}" for k in range(n)]


STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 190 302",                   # Player & Staff Search
    "wait screen:0x808690",
    "3000 click 600 135",                   # Filters
    "5000 click 305 125",                   # Attributes
] + clicks(6000, ABILITY_UP, 10) + [
    "10000 click 690 524",                  # Ok
    "12500 click 460 135",                  # Shortlist All
    "15000 click 600 135",
    "17000 click 305 125",
] + clicks(18000, POTENTIAL_UP, 14) + [
    "23000 dump {d}/attributes.bmp",
    "23500 click 690 524",
    "26000 click 460 135",
    "28500 click 600 135",
    "30500 click 305 125",
] + clicks(31500, ABILITY_DOWN, 10) + clicks(35000, POTENTIAL_DOWN, 14) + [
    "40000 click 690 524",
    "42500 click 460 135",
    "45000 quit",
]
SHOWN = re.compile(r"recomp: shortlist all: list \d+, (\d+) shown")
SET = re.compile(r"recomp: ability filter: ability (\d+) potential (\d+)")


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
    runs = [("on", [("hidden_tab", "on"), ("shortlist_all", "on")]),
            ("off", [("hidden_tab", "off"), ("shortlist_all", "on")])]
    with ThreadPoolExecutor(len(runs)) as pool:
        results = dict(zip([r[0] for r in runs],
                           pool.map(lambda r: play(program, game, folder, os.path.join(out, r[0]), r[1]), runs)))
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    code, log = results["on"]
    shown = [int(n) for n in SHOWN.findall(log)]
    sets = [(int(a), int(b)) for a, b in SET.findall(log)]
    check(code == 0 and "Error" not in log, f"on: the run ends without an error box (code {code})")
    check(len(sets) == 48 and sets[9] == (100, 0) and sets[23] == (100, 140) and sets[-1] == (0, 0),
          f"on: ability 100, then potential 140, then none ({sets[9:10] + sets[23:24] + sets[-1:]})")
    check(len(shown) == 3, f"on: three lists counted ({shown})")
    if len(shown) == 3:
        ability, both, whole = shown
        check(0 < both < ability < whole, f"on: ability 100 lists {ability}, with potential 140 {both}, of {whole}")
    code, log = results["off"]
    shown_off = [int(n) for n in SHOWN.findall(log)]
    check(code == 0 and "Error" not in log and not SET.search(log) and len(shown_off) == 3 and
          len(set(shown_off)) == 1 and (len(shown) < 3 or shown_off[0] == shown[2]),
          f"off: no minimum, every list whole ({shown_off})")
    print(f"pictures in {out}/<run>/")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
