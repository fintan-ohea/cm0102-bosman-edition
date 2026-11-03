#!/usr/bin/env python3
"""Value range check (fork; docs/specs/2026-10-30-value-range-spec.md).

Usage:
    python3 tests/smoke/value_range_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), at the same time:
    on      value_range = on, shortlist_all = on. Opens Player Search, then Filters: Value Range, From £1M, To £5M,
            Ok, and Shortlist All, which writes how many players the list shows. Then Filters: From £5M (To goes
            to No limit), Ok, Shortlist All. Then Filters: Value Any, Ok, Shortlist All.
    off     value_range = off, shortlist_all = on. The same clicks: the screen has no From and To.

Checks:
    - on: the clicks set £1M to £5M and £5M and up; each range lists fewer players than any value, more than none,
      and £1M to £5M and £5M and up together list no more than any value (they meet only at exactly £5M).
    - off: no range is set: Range picks the original's first band, 0 - £100K, for the first two lists, and
      Value Any shows as many players as with the option on.
Pictures in <out>/<run>/: from.bmp (the From list), to.bmp (the To list), range.bmp (the row with £1M - £5M).
"""
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

# Positions for 800x600 (classic). Search: Filters 600 135, Shortlist All 460 135. Filters: Value Any 327 284,
# Range 405 284, From 640 284, To 718 284, Ok 690 524. The From list: 0 at y 54, then every 20.7 (£1M 137, £5M
# 179). The To list: £100K at y 54, then every 20.7 (£5M 158), No limit 283.
STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 190 302",                   # Player & Staff Search
    "wait screen:0x808690",
    "3000 click 600 135",                   # Filters
    "5000 click 405 284",                   # Value Range
    "7000 click 640 284",                   # From
    "8500 dump {d}/from.bmp",
    "9000 click 530 137",                   # £1M
    "11000 click 718 284",                  # To
    "12500 dump {d}/to.bmp",
    "13000 click 620 158",                  # £5M
    "15000 dump {d}/range.bmp",
    "15500 click 690 524",                  # Ok
    "18000 click 460 135",                  # Shortlist All
    "21000 click 600 135",                  # Filters
    "23000 click 640 284",                  # From
    "24500 click 530 179",                  # £5M
    "26000 click 690 524",                  # Ok
    "28500 click 460 135",                  # Shortlist All
    "31500 click 600 135",                  # Filters
    "33500 click 327 284",                  # Value Any
    "35000 click 690 524",                  # Ok
    "37500 click 460 135",                  # Shortlist All
    "40000 quit",
]
SHOWN = re.compile(r"recomp: shortlist all: list \d+, (\d+) shown")
RANGE = re.compile(r"recomp: value range: (-?\d+) to (-?\d+)")


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
    runs = [("on", [("value_range", "on"), ("shortlist_all", "on")]),
            ("off", [("value_range", "off"), ("shortlist_all", "on")])]
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
    ranges = [(int(a), int(b)) for a, b in RANGE.findall(log)]
    check(code == 0 and "Error" not in log, f"on: the run ends without an error box (code {code})")
    check(ranges == [(1000000, -1), (1000000, 5000000), (5000000, -1)],
          f"on: From £1M, To £5M, then From £5M sets £5M and up ({ranges})")
    check(len(shown) == 3, f"on: three lists counted ({shown})")
    if len(shown) == 3:
        middle, top, every = shown
        check(0 < middle < every and 0 < top < every,
              f"on: £1M to £5M lists {middle} and £5M and up {top}, of {every} for any value")
        check(middle + top <= every, f"on: the two ranges together ({middle + top}) fit inside any value")
    code, log = results["off"]
    shown_off = [int(n) for n in SHOWN.findall(log)]
    check(code == 0 and "Error" not in log and not RANGE.search(log) and len(shown_off) == 3 and
          0 < shown_off[0] == shown_off[1] < shown_off[2] and (len(shown) < 3 or shown_off[2] == shown[2]),
          f"off: no range; the original's 0 - £100K band, then any value ({shown_off})")
    print(f"pictures in {out}/<run>/")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
