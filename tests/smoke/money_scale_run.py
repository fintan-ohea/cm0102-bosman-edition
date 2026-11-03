#!/usr/bin/env python3
"""Money scale check (fork; docs/specs/2026-10-24-money-scale-spec.md).

Usage:
    python3 tests/smoke/money_scale_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), with player_editor = on:
    off     money_scale = off.
    7       money_scale = 7.
Each opens Ray Parlour's profile from the Arsenal squad, then his Contract and Transfer tabs, then the player
editor box, and dumps each.

Checks:
    - 7: the game's rates are set 7 times (log "money scale 7: rate 7").
    - off: the rates are not touched.
    - contract, transfer, editor: the pictures of off and 7 differ, and only in the rows of money (Wages and
      Bonuses; Value; the editor's Wage, Balance and Funds).
Pictures in <out>/off and <out>/7.
"""
import os
import re
import shutil
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

STEPS = fixtures.LOAD_STEPS + [
    "1000 click 45 165",                    # the manager's menu
    "2500 click 150 156",                   # Arsenal Squad
    "6000 click 210 369",                   # Ray Parlour
    "8000 click 445 97",                    # Contract
    "10000 dump {d}/contract.bmp",
    "10500 click 583 97",                   # Transfer
    "12500 dump {d}/transfer.bmp",
    "13000 click 722 14",                   # Action
    "14500 click 680 267",                  # Edit player
    "16500 dump {d}/editor.bmp",
    "17000 quit",
]
# The rows of money in each picture (classic numbers, y from and to): only these may differ.
MONEY_ROWS = {
    "contract": [(180, 196), (235, 251)],   # Wages, Bonuses
    "transfer": [(180, 196)],               # Value
    "editor": [(182, 208), (480, 507)],     # Wage; Balance and Funds
}
RATE = re.compile(r"recomp: money scale (\d+): rate ([\d.]+)")


def play(program, game, user, out, scale):
    os.makedirs(out, exist_ok=True)
    fixtures.write_settings(user, "classic", [("money_scale", scale), ("player_editor", "on")])
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, STEPS), {}, 900)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    return r.returncode, r.stderr


def changed_rows(a, b):
    from PIL import Image, ImageChops
    box = ImageChops.difference(Image.open(a).convert("RGB"), Image.open(b).convert("RGB"))
    px = box.load()
    return sorted({y for y in range(box.height) for x in range(box.width) if px[x, y] != (0, 0, 0)})


def main(argv):
    args = argv[1:]
    folder = fixtures.DEFAULT_DIR
    if "--dir" in args:
        i = args.index("--dir")
        folder = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(p) for p in args)
    work = tempfile.mkdtemp(prefix="cm0102-money-")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    users = {}
    for n in ("off", "7"):
        users[n] = os.path.join(work, n, "user")
        os.makedirs(users[n])
        shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(users[n], "start.sav"))
    with ThreadPoolExecutor(2) as pool:
        runs = {n: pool.submit(play, program, game, users[n], os.path.join(out, n), n) for n in users}
        logs = {n: runs[n].result() for n in runs}

    code, log = logs["7"]
    rates = RATE.findall(log)
    check(code == 0 and bool(rates) and all(f == "7" and float(r) == 7.0 for f, r in rates),
          f"7: the rates are 7 times the game's ({rates})")
    code, log = logs["off"]
    check(code == 0 and not RATE.search(log), "off: the rates are the game's")

    for name, bands in MONEY_ROWS.items():
        a, b = (os.path.join(out, n, name + ".bmp") for n in ("off", "7"))
        if not (os.path.exists(a) and os.path.exists(b)):
            check(False, f"{name}: both pictures exist")
            continue
        rows = changed_rows(a, b)
        outside = [y for y in rows if not any(lo <= y <= hi for lo, hi in bands)]
        check(rows and not outside, f"{name}: the amounts change ({len(rows)} rows), nothing else "
                                    f"({len(outside)} rows outside: {outside[:5]})")
    print(f"pictures in {out}; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
