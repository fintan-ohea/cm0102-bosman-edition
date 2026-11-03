#!/usr/bin/env python3
"""Training assistant check (fork; docs/specs/2026-10-24-training-assistant-spec.md, slices 1 to 4).

Usage:
    python3 tests/smoke/training_assistant_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures):
    apply   training_assistant = on. Opens Training setup in the Assistant drop-down of the Arsenal squad, clicks
            Apply and Close, then Continue Game: the runtime saves the game a day later (RECOMP_SAVE_AFTER_DAYS=1).
    load    loads that save and opens the box again.
    off     training_assistant = off: opens the Assistant drop-down and clicks where Training setup would be.
    explain opens the box, Apply, the Explain tab.
    coaches opens the box, the Coaches tab, Apply suggestion.
    cycle   training_cycle = on. Opens the box on Saturday 14.7.01, Apply, Close, then goes on holiday; the runtime
            saves the game 8 days later.

Checks:
    - apply: slots 8 and 9 are unused when the box opens; Apply writes both and puts players on them; the box's
      workload equals the game's own (TRAINING_SCHEDULE::get_training_load 0x8b4f60) for both.
    - load: slots 8 and 9 hold Outfield and Keepers with their levels, and the players are still on them.
    - off: the box never opens.
    - explain: the tab draws (the run ends with code 0 and its picture exists).
    - coaches: the factors that the box works out equal the game's get_coaching_factor (0x8b41d0) for the coach
      table as it is and after Apply suggestion, and the suggestion has no fewer player-categories above 100.
    - cycle: Outfield goes Medium on Saturday at Apply, Intensive on Sunday 15.7 and Medium on Friday 20.7, in that
      order; the save holds Outfield with Medium levels (Saturday 21.7 is a Medium day).
Pictures in <out>/<run>/: box.bmp, applied.bmp, training.bmp.
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

# Positions for 800x600 (classic): the Assistant button at 592 135, Training setup its sixth item at 545 262; in
# the box Apply 300 524 and Close 590 524; the squad screen's Training button 307 527.
SQUAD = ["1000 click 45 165", "2500 click 150 156"]
APPLY_STEPS = fixtures.LOAD_STEPS + SQUAD + [
    "5000 click 592 135",
    "6500 click 545 262",
    "8500 dump {d}/box.bmp",
    "9000 click 300 524",
    "10500 dump {d}/applied.bmp",
    "11000 click 590 524",
    "13000 click 307 527",
    "16000 dump {d}/training.bmp",
    "17000 click 45 120",
    "27000 click 45 120",
    "37000 click 45 120",
    "47000 quit",
]
LOAD_STEPS = fixtures.LOAD_STEPS[:-1] + ["11000 click 45 165", "12500 click 150 156", "15000 click 592 135",
                                         "16500 click 545 262", "18500 dump {d}/box.bmp", "19000 quit"]
OFF_STEPS = fixtures.LOAD_STEPS + SQUAD + ["5000 click 592 135", "6500 click 545 262", "8500 dump {d}/after.bmp",
                                           "9000 quit"]
# The tabs at 258, 444 and 631 on y 112; Apply suggestion is the left button (300 524).
EXPLAIN_STEPS = fixtures.LOAD_STEPS + SQUAD + ["5000 click 592 135", "6500 click 545 262", "8500 click 300 524",
                                               "10000 click 444 112", "12000 dump {d}/explain.bmp", "13000 quit"]
COACHES_STEPS = fixtures.LOAD_STEPS + SQUAD + ["5000 click 592 135", "6500 click 545 262", "8500 click 631 112",
                                               "10500 dump {d}/coaches.bmp", "11000 click 300 524",
                                               "12500 dump {d}/applied.bmp", "13000 quit"]
# The holiday as in smoke_run.STEPS: the manager's menu, Go on Holiday, Please Confirm: Yes.
CYCLE_STEPS = fixtures.LOAD_STEPS + SQUAD + ["5000 click 592 135", "6500 click 545 262", "8500 click 300 524",
                                             "10000 click 590 524", "12000 click 45 165", "14000 click 150 427",
                                             "16500 click 705 572", "400000 quit"]
FACTORS = re.compile(r"factors(?:| now)((?: \d+/\d+){5}) \((worked out|suggested)/game\)")
ABOVE = re.compile(r"above 100: now (\d+), suggested (\d+)")
CYCLE = re.compile(r"recomp: training cycle: manager \d+ (\w+), Outfield (\w+)")
SLOT = re.compile(r"recomp: training assistant slot (\d) type (-?\d+) levels (\d+) (\d+) (\d+) (\d+) (\d+) name (.*)")
PLAYERS = re.compile(r"recomp: training assistant players on slot 8 (\d+), slot 9 (\d+)")
WORKLOAD = re.compile(r"recomp: training assistant (\w+) workload box (\d+) game (\d+)")
OUTFIELD = (25, 50, 25, 50, 0)
KEEPERS = (25, 25, 0, 25, 50)


def play(program, game, user, out, steps, options, env=None):
    os.makedirs(out, exist_ok=True)
    fixtures.write_settings(user, "classic", options)
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, steps), env or {}, 600)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    return r.returncode, r.stderr


def slots(log):
    """The slots of the first opening of the box: {slot: (type, levels, name)}."""
    out = {}
    for m in SLOT.finditer(log):
        k = int(m.group(1))
        if k in out:
            break
        out[k] = (int(m.group(2)), tuple(int(m.group(i)) for i in range(3, 8)), m.group(8).strip())
    return out


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
    work = tempfile.mkdtemp(prefix="cm0102-training-")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    def user(name, save):
        u = os.path.join(work, name, "user")
        os.makedirs(u)
        shutil.copyfile(save, os.path.join(u, "start.sav"))
        return u

    start = os.path.join(folder, "start.sav")
    apply_user, off_user = user("apply", start), user("off", start)
    cycle_user = user("cycle", start)
    with ThreadPoolExecutor(5) as pool:
        explain = pool.submit(play, program, game, user("explain", start), os.path.join(out, "explain"), EXPLAIN_STEPS,
                              [("training_assistant", "on")])
        coaches = pool.submit(play, program, game, user("coaches", start), os.path.join(out, "coaches"), COACHES_STEPS,
                              [("training_assistant", "on")])
        cycle = pool.submit(play, program, game, cycle_user, os.path.join(out, "cycle"), CYCLE_STEPS,
                            [("training_assistant", "on"), ("training_cycle", "on")], {"RECOMP_SAVE_AFTER_DAYS": "8"})
        apply = pool.submit(play, program, game, apply_user, os.path.join(out, "apply"), APPLY_STEPS,
                            [("training_assistant", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
        off = pool.submit(play, program, game, off_user, os.path.join(out, "off"), OFF_STEPS,
                          [("training_assistant", "off")])
        code, log = apply.result()
        before = slots(log)
        check(before.get(8, (0,))[0] == 7 and before.get(9, (0,))[0] == 7, f"apply: slots 8 and 9 are unused ({before})")
        m = re.search(r"outfield slot 8 written (\d+) players, keepers slot 9 written (\d+) players", log)
        check(bool(m) and int(m.group(1)) > 0 and int(m.group(2)) > 0, f"apply: both slots written with players ({m and m.groups()})")
        loads = WORKLOAD.findall(log)
        check(len(loads) == 2 and all(box == g for _, box, g in loads), f"apply: box workload = game workload ({loads})")
        saved = os.path.join(apply_user, "samesave.sav")
        check(code == 0 and os.path.exists(saved), f"apply: the run ends with code {code} and saves the game")
        code, log = off.result()
        check(code == 0 and "reached first-news" in log and "training assistant" not in log,
              "off: the run loads the game, ends with code 0, and the box never opens")
        code, log = explain.result()
        check(code == 0 and os.path.exists(os.path.join(out, "explain", "explain.bmp")), "explain: the tab draws")
        code, log = coaches.result()
        pairs = [(kind, [tuple(map(int, x.split("/"))) for x in vals.split()]) for vals, kind in FACTORS.findall(log)]
        check(code == 0 and len(pairs) == 2 and all(a == b for _, v in pairs for a, b in v),
              f"coaches: the box's factors equal the game's, now and after Apply suggestion ({pairs})")
        a = ABOVE.search(log)
        check(bool(a) and int(a.group(2)) >= int(a.group(1)), f"coaches: player-categories above 100 ({a and a.groups()})")
        code, log = cycle.result()
        steps = CYCLE.findall(log)
        want = [("Saturday", "Medium"), ("Sunday", "Intensive"), ("Friday", "Medium")]
        check(code == 0 and steps[:3] == want, f"cycle: Medium at Apply, Intensive from Sunday, Medium on Friday ({steps})")
        check(os.path.exists(os.path.join(cycle_user, "samesave.sav")), "cycle: the game is saved 8 days later")
    if os.path.exists(os.path.join(apply_user, "samesave.sav")):
        code, log = play(program, game, user("load", os.path.join(apply_user, "samesave.sav")),
                         os.path.join(out, "load"), LOAD_STEPS, [("training_assistant", "on")])
        after = slots(log)
        check(after.get(8) == (8, OUTFIELD, "Outfield") and after.get(9) == (8, KEEPERS, "Keepers"),
              f"load: slots 8 and 9 hold Outfield and Keepers ({after.get(8)}, {after.get(9)})")
        p = PLAYERS.search(log)
        check(code == 0 and bool(p) and int(p.group(1)) > 0 and int(p.group(2)) > 0,
              f"load: players are still on both ({p and p.groups()})")
    print(f"pictures in {out}/<run>/; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
