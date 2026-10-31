#!/usr/bin/env python3
"""Bench and substitutes used check (fork; src/recomp_rt/win/subs_option.c).

Usage:
    python3 tests/smoke/subs_option_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), at the same time:
    on      bench = 9, subs_used = 5. Opens Arsenal's squad, then Tactics (picture), then goes on holiday; the
            game saves 20 game days after the load.
    off     bench = off, subs_used = off. The same steps.

Checks:
    - on: the human turn sets Arsenal's unplayed fixtures (at least one), every match that starts has bench 9 and
      5 substitutes used in the word at fixture +0x4d (at least one match), and the game saves.
    - off: the option writes nothing (no "subs option" line) and the game saves.
Pictures in <out>/<run>/: tactics.bmp (the tactics screen with the bench of the next match).
"""
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

# Positions for 800x600 (classic), as tests/smoke/foreign_run.py and smoke_run.py.
STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156",                   # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 169 527",                   # Tactics
    "4000 dump {d}/tactics.bmp",
    "4500 click 358 573",                   # Cancel
    "wait screen:0x454900",
    "1500 click 45 165",                    # the manager's menu
    "3500 click 150 427",                   # Go on Holiday
    "6000 click 705 572",                   # Please Confirm: Yes
]
DAYS = 20
TURN = re.compile(r"recomp: subs option: bench (\d+), used (\d+): (\d+) fixtures")
MATCH = re.compile(r"recomp: subs option: match starts with word ([0-9a-f]{4})")
SAVED = re.compile(r"recomp: saved .* after \d+ game days \(result 1\)")


def play(program, game, folder, out, sets):
    os.makedirs(out, exist_ok=True)
    steps = os.path.join(out, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(STEPS) + "\n")
    code = fixtures.play(program, game, folder, "start.sav", steps, out, "classic", sets, DAYS)
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
    runs = [("on", [("bench", "9"), ("subs_used", "5")]),
            ("off", [("bench", "off"), ("subs_used", "off")])]
    with ThreadPoolExecutor(len(runs)) as pool:
        results = dict(zip([r[0] for r in runs],
                           pool.map(lambda r: play(program, game, folder, os.path.join(out, r[0]), r[1]), runs)))
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    code, log = results["on"]
    turns = [(int(b), int(u), int(n)) for b, u, n in TURN.findall(log)]
    words = [int(w, 16) for w in MATCH.findall(log)]
    check(code == 0 and SAVED.search(log), f"on: the game saves {DAYS} days after the load (code {code})")
    check(turns and turns[0][:2] == (9, 5) and turns[0][2] > 0, f"on: the human turn sets the club's fixtures {turns[:2]}")
    check(words and all(w & 0xff == 0x59 for w in words),
          f"on: each match starts with bench 9 and 5 used ({[f'{w:04x}' for w in words]})")
    code, log = results["off"]
    check(code == 0 and SAVED.search(log) and "subs option" not in log,
          f"off: the game saves and the option writes nothing (code {code})")
    print(f"pictures in {out}/<run>/")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
