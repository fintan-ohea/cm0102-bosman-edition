#!/usr/bin/env python3
"""Hidden tab check (fork; docs/specs/2026-10-30-hidden-tab-spec.md).

Usage:
    python3 tests/smoke/hidden_tab_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), at the same time:
    on      hidden_tab = on. Opens Ray Parlour's profile from the Arsenal squad, clicks Hidden, then Contract, then
            Hidden again and History.
    off     hidden_tab = off. The same clicks.

Checks:
    - on: the Hidden tab draws twice, for the same player, with current and potential ability from 1 to 200 and the
      19 other values from 1 to 20; moving to Contract and History from it shows no error box.
    - off: the tab is never drawn and the run has no error box.
Pictures in <out>/<run>/: hidden.bmp, contract.bmp.
"""
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

# Positions for 800x600 (classic). With six tabs the bar has two rows: Profile, Injuries & Bans and Contract on top
# (y 97), Transfer, History and Hidden below the page (y 527); x 215, 445 and 675.
STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156",                   # Arsenal Squad
    "7500 click 210 369",                   # Ray Parlour
    "10000 click 675 527",                  # Hidden
    "12500 dump {d}/hidden.bmp",
    "13000 click 675 97",                   # Contract
    "15500 dump {d}/contract.bmp",
    "16000 click 675 527",                  # Hidden
    "18500 click 445 527",                  # History
    "21000 quit",
]
LINE = re.compile(r"recomp: hidden tab: staff (\d+) ca (-?\d+) pa (-?\d+)((?: -?\d+){19})\n")


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
    runs = [("on", [("hidden_tab", "on")]), ("off", [("hidden_tab", "off")])]
    with ThreadPoolExecutor(len(runs)) as pool:
        results = dict(zip([r[0] for r in runs],
                           pool.map(lambda r: play(program, game, folder, os.path.join(out, r[0]), r[1]), runs)))
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    code, log = results["on"]
    seen = [(int(m.group(1)), int(m.group(2)), int(m.group(3)), [int(v) for v in m.group(4).split()])
            for m in LINE.finditer(log)]
    check(code == 0 and "Error" not in log, f"on: the run ends without an error box (code {code})")
    check(len(seen) == 2 and seen[0] == seen[1], f"on: the Hidden tab draws twice for the same player ({len(seen)})")
    if seen:
        staff, ca, pa, values = seen[0]
        check(1 <= ca <= 200 and 1 <= pa <= 200, f"on: current ability {ca}, potential ability {pa}")
        check(all(1 <= v <= 20 for v in values), f"on: the other values are 1 to 20 ({values})")
    code, log = results["off"]
    check(code == 0 and "Error" not in log and not LINE.search(log), f"off: no Hidden tab, no error box (code {code})")
    print(f"pictures in {out}/<run>/")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
