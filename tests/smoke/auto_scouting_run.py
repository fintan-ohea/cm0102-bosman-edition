#!/usr/bin/env python3
"""Automatic scouting check (fork; docs/specs/2026-10-24-auto-scouting-spec.md, slices 1 to 4).

Usage:
    python3 tests/smoke/auto_scouting_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Runs, in classic mode, from start.sav (tests/fixtures), each a holiday of 14 days (RECOMP_SAVE_AFTER_DAYS=14):
    on, again   auto_scouting = on, twice.
    off         auto_scouting = off.
    low         auto_scouting = on, the reports written as by scouts of judging 1 (RECOMP_SCOUT_JUDGING=1, test only).
    load        loads the save of "on", returns from the holiday and opens Scout reports in the Assistant drop-down
                of the Arsenal squad, then the first report.

Checks:
    - on: every scout of the club gets a duty; reports are filed, at most 5 a game day; in every report the true
      value of each attribute is inside its range; a news item goes out on Monday 16.7.01.
    - on: the save has the part bosman_scouting.dat; the width of each range follows the scout's judging
      (1 + (20 - judging) * 4 / 19).
    - on: every report has the player's value, and a wage when he has a contract; more than a quarter of the
      reports are of a role the squad needs (two roles of eight, ranked up when the reports are chosen).
    - low: every range is 6 numbers wide (span 5) and still holds the true value; the ranges are wider than on's.
    - again: the save is the same as that of on (the reports draw no random numbers of the game).
    - off: no duty, no report, and no part in the save.
    - load: the load reads the reports and the box opens with them.
Pictures in <out>/load/: list.bmp, report.bmp.
"""
import os
import re
import shutil
import struct
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402
import savfile  # noqa: E402

# The holiday as smoke_run.STEPS: the manager's menu (45 165), Go on Holiday (150 427), Please Confirm: Yes.
HOLIDAY = fixtures.LOAD_STEPS + ["1000 click 45 165", "3000 click 150 427", "5500 click 705 572", "900000 quit"]
# The save is in the holiday: the manager's menu asks "Return from holiday?" (Yes 705 572). Then the squad, the
# Assistant button (592 135), Scout reports (its sixth item with training_assistant off, 545 262), and the first
# report's name (293 155).
LOAD = fixtures.LOAD_STEPS[:-1] + ["11000 click 45 165", "12500 click 705 572", "15000 click 45 165",
                                   "16500 click 150 156", "19000 click 592 135", "20500 click 545 262",
                                   "22500 dump {d}/list.bmp", "23000 click 293 155", "25000 dump {d}/report.bmp",
                                   "26000 quit"]
DUTY = re.compile(r"recomp: scouting duty manager \d+ slot (\d+) scout (\d+)")
REPORT = re.compile(r"recomp: scouting report player (\d+) by scout (\d+) \(judging (\d+), span (\d+), true value "
                    r"inside (\d+) of (\d+)\)")
MONEY = re.compile(r"recomp: scouting report .*?: stars \d+/\d+, [^:]*? (\(need\) )?est \d+ best \d+ second \d+ "
                   r"value (-?\d+) wage (-?\d+)")
NEWS = re.compile(r"recomp: scouting news: (\d+) new reports")
DAY = re.compile(r"recomp: (?:human turn|day) ")
REPORT_SIZE = 108   # scouting.c Report: ids, date, 8 bytes, 4 scores, 2 ids, 32 + 32 range ends, value, wage
LO, HI = 36, 68
DUTIES = 16 * 10 * 8


def play(program, game, user, out, steps, options, env=None):
    os.makedirs(out, exist_ok=True)
    fixtures.write_settings(user, "classic", options)
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, steps), env or {}, 1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    return r.returncode, r.stderr


def part_of(save):
    try:
        return bytes(savfile.Sav(save).read_member("bosman_scouting.dat"))
    except KeyError:
        return None


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
    work = tempfile.mkdtemp(prefix="cm0102-scouting-")
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
    users = {n: user(n, start) for n in ("on", "again", "off", "low")}
    env = {"RECOMP_SAVE_AFTER_DAYS": "14"}
    with ThreadPoolExecutor(4) as pool:
        runs = {n: pool.submit(play, program, game, users[n], os.path.join(out, n), HOLIDAY,
                               [("auto_scouting", "off" if n == "off" else "on")],
                               dict(env, RECOMP_SCOUT_JUDGING="1") if n == "low" else env) for n in users}
        logs = {n: runs[n].result() for n in runs}

    code, log = logs["on"]
    duties = DUTY.findall(log)
    check(code == 0 and len({slot for slot, _ in duties}) >= 1, f"on: scouts get duties ({len(duties)} given)")
    reports = REPORT.findall(log)
    check(len(reports) > 0 and len(reports) <= 5 * 15, f"on: {len(reports)} reports in 15 game days, at most 5 a day")
    check(all(int(inside) == int(total) for *_, inside, total in reports),
          "on: in every report each true value is inside its range")
    check(all(int(span) == 1 + (20 - int(j)) * 4 // 19 for _, _, j, span, _, _ in reports),
          "on: each report's span follows its scout's judging")
    money = MONEY.findall(log)
    check(len(money) == len(reports) and all(int(v) > 0 and int(w) != 0 for _, v, w in money),
          f"on: every report has a value and a wage ({money[:3]})")
    need = sum(bool(n) for n, _, _ in money)
    check(money and need * 4 > len(money), f"on: {need} of {len(money)} reports are of a role the squad needs")
    width_on = sum(int(span) for _, _, _, span, _, _ in reports) / max(len(reports), 1)
    news = NEWS.findall(log)
    check(bool(news) and int(news[0]) > 0, f"on: news items go out ({news})")
    saved = os.path.join(users["on"], "samesave.sav")
    part = part_of(saved) if os.path.exists(saved) else None
    check(part is not None and len(part) >= 16 + DUTIES, f"on: the save has bosman_scouting.dat ({part and len(part)} bytes)")
    if part:
        n = struct.unpack_from("<I", part, 8)[0]
        spans = []
        for i in range(n):
            r = part[16 + DUTIES + i * REPORT_SIZE:16 + DUTIES + (i + 1) * REPORT_SIZE]
            judging = r[16]
            lo, hi = r[LO:LO + 32], r[HI:HI + 32]
            want = 1 + (20 - judging) * 4 // 19
            spans.append(all(hi[k] - lo[k] == want or (lo[k] == 1 or hi[k] == 20) for k in range(1, 32)))
        check(n > 0 and all(spans), f"on: the {n} saved reports keep ranges as wide as their scout's judging gives")

    again = os.path.join(users["again"], "samesave.sav")
    check(os.path.exists(again) and os.path.exists(saved) and open(again, "rb").read() == open(saved, "rb").read(),
          "again: the save is the same as that of on")

    code, log = logs["off"]
    off_save = os.path.join(users["off"], "samesave.sav")
    check(code == 0 and "scouting duty" not in log and "scouting report" not in log and
          os.path.exists(off_save) and part_of(off_save) is None, "off: no duty, no report, no part in the save")

    code, log = logs["low"]
    low = REPORT.findall(log)
    check(code == 0 and low and all(int(j) == 1 and int(span) == 5 and int(i) == int(t) for _, _, j, span, i, t in low)
          and 5 > width_on, f"low: {len(low)} reports of judging 1, span 5 (on: {width_on:.2f}), true values inside")

    if os.path.exists(saved):
        code, log = play(program, game, user("load", saved), os.path.join(out, "load"), LOAD, [("auto_scouting", "on")])
        m = re.search(r"recomp: scouting: the save has (\d+) reports", log)
        o = re.search(r"recomp: scout reports opened: (\d+) reports", log)
        check(code == 0 and bool(m) and bool(o) and int(o.group(1)) >= int(m.group(1)) > 0,
              f"load: the load reads the reports and the box opens ({m and m.group(1)}, {o and o.group(1)})")
    print(f"pictures in {out}/load/; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
