#!/usr/bin/env python3
"""Coach editor check (fork; docs/specs/2026-10-24-player-editor-spec.md, coach editor).

Usage:
    python3 tests/smoke/coach_editor_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures). The coach is Pat Rice, Arsenal's assistant manager, the first
row of the Coaches box of Arsenal's training screen:
    edit    player_editor = on. Opens his profile, opens Edit coach in its Action drop-down, clicks Coaching + and
            Working With Youngsters +, and Cancel. Opens the box again, clicks current ability +10, potential
            ability +, Coaching +, Working With Youngsters +, Wage +10% and Contract +1y, and Save. Then Continue
            Game: the runtime saves the game a day later (RECOMP_SAVE_AFTER_DAYS=1).
    load    player_editor = on. Loads that save and opens the box for the same coach.
    release player_editor = on. Opens the box, clicks Release coach twice (the first click asks), then Continue
            Game to the test save.
    off     player_editor = off. Opens the Action drop-down and clicks where Edit coach would be.

Checks:
    - edit: the profile has Edit coach and the box opens; Cancel gives back every value; Save shows current ability
      +10, potential ability at least as high, a higher Coaching (a computed attribute), Working With Youngsters +1
      (a stored one), a wage 10% higher and a contract a year longer; the save has the edited mark.
    - load: the load reads the mark, and the box shows the values of the Save.
    - release: the second click releases the coach: no club and no contract; the game plays a day and saves.
    - off: the box never opens.
The editor writes "recomp: coach editor <opened|cancelled|saved> staff <id> ca <n> pa <n> values <12 values> ...".
Pictures in <out>/<run>/: box.bmp, profile.bmp.
"""
import os
import re
import shutil
import sys
import tempfile
from datetime import date, timedelta
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402
import savfile  # noqa: E402

# Positions for 800x600 (classic). Arsenal Squad, Training at 307 527, its Coaches button at 592 135, the first
# coach at 172 233. The profile's Action button at 722 14, Edit coach the 6th item at 685 141. The box: current
# ability +10 at 420 132, potential ability + at 682 132; Wage +10% at 421 165, Contract +1y at 718 165; the
# attribute + buttons at x 434 (first column, attributes 1 to 6) and 713 (second, 7 to 12), rows from y 196 every
# 24; Save 251 524, Release coach 445 524, Cancel 638 524.
PROFILE = ["1000 click 45 165",          # the manager's menu
           "2500 click 150 156",         # Arsenal Squad
           "6000 click 307 527",         # Training
           "8000 click 592 135",         # Coaches
           "10500 click 172 233"]        # Pat Rice
ACTION, EDIT = "click 722 14", "click 685 141"
COACHING, YOUNGSTERS = "click 434 244", "click 713 316"
# Continue Game does not move on from inside the training screen: Back to the Coaches box, its Ok at 681 434, the
# training screen's Ok at 705 572, then Continue Game; the test save follows a day later.
LEAVE = ["click 360 572", "click 681 434", "click 705 572"]
CONTINUE = ["click 45 120"] * 3
EDIT_STEPS = fixtures.LOAD_STEPS + PROFILE + [
    "13500 " + ACTION,
    "15500 dump {d}/action.bmp",
    "16000 " + EDIT,
    "18000 dump {d}/box.bmp",
    "18500 " + COACHING,
    "20000 " + YOUNGSTERS,
    "21500 click 638 524",               # Cancel
    "24000 " + ACTION,
    "26500 " + EDIT,
    "29000 click 420 132",               # current ability +10
    "30500 click 682 132",               # potential ability +
    "32000 " + COACHING,
    "33500 " + YOUNGSTERS,
    "34000 click 421 165",               # Wage +10%
    "34500 click 718 165",               # Contract +1y
    "35000 dump {d}/edited.bmp",
    "35500 click 251 524",               # Save
    "38000 dump {d}/profile.bmp",
    "38500 " + LEAVE[0], "41000 " + LEAVE[1], "43500 " + LEAVE[2],
    "46000 " + CONTINUE[0], "56000 " + CONTINUE[1], "66000 " + CONTINUE[2],
    "76000 quit",
]


def later(steps, ms):
    """The steps ms later: a step's time counts from the last wait."""
    return [f"{int(t) + ms} {rest}" for t, rest in (step.split(" ", 1) for step in steps)]


# The test save is not at a news screen, so the load waits a fixed time in place of "wait first-news": the steps
# after the load start 10 s after the clicks of the Load Game screen.
LOAD_STEPS = fixtures.LOAD_STEPS[:-1] + later(PROFILE + [
    "13500 " + ACTION,
    "16000 " + EDIT,
    "18000 dump {d}/box.bmp",
    "18500 quit",
], 10000)
RELEASE_STEPS = fixtures.LOAD_STEPS + PROFILE + [
    "13500 " + ACTION,
    "16000 " + EDIT,
    "18000 click 445 524",               # Release coach
    "19500 dump {d}/asked.bmp",
    "20000 click 445 524",               # Click again to release
    "22500 dump {d}/profile.bmp",
    "23000 " + LEAVE[0], "25500 " + LEAVE[1], "28000 " + LEAVE[2],
    "30500 " + CONTINUE[0], "40500 " + CONTINUE[1], "50500 " + CONTINUE[2],
    "60500 quit",
]
OFF_STEPS = fixtures.LOAD_STEPS + PROFILE + ["13500 " + ACTION, "16000 " + EDIT, "18000 dump {d}/after.bmp",
                                             "19000 quit"]
LINE = re.compile(r"recomp: coach editor (opened|cancelled|saved) staff (-?\d+) ca (-?\d+) pa (-?\d+) "
                  r"values((?: -?\d+){12}) wage (?:none|(-?\d+) end (-?\d+) (-?\d+) type (\d+))")
RELEASED = re.compile(r"recomp: player editor: staff (-?\d+) released by club (-?\d+), now club (-?\d+), contract (\w+)")
COACHING_K, YOUNGSTERS_K = 3, 12          # attribute indices of 0x543fd0


def day(year, yday):
    """A contract date (year, day of the year from 0) as a date."""
    return date(year, 1, 1) + timedelta(days=yday)


def lines(log):
    return [(m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), [int(v) for v in m.group(5).split()],
             None if m.group(6) is None else (int(m.group(6)), int(m.group(7)), int(m.group(8)), int(m.group(9))))
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
    work = tempfile.mkdtemp(prefix="cm0102-coach-editor-")
    start = os.path.join(folder, "start.sav")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    edit_user, release_user, off_user = (new_user(work, n, start) for n in ("edit", "release", "off"))
    with ThreadPoolExecutor(3) as pool:
        edit = pool.submit(play, program, game, edit_user, os.path.join(out, "edit"), EDIT_STEPS,
                           [("player_editor", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        release = pool.submit(play, program, game, release_user, os.path.join(out, "release"), RELEASE_STEPS,
                              [("player_editor", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        off = pool.submit(play, program, game, off_user, os.path.join(out, "off"), OFF_STEPS, [("player_editor", "off")])

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
            ca = min(first[2] + 10, 200)
            check(after[2] == ca and after[3] >= ca, f"edit: current ability {first[2]} -> {after[2]}, "
                                                     f"potential ability {first[3]} -> {after[3]}")
            c0, c1 = first[4][COACHING_K - 1], after[4][COACHING_K - 1]
            check(c1 > c0 or c0 == 20, f"edit: Coaching {c0} -> {c1}")
            y0, y1 = first[4][YOUNGSTERS_K - 1], after[4][YOUNGSTERS_K - 1]
            check(y1 == min(y0 + 1, 20), f"edit: Working With Youngsters {y0} -> {y1}")
            fc, ac = first[5], after[5]
            before, later = (day(fc[1], fc[2]), day(ac[1], ac[2])) if fc and ac else (None, None)
            check(fc is not None and ac[0] == fc[0] + max(fc[0] // 10, 100) and
                  later == before.replace(year=before.year + 1),
                  f"edit: contract {fc} -> {ac}: wage +10%, end {before} -> {later}")
        if os.path.exists(saved):
            try:
                part = bytes(savfile.Sav(saved).read_member("bosman_editor.dat"))
            except KeyError:
                part = None
            check(part is not None and len(part) >= 16 and part[8] == 1, "edit: the save has the edited mark")
        code, log = off.result()
        check(code == 0 and "reached first-news" in log and "coach editor" not in log and "player editor" not in log,
              "off: the run loads the game, ends with code 0, and the box never opens")
        code, log = release.result()
        freed = RELEASED.findall(log)
        check(len(freed) == 1 and freed[0][2] == "-1" and freed[0][3] == "none",
              f"release: the second click releases the coach, with no club and no contract ({freed})")
        check(code == 0 and os.path.exists(os.path.join(release_user, "samesave.sav")) and "Error" not in log,
              f"release: the game plays a day and saves (code {code})")

    if os.path.exists(os.path.join(edit_user, "samesave.sav")) and len(seen) >= 4:
        load_user = new_user(work, "load", os.path.join(edit_user, "samesave.sav"))
        code, log = play(program, game, load_user, os.path.join(out, "load"), LOAD_STEPS, [("player_editor", "on")])
        loaded = lines(log)
        check(code == 0 and "the save is an edited game" in log, "load: the load reads the edited mark")
        check(bool(loaded) and loaded[0][0] == "opened" and loaded[0][1:] == seen[3][1:],
              f"load: the box shows the values of the Save ({loaded[0][1:] if loaded else None})")
    print(f"pictures in {out}/<run>/; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
