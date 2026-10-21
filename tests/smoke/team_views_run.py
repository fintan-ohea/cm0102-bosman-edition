#!/usr/bin/env python3
"""Team views check (spec 2026-10-15-features-0.4.0, slice 3): the screen opens from the Assistant drop-down
of the own squad and of another club's squad, its tabs switch, and Back returns to the squad screen.

Usage:
    python3 tests/smoke/team_views_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Four runs from start.sav (tests/fixtures/fixtures.py), at the same time:
    classic, widescreen  the tour: own squad (Arsenal) > Assistant > Team views > Compare players > Best
                         XI > Back; Premier Division table > Liverpool > Squad > Assistant > Team views >
                         Back. Screen dumps of each step in <out>/<mode>.
    same-on, same-off    the same-save check (classic): squad_views = on and off, the same steps (SAME_STEPS: both
                         drop-downs open and close, the screen stays shut), then one game day; the runtime
                         saves (RECOMP_SAVE_AFTER_DAYS). The two saves must be equal byte for byte.

Checks, from the screen log (RECOMP_SCREEN_LOG=1) and the runtime's line "team views for club":
    - the screen opens twice, for two clubs, the first the own club
    - after each open the top screen is a new one, and after each Back it is the squad screen 0x454900
    - the tabs switch (the screen is built three times on the own club: Best XI, Compare players, Best XI)
    - the two saves are equal
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

SQUAD = 0x454900
TABLE = 0x48fcd0     # the competition screen, Table tab

# Positions for 800x600; "| x y" gives the widescreen position where the game does not scale (popups).
TO_SQUAD = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",         # Arsenal Squad
    "wait screen:0x454900",
]
TOUR = TO_SQUAD + [
    "1500 click 592 135",                   # Assistant
    "3000 click 540 241 | 928 288",         # Team views
    "5000 dump {d}/1-best-xi.bmp",
    "5500 move 675 97",
    "6000 click 675 97",                    # Compare players
    "8000 dump {d}/2-compare.bmp",
    "8500 move 272 97",
    "9000 click 272 97",                    # Best XI again (two tabs since 0.6.0)
    "11000 dump {d}/3-best-xi-again.bmp",
    "12000 click 358 573",                  # Back
    "wait screen:0x454900",
    "1500 dump {d}/4-back.bmp",
    "2500 click 584 527",                   # Premier Division
    "wait screen:0x48fcd0",
    "1500 click 215 462",                   # Liverpool
    "3500 click 169 97",                    # Squad
    "5500 click 592 135",                   # Assistant
    "7000 dump {d}/5-other-popup.bmp",
    "8000 click 540 159 | 928 205",         # Team views, the only item
    "10000 dump {d}/6-other-best-xi.bmp",
    "11000 click 358 573",                  # Back
    "wait screen:0x454900",
    "1500 dump {d}/7-other-back.bmp",
]
# The same-save steps: both drop-downs open and close, so both runs build the screens with and without
# the new items, but the Team views screen stays shut. Opening any screen changes later days in the
# original too (a visit of the league table does), so the check keeps the steps equal.
SAME_STEPS = TO_SQUAD + [
    "1500 click 592 135",                   # Assistant
    "3000 press escape",
    "4000 click 584 527",                   # Premier Division
    "wait screen:0x48fcd0",
    "1500 click 215 462",                   # Liverpool
    "3500 click 169 97",                    # Squad
    "5500 click 592 135",                   # Assistant (with squad_views = off: no button there)
    "7000 press escape",
    "8000 move 400 30",
]
DAYS = ["wait continue", "500 press space", "wait day:15.7.01"]


def scaled(mode, d, steps):
    return smoke_run.scaled_steps(mode, d, steps)


def run(program, game, folder, mode, out, steps, options, days=None):
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-views-")
    user = os.path.join(work, "user")
    os.makedirs(user)
    shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, mode, options)
    end = "600000 quit" if days is not None else "3000 quit"
    all_steps = scaled(mode, out, fixtures.LOAD_STEPS) + scaled(mode, out, steps) + [end]
    env = {"RECOMP_SCREEN_LOG": "1"}
    if days is not None:
        env["RECOMP_SAVE_AFTER_DAYS"] = str(days)
    r, took = fixtures.run(program, game, mode, user, all_steps, env, 1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    save = os.path.join(user, smoke_run.SAVE_NAME)
    if os.path.exists(save):
        shutil.copyfile(save, os.path.join(out, smoke_run.SAVE_NAME))
    for f in sorted(os.listdir(out)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(out, f), "--out",
                            os.path.join(out, f[:-4] + ".png")], capture_output=True)
    return r.returncode, r.stderr


def check_tour(name, code, log):
    """Problems of one tour run, from its log."""
    problems = []
    if code != 0:
        problems.append(f"{name}: exit code {code}")
    opens = re.findall(r"team views for club (0x[0-9a-f]+) team (\d+)", log)
    if len(opens) != 2 or opens[0][0] == opens[1][0]:
        problems.append(f"{name}: the screen opened for {opens}, expected two clubs")
    builds = len(re.findall(r"recomp: team views built", log))
    # The screens in log order, without repeats.
    screens = []
    for m in re.finditer(r"recomp: (team views for club|state at \d+ ms, date \S+: screen (0x[0-9a-f]+))", log):
        if m.group(2) is None:
            screens.append("open")
        elif not screens or screens[-1] != m.group(2):
            screens.append(m.group(2))
    for i, s in enumerate(screens):
        if s == "open":
            after = [x for x in screens[i + 1:] if x != "open"]
            if len(after) < 2 or int(after[0], 16) in (SQUAD, TABLE) or int(after[1], 16) != SQUAD:
                problems.append(f"{name}: after an open the screens were {after[:2]}, expected the views, then "
                                f"the squad screen")
    if builds < 4:
        problems.append(f"{name}: the screen was built {builds} times, expected 3 on the own club and the other club")
    return problems


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
    program, game, out = (os.path.abspath(a) for a in args)
    jobs = {
        "classic": ("classic", TOUR, [], None),
        "widescreen": ("widescreen", TOUR, [], None),
        "same-on": ("classic", SAME_STEPS + DAYS, [("squad_views", "on")], 1),
        "same-off": ("classic", SAME_STEPS + DAYS, [("squad_views", "off")], 1),
    }
    with ThreadPoolExecutor(len(jobs)) as pool:
        futures = {name: pool.submit(run, program, game, folder, mode, os.path.join(out, name), steps, opts, days)
                   for name, (mode, steps, opts, days) in jobs.items()}
        results = {name: f.result() for name, f in futures.items()}
    problems = []
    for name in ("classic", "widescreen"):
        problems += check_tour(name, *results[name])
    saves = [os.path.join(out, n, smoke_run.SAVE_NAME) for n in ("same-on", "same-off")]
    if not all(os.path.exists(s) for s in saves):
        problems.append("same-save: a run made no save")
    elif open(saves[0], "rb").read() != open(saves[1], "rb").read():
        problems.append("same-save: the saves with squad_views on and off differ")
    for p in problems:
        print("FAIL:", p)
    print("team views: " + ("FAIL" if problems else "all checks passed") + f" (dumps in {out})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
