#!/usr/bin/env python3
"""Player editor check (fork; docs/specs/2026-10-24-player-editor-spec.md, slice 1).

Usage:
    python3 tests/smoke/player_editor_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures):
    edit    player_editor = on. Opens a profile from the Arsenal squad, opens Edit player in its Action
            drop-down, clicks Finishing + and Acceleration +, and Cancel. Opens the box again, clicks current
            ability +10, potential ability +, Finishing +, Pace +, Condition Full and Morale +, and Save. Then
            Continue Game: the runtime saves the game a day later (RECOMP_SAVE_AFTER_DAYS=1) as samesave.sav.
            Wage +10% and Contract +1y are clicked before Save too.
    load    player_editor = on. Loads that save and opens the box for the same player.
    cure    player_editor = on. Opens the box of an injured player (Van Bronckhorst), clicks Cure and Save, then
            Continue Game to the test save.
    cured   loads that save and opens the box of the same player.
    off     player_editor = off. Opens the Action drop-down and clicks where Edit player would be.

Checks:
    - edit: the box opens; Cancel gives back the values of the first opening, and the second opening shows
      them; Save shows current ability +10, potential ability +1, Pace +1 (a stored attribute) and a higher
      Finishing (a computed one); the save has the part bosman_editor.dat with the edited mark.
    - edit: Save also shows condition 100% and morale +1, a wage 10% higher and a contract a year longer.
    - load: the load reads the mark, and the box shows the values of the Save.
    - cure: the player is injured when the box opens and fit after Cure; cured: still fit after the load.
    - off: the box never opens.
The editor writes "recomp: player editor <opened|cancelled|saved> staff <id> ca <n> pa <n> values <31 values>".
Pictures in <out>/<run>/: box.bmp, profile.bmp.
"""
import os
import re
import shutil
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402
import savfile  # noqa: E402

# Positions for 800x600 (classic). The box: current ability +10 at 420 132, potential ability + at 682 132;
# Condition Full at 334 165, Morale + at 523 165, Cure at 712 165; Wage +10% at 421 195, Contract +1y at
# 710 195; the attribute + buttons at x 334 (first column) and 524 (second), rows from y 225 every 25; Save
# 300 524, Cancel 590 524. The profile's Action button at 722 14, Edit player the 11th item at 680 267.
PROFILE = ["1000 click 45 165",          # the manager's menu
           "2500 click 150 156",         # Arsenal Squad
           "6000 click 210 369"]         # a player of the squad list
ACTION, EDIT = "click 722 14", "click 680 267"
EDIT_STEPS = fixtures.LOAD_STEPS + PROFILE + [
    "9500 " + ACTION,
    "12000 " + EDIT,
    "14000 dump {d}/box.bmp",
    "14500 click 524 225",               # Finishing +
    "16000 click 334 225",               # Acceleration +
    "17500 click 590 524",               # Cancel
    "20000 " + ACTION,
    "22500 " + EDIT,
    "25000 click 420 132",               # current ability +10
    "26500 click 682 132",               # potential ability +
    "28000 click 524 225",               # Finishing +
    "29500 click 524 450",               # Pace +
    "30000 click 334 165",               # Condition Full
    "30300 click 523 165",               # Morale +
    "30600 click 421 195",               # Wage +10%
    "30800 click 710 195",               # Contract +1y
    "31000 click 300 524",               # Save
    "34000 dump {d}/profile.bmp",
    "35000 click 45 120",                # Continue Game: the test save follows a day later
    "45000 click 45 120",
    "55000 click 45 120",
    "65000 quit",
]
# The test save is not at a news screen, so the load waits a fixed time in place of "wait first-news".
LOAD_STEPS = fixtures.LOAD_STEPS[:-1] + [
    "11000 click 45 165",
    "12500 click 150 156",
    "16000 click 210 369",
    "19500 " + ACTION,
    "22000 " + EDIT,
    "24000 dump {d}/box.bmp",
    "25000 quit",
]
INJURED = "6000 click 260 346"          # Van Bronckhorst, "Inj" in the squad list of start.sav
CURE_STEPS = fixtures.LOAD_STEPS + PROFILE[:2] + [INJURED,
    "9500 " + ACTION,
    "12000 " + EDIT,
    "14000 click 712 165",               # Cure
    "15500 dump {d}/box.bmp",
    "16000 click 300 524",               # Save
    "19000 click 45 120",
    "29000 click 45 120",
    "39000 click 45 120",
    "49000 quit",
]
CURED_STEPS = LOAD_STEPS[:LOAD_STEPS.index("16000 click 210 369")] + ["16000 click 260 346"] + \
    LOAD_STEPS[LOAD_STEPS.index("16000 click 210 369") + 1:]
OFF_STEPS = fixtures.LOAD_STEPS + PROFILE + ["9500 " + ACTION, "12000 " + EDIT, "14000 dump {d}/after.bmp", "15000 quit"]

LINE = re.compile(r"recomp: player editor (opened|cancelled|saved) staff (-?\d+) ca (-?\d+) pa (-?\d+) values((?: -?\d+){31})"
                  r" condition (-?\d+) morale (-?\d+) injury (\d+) (-?\d+) wage (?:none|(-?\d+) end (-?\d+) (-?\d+))")
FINISHING, PACE = 12, 21                 # attribute indices (docs/research/2026-10-24-player-editor-attributes.md)


def lines(log):
    return [(m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), [int(v) for v in m.group(5).split()],
             int(m.group(6)), int(m.group(7)), int(m.group(8)),
             None if m.group(10) is None else (int(m.group(10)), int(m.group(11)), int(m.group(12))))
            for m in LINE.finditer(log)]


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
    work = tempfile.mkdtemp(prefix="cm0102-player-editor-")
    start = os.path.join(folder, "start.sav")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    edit_user = new_user(work, "edit", start)
    off_user = new_user(work, "off", start)
    cure_user = new_user(work, "cure", start)
    with ThreadPoolExecutor(3) as pool:
        cure = pool.submit(play, program, game, cure_user, os.path.join(out, "cure"), CURE_STEPS,
                           [("player_editor", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        edit = pool.submit(play, program, game, edit_user, os.path.join(out, "edit"), EDIT_STEPS,
                           [("player_editor", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        off = pool.submit(play, program, game, off_user, os.path.join(out, "off"), OFF_STEPS,
                          [("player_editor", "off")])
        code, log = edit.result()
        seen = lines(log)
        kinds = [s[0] for s in seen]
        check(kinds[:4] == ["opened", "cancelled", "opened", "saved"], f"edit: the box opens, cancels, opens, saves ({kinds})")
        saved = os.path.join(edit_user, "samesave.sav")
        check(code == 0 and os.path.exists(saved), f"edit: the run ends with code {code} and saves the game")
        if kinds[:4] == ["opened", "cancelled", "opened", "saved"]:
            first, cancelled, again, after = seen[:4]
            check(cancelled[2:] == first[2:] and again[2:] == first[2:],
                  "edit: Cancel gives back every value, and the box opens with them again")
            # Current ability above potential raises potential with it (editor.c, step_ability).
            ca = min(first[2] + 10, 200)
            check(after[2] == ca and after[3] == min(max(first[3], ca) + 1, 200),
                  f"edit: current ability {first[2]} -> {after[2]}, potential ability {first[3]} -> {after[3]}")
            check(after[4][PACE - 1] == min(first[4][PACE - 1] + 1, 20),
                  f"edit: Pace {first[4][PACE - 1]} -> {after[4][PACE - 1]}")
            check(after[4][FINISHING - 1] > first[4][FINISHING - 1] or first[4][FINISHING - 1] == 20,
                  f"edit: Finishing {first[4][FINISHING - 1]} -> {after[4][FINISHING - 1]}")
            check(after[5] == 100 and after[6] == min(first[6] + 1, 20),
                  f"edit: condition {first[5]} -> {after[5]}, morale {first[6]} -> {after[6]}")
            # wage, end year, end day of the year; +10% steps at least WAGE_STEP (editor.c).
            fc, ac = first[8], after[8]
            check(fc is not None and ac == (fc[0] + max(fc[0] // 10, 100), fc[1] + 1, fc[2]),
                  f"edit: contract {fc} -> {ac} (wage +10%, a year longer)")
        if os.path.exists(saved):
            try:
                part = bytes(savfile.Sav(saved).read_member("bosman_editor.dat"))
            except KeyError:
                part = None
            check(part is not None and len(part) == 16 and part[8] == 1,
                  f"edit: the save has bosman_editor.dat with the mark ({part!r})")
        code, log = off.result()
        check(code == 0 and "reached first-news" in log and "player editor" not in log,
              "off: the run loads the game, ends with code 0, and the box never opens")
        code, log = cure.result()
        cured = lines(log)
        check(code == 0 and [c[0] for c in cured[:2]] == ["opened", "saved"] and cured[0][7] == 2 and cured[1][7] == 0,
              f"cure: injured when the box opens, fit after Cure ({[(c[0], c[7]) for c in cured]})")
        cure_save = os.path.join(cure_user, "samesave.sav")
        check(os.path.exists(cure_save), "cure: the game is saved")

    if os.path.exists(os.path.join(edit_user, "samesave.sav")) and len(seen) >= 4:
        load_user = new_user(work, "load", os.path.join(edit_user, "samesave.sav"))
        code, log = play(program, game, load_user, os.path.join(out, "load"), LOAD_STEPS, [("player_editor", "on")])
        loaded = lines(log)
        check(code == 0 and "the save is an edited game" in log, "load: the load reads the edited mark")
        check(bool(loaded) and loaded[0][0] == "opened" and loaded[0][1:] == seen[3][1:],
              f"load: the box shows the values of the Save ({loaded[0][1:] if loaded else None})")
    if os.path.exists(os.path.join(cure_user, "samesave.sav")):
        cured_user = new_user(work, "cured", os.path.join(cure_user, "samesave.sav"))
        code, log = play(program, game, cured_user, os.path.join(out, "cured"), CURED_STEPS, [("player_editor", "on")])
        again = lines(log)
        check(code == 0 and bool(again) and again[0][1] == cured[0][1] and again[0][7] == 0,
              f"cured: the player is still fit after the load ({[(c[1], c[7]) for c in again]})")
    print(f"pictures in {out}/<run>/; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
