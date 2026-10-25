#!/usr/bin/env python3
"""Club editor check (fork; docs/specs/2026-10-25-club-editor-spec.md).

Usage:
    python3 tests/smoke/club_editor_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures):
    edit    club_editor = on. Opens the Arsenal club screen and its Edit Club tab, clicks Balance +, Transfer
            funds +10m, Reputation -1k, Training facilities -5, Status +, Board confidence +, Capacity -5k,
            Expansion limit +5k and Change on heating, roof and ground, then Finances & Info and Edit Club again.
            Then Continue Game: the runtime saves the game a day later (RECOMP_SAVE_AFTER_DAYS=1).
    load    club_editor = on. Loads that save and opens the Edit Club tab.
    off     club_editor = off. Opens the club screen and clicks where Edit Club would be.

Checks:
    - edit: the tab opens with the club's values; each click changes its value by its step (the seats follow the
      capacity down), the tab survives a move to Finances & Info and back, and the run has no error box.
    - load: the load reads the edited mark and the funds adjustment, and the tab shows the new reputation,
      facilities, status, stadium and ground.
    - off: the club screen has no Edit Club tab: the click opens Finances & Info and the tab never draws.
Pictures in <out>/<run>/: before.bmp, after.bmp, tab.bmp.
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

# Positions for 800x600 (classic). Six tabs on top from x 100, 115 wide: Finances & Info 617 97, Edit Club 732 97.
# The left rows (x: big down 242, down 275, up 386, big up 419) at y 166 Balance, 191 Transfer funds, 248
# Reputation, 273 Training facilities, 298 Status, 323 Board confidence; the right rows (big down 587, big up 764,
# Change 752) at y 166 Capacity, 191 Seated, 216 Expansion limit, 241 heating, 266 roof, 291 ground.
CLUB = ["1000 click 45 165",               # the manager's menu
        "3000 click 150 156",              # Arsenal Squad: the club screen
        "6000 click 732 97"]               # Edit Club
EDIT_STEPS = fixtures.LOAD_STEPS + CLUB + [
    "8000 dump {d}/before.bmp",
    "8500 click 386 166",                  # Balance +
    "9000 click 419 191",                  # Transfer funds +10m
    "9500 click 242 248",                  # Reputation -1k
    "10000 click 242 273",                 # Training facilities -5
    "10500 click 386 298",                 # Status +
    "11000 click 386 323",                 # Board confidence +
    "11500 click 587 166",                 # Capacity -5k
    "12000 click 764 216",                 # Expansion limit +5k
    "12500 click 752 241",                 # Under-soil heating: Change
    "13000 click 752 266",                 # Retractable roof: Change
    "13500 click 752 291",                 # Ground: Change
    "14500 click 617 97",                  # Finances & Info
    "16500 click 732 97",                  # Edit Club
    "18500 dump {d}/after.bmp",
    "19000 click 45 120",                  # Continue Game: the test save follows a day later
    "29000 click 45 120",
    "39000 click 45 120",
    "49000 quit",
]
# The test save is not at a news screen, so the load waits a fixed time in place of "wait first-news".
LOAD_STEPS = fixtures.LOAD_STEPS[:-1] + [
    "11000 click 45 165",
    "13000 click 150 156",
    "16000 click 732 97",
    "18000 dump {d}/tab.bmp",
    "19000 quit",
]
OFF_STEPS = fixtures.LOAD_STEPS + CLUB + ["8000 dump {d}/tab.bmp", "9000 quit"]

OPENED = re.compile(r"recomp: club editor opened: club (\d+)((?: [a-z]+ -?\d+)+)")
CHANGE = re.compile(r"recomp: club editor: club \d+ ([a-z]+) (-?\d+) -> (-?\d+)")


def values(m):
    words = m.group(2).split()
    return {words[i]: int(words[i + 1]) for i in range(0, len(words), 2)}


def play(program, game, user, out, steps, options, env=None):
    os.makedirs(out, exist_ok=True)
    fixtures.write_settings(user, "classic", options)
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, steps), env or {}, 600)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    return r.returncode, r.stderr


def new_user(work, name, save):
    user = os.path.join(work, name, "user")
    os.makedirs(user)
    shutil.copyfile(save, os.path.join(user, "start.sav"))
    return user


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
    work = tempfile.mkdtemp(prefix="cm0102-club-editor-")
    start = os.path.join(folder, "start.sav")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    edit_user, off_user = new_user(work, "edit", start), new_user(work, "off", start)
    with ThreadPoolExecutor(2) as pool:
        edit = pool.submit(play, program, game, edit_user, os.path.join(out, "edit"), EDIT_STEPS,
                           [("club_editor", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        off = pool.submit(play, program, game, off_user, os.path.join(out, "off"), OFF_STEPS, [("club_editor", "off")])
        code, log = edit.result()
        opened = [values(m) for m in OPENED.finditer(log)]
        changes = {f: (int(a), int(b)) for f, a, b in CHANGE.findall(log)}
        check(code == 0 and len(opened) >= 2 and "Error" not in log, f"edit: the tab opens ({len(opened)} builds), "
                                                                     f"no error box, code {code}")
        first = opened[0] if opened else {}
        want = {
            "balance": first.get("balance", 0) + 1000000,
            "reputation": max(first.get("reputation", 0) - 1000, 1),
            "training": max(first.get("training", 0) - 5, 1),
            "status": min(first.get("status", 0) + 1, 3),
            "confidence": min(first.get("confidence", 0) + 100, 10000),
            "capacity": max(first.get("capacity", 0) - 5000, 0),
            "expansion": first.get("expansion", 0) + 5000,
            "heating": int(not first.get("heating", 0)),
            "roof": int(not first.get("roof", 0)),
            "owns": int(not first.get("owns", 0)),
        }
        for field, v in want.items():
            got = changes.get(field)
            check(got is not None and got[0] == first.get(field) and got[1] == v,
                  f"edit: {field} {first.get(field)} -> {v} ({got})")
        funds = changes.get("funds")
        check(funds is not None and funds[1] - funds[0] == 10000000, f"edit: transfer funds +10m ({funds})")
        last = opened[-1] if opened else {}
        check(last.get("seated", -1) <= last.get("capacity", -2) and last.get("capacity") == want["capacity"],
              f"edit: the seats follow the capacity down ({last.get('seated')} of {last.get('capacity')}), and the "
              f"tab shows the changes after Finances & Info")
        saved = os.path.join(edit_user, "samesave.sav")
        check(os.path.exists(saved), "edit: the game is saved a day later")
        code, log = off.result()
        check(code == 0 and "club editor" not in log and "Error" not in log,
              "off: no Edit Club tab, and the click where it would be opens Finances & Info")

    if os.path.exists(os.path.join(edit_user, "samesave.sav")) and opened:
        load_user = new_user(work, "load", os.path.join(edit_user, "samesave.sav"))
        code, log = play(program, game, load_user, os.path.join(out, "load"), LOAD_STEPS, [("club_editor", "on")])
        check(code == 0 and "the save is an edited game, 1 funds adjustments" in log,
              "load: the load reads the edited mark and the funds adjustment")
        loaded = [values(m) for m in OPENED.finditer(log)]
        keep = ("reputation", "training", "status", "capacity", "seated", "expansion", "heating", "roof", "owns")
        check(bool(loaded) and all(loaded[0].get(k) == last.get(k) for k in keep),
              f"load: the tab shows the saved values ({ {k: loaded[0].get(k) for k in keep} if loaded else None})")
    print(f"pictures in {out}/<run>/; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
