#!/usr/bin/env python3
"""Best XI check (spec 2026-10-15-features-0.4.0, slices 4 and 5; one card per slot with the best three since
Max's play test of 0.6.0): the Best XI tab of the Team views screen.

Usage:
    python3 tests/smoke/best_xi_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]
        [--mode classic|widescreen]...

One run from start.sav (Arsenal, 14.7.01) for each mode (classic and widescreen when no --mode is given),
at the same time. The tour: own squad > Assistant > Team views (Best XI) > Formation > 3-5-2 >
Back > Assistant > Pick best team > Assistant > Team views > Back; Premier Division table > Liverpool >
Squad > Assistant > Team views. Screen dumps of each step in <out>/<mode>.

The runtime writes a line for each draw of the eleven (RECOMP_VIEWS_LOG=1):
    recomp: best xi club <club> tactic <i> "<name>" masks <11 hex> staff <11 staff indices> fit <11>
        selection <11 staff indices of the club's stored selection> score <11 picker values> shown <11 numbers>
a line for the Formation list: recomp: best xi tactics <n> current <i>,
and for each draw, in 800x600 numbers, a pitch line and a line for each of the 11 cards:
    recomp: best xi pitch <x1> <y1> <x2> <y2> columns <n> most <n> card <w> x <h>
    recomp: best xi card pos <position> rect <x1> <y1> <x2> <y2> staff <1 to 3 staff> score <picker values>
        halves <half stars 1 to 10, 0 for no score> out <0, or 1 injured, 2 international, 3 another reason>
A picker value is the score times 1200 (picker.c value()); the score is rounded, 1 to 20 (Q156), and shows
as half stars: (score + 1) // 2, 0 for no score (Q420).

Checks:
    - the first draw is on the current tactic (tactic 0) and has 11 players, each fit 1 to 20
    - Formation lists two or more tactics; the second draw is another tactic with other masks, and the
      stored selection is the same before and after (view only)
    - after "Pick best team" the stored selection equals the eleven of the first draw (same day)
    - the other club's draw has 11 players of another club
    - no draw holds a player twice
    - each draw has 11 cards, each inside the pitch, no two overlapping; the first player of each card is a
      player of the eleven, each eleven player is first on one card, and a card holds no player twice (its
      2nd and 3rd are the next for that slot and may play elsewhere in the eleven, Q420)
    - each card's stars are the picker's score rounded (Q156), as half stars
    - no hidden attribute of another club's player feeds a score (hidden_check.py)
    - a Formation list that does not fit is paged: with RECOMP_VIEWS_FORMATION_PAGE=5 (a classic run), "More
      formations" opens page 2 and its first item draws the sixth tactic
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
import hidden_check  # noqa: E402

# Positions for 800x600; "| x y" gives the widescreen position where the game does not scale
# (popups).
TOUR = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 592 135",                   # Assistant
    "3000 click 540 241 | 928 288",  # Team views
    "5000 dump {d}/1-best-xi.bmp",
    "5500 click 712 25",                    # Formation
    "7000 dump {d}/2-formation-list.bmp",
    "8000 click 720 174 | 1168 189",  # 3-5-2, the seventh item
    "10000 dump {d}/3-other-formation.bmp",
    "11000 click 358 573",                  # Back
    "wait screen:0x454900",
    "1500 click 592 135",                   # Assistant
    "3000 click 540 159 | 928 205",  # Pick best team
    "5000 click 592 135",                   # Assistant
    "6500 click 540 241 | 928 288",  # Team views
    "8500 dump {d}/4-after-pick.bmp",
    "9500 click 358 573",                   # Back
    "wait screen:0x454900",
    "1500 click 584 527",                   # Premier Division
    "wait screen:0x48fcd0",
    "1500 click 215 462",                   # Liverpool
    "3500 click 169 97",                    # Squad
    "5500 click 592 135",                   # Assistant
    "7000 click 540 159 | 928 205",  # Team views, the only item
    "9000 dump {d}/5-other-club.bmp",
]

# The Formation list in pages of 5 (RECOMP_VIEWS_FORMATION_PAGE=5), classic positions: item k of the popup is
# at y = 48 + 21 k.
TOUR_PAGED = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156",                   # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 592 135",                   # Assistant
    "3000 click 540 241",                   # Team views
    "5000 click 712 25",                    # Formation
    "6500 dump {d}/1-page-1.bmp",
    "7500 click 720 153",                   # More formations (after the 5 tactics of page 1)
    "9500 click 712 25",                    # Formation
    "11000 dump {d}/2-page-2.bmp",
    "12000 click 720 69",                   # the first tactic of page 2 (after Earlier formations)
    "14000 dump {d}/3-page-2-chosen.bmp",
]

LINE = re.compile(r'recomp: best xi club (0x[0-9a-f]+) tactic (\d+) "([^"]*)" masks ([0-9a-f ]+) staff '
                  r'([-0-9 ]+) fit ([-0-9 ]+) selection ([-0-9 ]+) score ([-0-9 ]+) shown ([-0-9 ]+)')


def run(program, game, folder, mode, out, tour=None, env=None):
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-bestxi-")
    user = os.path.join(work, "user")
    os.makedirs(user)
    shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, mode, [])
    steps = smoke_run.scaled_steps(mode, out, fixtures.LOAD_STEPS) + smoke_run.scaled_steps(mode, out, tour or TOUR)
    steps.append("16000 quit" if tour else "12000 quit")          # after the last step of the last block
    r, took = fixtures.run(program, game, mode, user, steps,
                           dict({"RECOMP_SCREEN_LOG": "1", "RECOMP_VIEWS_LOG": "1"}, **(env or {})), 1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    for f in sorted(os.listdir(out)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(out, f), "--out",
                            os.path.join(out, f[:-4] + ".png")], capture_output=True)
    return r.returncode, r.stderr


def draws(log):
    """The draws of the eleven in log order, without repeats of the same draw."""
    out = []
    for m in LINE.finditer(log):
        d = {"club": m.group(1), "tactic": int(m.group(2)), "name": m.group(3), "masks": m.group(4).split(),
             "staff": [int(x) for x in m.group(5).split()], "fit": [int(x) for x in m.group(6).split()],
             "selection": [int(x) for x in m.group(7).split()], "score": [int(x) for x in m.group(8).split()],
             "shown": [int(x) for x in m.group(9).split()]}
        if not out or out[-1] != d:
            out.append(d)
    return out


PITCH = re.compile(r"recomp: best xi pitch (-?\d+) (-?\d+) (-?\d+) (-?\d+) columns")
CARD = re.compile(r"recomp: best xi card pos (\S+) rect (-?\d+) (-?\d+) (-?\d+) (-?\d+) staff ([-0-9 ]*) "
                  r"score ([-0-9 ]*) halves ([-0-9 ]*) out ([-0-9 ]*)")


def shown_score(value):
    """The score on the screen for a picker value (Q156): the score value / 1200, rounded, 1 to 20. 0 for a
    player with no score."""
    if value == -2:                         # PICK_UNSCORED
        return 0
    return max(1, min(20, (value + 600) // 1200))


def halves(value):
    shown = shown_score(value)
    return (shown + 1) // 2 if shown else 0


def score_problems(name, values, numbers):
    if len(values) != len(numbers) or any(shown_score(v) != n for v, n in zip(values, numbers)):
        return [f"{name}: the numbers {numbers} are not the picker's scores {values}"]
    return []


def check_layout(name, log):
    """Problems of the cards: each draw of the eleven, then its pitch line and 11 card lines."""
    problems = []
    events = sorted([(m.start(), "draw", m) for m in LINE.finditer(log)] +
                    [(m.start(), "pitch", m) for m in PITCH.finditer(log)] +
                    [(m.start(), "card", m) for m in CARD.finditer(log)], key=lambda e: e[0])
    groups, eleven, pitch = [], None, None
    for _, kind, m in events:
        if kind == "draw":
            eleven = [int(x) for x in m.group(5).split()]
        elif kind == "pitch":
            pitch = [int(v) for v in m.groups()]
            groups.append((eleven, pitch, []))
        elif groups:
            groups[-1][2].append(m.groups())
    if not groups:
        return [f"{name}: no pitch line in the log"]
    for eleven, (px1, py1, px2, py2), cards in groups:
        if len(cards) != 11:
            problems.append(f"{name}: {len(cards)} cards, expected 11")
            continue
        firsts, rects = [], []
        for pos, x1, y1, x2, y2, staff, values, hv, out in cards:
            x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
            staff, values, hv = ([int(v) for v in t.split()] for t in (staff, values, hv))
            if x1 < px1 or x2 > px2 or y1 < py1 or y2 > py2:
                problems.append(f"{name}: the card {pos} {x1} {y1} {x2} {y2} sticks out of the pitch")
            for o in rects:
                if x1 < o[2] and o[0] < x2 and y1 < o[3] and o[1] < y2:
                    problems.append(f"{name}: the card {pos} overlaps another card")
            rects.append((x1, y1, x2, y2))
            if not staff:
                problems.append(f"{name}: the card {pos} is empty")
                continue
            firsts.append(staff[0])
            if len(staff) != len(set(staff)):
                problems.append(f"{name}: the card {pos} holds a player twice: {staff}")
            if hv != [halves(v) for v in values]:
                problems.append(f"{name}: the card {pos} shows {hv} half stars for the scores {values}")
        if eleven is not None and sorted(firsts) != sorted(s for s in eleven if s >= 0):
            problems.append(f"{name}: the first players {sorted(firsts)} are not the eleven {sorted(eleven)}")
    return problems


def check(name, code, log):
    problems = check_layout(name, log)
    if code != 0:
        problems.append(f"{name}: exit code {code}")
    d = draws(log)
    for x in d:
        players = [s for s in x["staff"] if s >= 0]
        if len(players) != len(set(players)):
            problems.append(f"{name}: a player twice in {x}")
        if len(x["staff"]) != 11 or len(players) != 11:
            problems.append(f"{name}: not 11 players in {x}")
        if any(not 1 <= f <= 20 for f in x["fit"]):
            problems.append(f"{name}: a fit outside 1 to 20 in {x}")
        problems += score_problems(name + " eleven", x["score"], x["shown"])
    if len(d) < 4:
        return problems + [f"{name}: {len(d)} draws of the eleven, expected 4: {d}"]
    problems += hidden_check.check_flags(name, log) + hidden_check.check_draw_scores(name, log, d)
    first, other, after, club2 = d[0], d[1], d[2], d[-1]
    lists = re.findall(r"recomp: best xi tactics (\d+) current (\d+)", log)
    if not lists or int(lists[0][0]) < 2:
        problems.append(f"{name}: the Formation list is {lists}, expected 2 or more tactics")
    if first["tactic"] != 0:
        problems.append(f"{name}: the first draw is on tactic {first['tactic']}, expected the current one (0)")
    if other["club"] != first["club"] or other["tactic"] == first["tactic"] or other["masks"] == first["masks"]:
        problems.append(f"{name}: the change of formation did not draw another formation: {other}")
    if other["selection"] != first["selection"]:
        problems.append(f"{name}: the change of formation changed the stored selection")
    if after["club"] != first["club"] or after["selection"] != first["staff"]:
        problems.append(f"{name}: after Pick best team the selection is {after['selection']}, "
                        f"best XI was {first['staff']}")
    if after["staff"] != first["staff"]:
        problems.append(f"{name}: best XI changed after Pick best team: {after['staff']} against {first['staff']}")
    if club2["club"] == first["club"]:
        problems.append(f"{name}: the last draw is not of another club")
    return problems


def check_paged(code, log):
    """The Formation list in pages of 5: More formations, then the first tactic of page 2."""
    problems = []
    if code != 0:
        problems.append(f"paged: exit code {code}")
    d = draws(log)
    if len(d) < 2:
        return problems + [f"paged: {len(d)} draws of the eleven, expected 2 (before and after the choice)"]
    if d[0]["tactic"] != 0 or d[-1]["tactic"] != 5 or d[-1]["masks"] == d[0]["masks"]:
        problems.append(f"paged: the tactics drawn are {[x['tactic'] for x in d]}, expected 0 and then 5")
    return problems


def main(argv):
    args = argv[1:]
    folder = fixtures.DEFAULT_DIR
    modes = []
    while "--dir" in args or "--mode" in args:
        flag = "--dir" if "--dir" in args else "--mode"
        i = args.index(flag)
        if flag == "--dir":
            folder = args[i + 1]
        else:
            modes.append(args[i + 1])
        del args[i:i + 2]
    if len(args) != 3:
        print(__doc__)
        return 2
    modes = modes or ["classic", "widescreen"]
    program, game, out = (os.path.abspath(a) for a in args)
    with ThreadPoolExecutor(len(modes) + 1) as pool:
        futures = {m: pool.submit(run, program, game, folder, m, os.path.join(out, m)) for m in modes}
        paged = pool.submit(run, program, game, folder, "classic", os.path.join(out, "paged"), TOUR_PAGED,
                            {"RECOMP_VIEWS_FORMATION_PAGE": "5"})
        results = {m: f.result() for m, f in futures.items()}
        paged = paged.result()
    problems = []
    for m in modes:
        problems += check(m, *results[m])
    problems += check_paged(*paged)
    for p in problems:
        print("FAIL:", p)
    print("best xi: " + ("FAIL" if problems else "all checks passed") + f" (dumps in {out})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
