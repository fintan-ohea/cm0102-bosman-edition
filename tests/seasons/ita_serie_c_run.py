#!/usr/bin/env python3
"""Italy's single Serie C of three groups (0.6.0 slice 20b, 2014/15 on): groups A, B and C on the objects of C1/A,
C1/B and C2/A, groups of 20 with 38 match days, the Serie C chain and regroup, and the change from C1 and C2.

Usage:
    python3 tests/seasons/ita_serie_c_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Italy. The runs:
    2014      October 2014, changing rules off, 2014/15 and 2015/16 (the Serie C start code runs at the change)
    2014-c2   the same with the extra division (Serie C2 selected: the original makes C2/A, C2/B and C2/C)
    change    Autumn 2013, changing rules on, 2013/14 to the end of 2014/15 (C1 and C2 end)
    change-c2 the same with the extra division
    load      October 2014, changing rules off: the game saves 270 days after the start (April 2015); a new program
              loads that save and plays 120 days, across the season change into 2015/16 (the module makes group C
              again from its saved file)
    load-c2   the same with the extra division (the original's C2 objects load; the module makes none)
    c2-two    September 2010 ODB, changing rules on, the extra division, 2010/11 to the end of 2014/15: Serie C2 of
              three groups, then two groups from 2011/12 to 2013/14 (Q529: C2/C ends), then the Serie C
    c2-two-off September 2011 ODB, changing rules off, the extra division, 2011/12 and 2012/13: two C2 groups of 20
              from the new game on
    c2-load   the same start: the game saves 270 days after the start (April 2012); a new program loads that save
              and plays 120 days, across the season change into 2012/13
The checks: the jump ends with code 0 and shows no message box of the game; the report has the groups (files
ita_ser_c1a, ita_ser_c1b and ita_ser_c2a, Q520) at the end of each season of the era, each with the clubs of its row (20 in 2014/15;
with changing rules off every later season keeps the start season's counts, Q232), and C2/B and C2/C with
year 2999 at the end of a season of the era (Q623); the log has the 38 match days of each group of 20, the regroup line of each
season change into the era, and the news sentence of the change in the change runs. In each season change
of the era 4 clubs go up to Serie B and Serie B sends 4 down (Q627), and group C is made once (no line "starts
again"). The two-group runs (Q529): C2/A and C2/B hold the clubs of their rows at the end of each two-group season
(20 and 20 in 2011/12, the 21 of C2/B is 20; 18 and 18 in 2012/13 and 2013/14), C2/C has year 2999, each
season change into a two-group season regroups C2 into the two counts, and in each season change C1 sends down as
many clubs as C2 sends up. The load runs: both programs end with code 0 and no message box, and after the load the regroup into
2015/16 and three groups of 20 with 38 match days.
"""
import concurrent.futures
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402
import club_world_check as cwc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
GROUPS = ["ita_ser_c1a", "ita_ser_c1b", "ita_ser_c2a"]      # Q520: the rows take the files of their objects
NEWS = "Serie C1 and Serie C2 end: the Serie C is now one division of three groups of 20, 20 and 20 clubs."


def comps(report):
    """{(season, phase): {file: (clubs, year)}} of the comp lines of a report."""
    out = {}
    with open(report, encoding="latin-1") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if c[0] == "comp" and len(c) > 10:
                out.setdefault((int(c[1]), c[2]), {})[c[5]] = (int(c[10]), int(c[9]))
    return out


def check_report(name, report, seasons, want):
    fails = []
    if not os.path.exists(report):
        return [f"{name}: no report"]
    got = comps(report)
    for s in seasons:
        end = got.get((s, "end"), {})
        for g, file in enumerate(GROUPS):
            got_clubs = end.get(file, (None, None))[0]
            if got_clubs != want[s][g]:
                fails.append(f"{name}: {file} at the end of {s} holds {got_clubs} clubs, want {want[s][g]}")
        for old in ("ita_ser_c2b", "ita_ser_c2c"):
            if old in end and end[old][1] != 2999:  # Q623: an ended league waits for year 2999
                fails.append(f"{name}: {old} has year {end[old][1]} at the end of {s} (it ends in the era)")
    return fails


def check_moves(name, log):
    """Q627: each season change of the era sends 4 clubs up to Serie B and Serie B sends 4 down."""
    fails = []
    for up, down in re.findall(r"serie c: (\d+) up to Serie B \(groups A \d+, B \d+, C \d+\), (\d+) down", log):
        if up != down or up != "4":
            fails.append(f"{name}: {up} clubs up to Serie B and {down} down (want 4 and 4)")
    if "group C starts again" in log:
        fails.append(f"{name}: group C started twice in one season change")
    # O259 row A21 (Q628): the Coppa Italia Serie C takes no Serie D club.
    if "Coppa Italia Serie C takes" in log:
        fails.append(f"{name}: the Coppa Italia Serie C took Serie D clubs")
    return fails


def check_ccup(name, log):
    """O259 row A21 (Q628): in the era the Coppa Italia Serie C takes the clubs of the three groups: 16 groups of 3
    and the rest seeded into the knockout of 32."""
    if not re.search(r"Coppa Italia Serie C \d+: (4[89]|5\d|6[0-4]) clubs, 16 groups of 3 and \d+ seeds", log):
        return [f"{name}: no Coppa Italia Serie C of the groups' clubs"]
    return []


def run_season(name, extra):
    def run(program, game, work):
        code, report = rrc.run(program, game, 2015, 2014, os.path.join(DB, "October 2014"), ["Italy"], extra, work,
                               7200, None, {"RECOMP_SEASON_CHANGE_LOG": "1"})
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        fails += check_report(name, report, [2014, 2015], {2014: [20, 20, 20], 2015: [20, 20, 20]})
        if len(re.findall(r"serie c: .*: 20 clubs, 38 match days", log)) < 3:
            fails.append(f"{name}: fewer than 3 groups of 20 with 38 match days")
        if not re.search(r"serie c 2015/16: 60 clubs in groups of 20, 20 and 20", log):
            fails.append(f"{name}: no regroup into 2015/16")
        if not re.search(r"serie c: \d+ up to Serie B", log):
            fails.append(f"{name}: no line of the clubs that go up to Serie B")
        fails += check_ccup(name, log)
        return fails + check_moves(name, log)
    return run


def run_change(name, extra):
    def run(program, game, work):
        code, report = rrc.run(program, game, 2014, 2013, os.path.join(DB, "Autumn 2013"), ["Italy"], extra, work,
                               10800, {"changing_rules": "on"}, {"RECOMP_SEASON_CHANGE_LOG": "1"})
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        fails += check_report(name, report, [2014], {2014: [20, 20, 20]})
        if not re.search(r"serie c 2014/15: 60 clubs in groups of 20, 20 and 20", log):
            fails.append(f"{name}: no regroup into 2014/15")
        if NEWS not in log:
            fails.append(f"{name}: no news sentence of the change")
        if not extra and not re.search(r"serie c: group C 2014: table and fixtures of 20 clubs", log):
            fails.append(f"{name}: group C made in the change did not get its table and fixtures once")
        # Q529: a 2013/14 start with the flag is a two-group C2 season, so C2/C ends at the new game; its one club
        # goes to the Serie D, and at the change into the era the ended C2/C holds no club.
        if extra and not re.search(r"serie c: [^\n]*(girone C|C2/C): its club goes to the Serie D", log):
            fails.append(f"{name}: the one club of the ended C2/C did not go to the Serie D at the new game")
        if extra and not re.search(r"serie c: [^\n]*(girone C|C2/C) \(ended\): 0 clubs to the Serie C", log):
            fails.append(f"{name}: the ended C2/C held clubs at the change into the era")
        fails += check_ccup(name, log)
        return fails + check_moves(name, log)
    return run


def run_load(name, extra):
    def run(program, game, work):
        code, _ = rrc.run(program, game, 2015, 2014, os.path.join(DB, "October 2014"), ["Italy"], extra, work, 7200,
                          None, {"RECOMP_SAVE_AFTER_DAYS": "270"})
        first = os.path.join(work, "user", "samesave.sav")
        found, log = ufr.boxes(work)
        if code != 0 or not os.path.exists(first):
            return [f"{name}: the first run made no save (code {code}, {work})"]
        fails = [f"{name}: message box {b} before the save" for b in found]
        again = os.path.join(work, "again")
        os.makedirs(again, exist_ok=True)
        shutil.copyfile(first, os.path.join(again, "serie_c.sav"))
        r, saved = cwc.load_and_play(program, game, os.path.join(again, "serie_c.sav"), again, 120, timeout=2400)
        out = r.stderr
        if r.returncode != 0 or not saved:
            fails.append(f"{name}: the save did not load and play on (code {r.returncode}, {again})")
        if "message box" in out:
            fails.append(f"{name}: a message box after the load ({again}/stderr.txt)")
        made = re.search(r"new competition ita_ser_c2a made for \d+ \(entry 21, record \d+\) from the save", out)
        if not extra and not made:
            fails.append(f"{name}: the module did not make group C again from the save")
        if extra and "group C object made" in out:
            fails.append(f"{name}: the module made a group C object although the original's exists")
        if not re.search(r"serie c 2015/16: 60 clubs in groups of 20, 20 and 20", out):
            fails.append(f"{name}: no regroup into 2015/16 after the load")
        if len(re.findall(r"serie c: .* 2015: 20 clubs, 38 match days", out)) < 3:
            fails.append(f"{name}: fewer than 3 groups of 20 with 38 match days after the load")
        fails += check_ccup(name, out)
        return fails + check_moves(name, out)
    return run


C2_FILES = ["ita_ser_c2a", "ita_ser_c2b"]


def run_two_load(name):
    """A two-group season saved in April 2012 (270 days after a September 2011 start, changing rules off, the extra
    division) loads and plays 120 days across the June change into 2012/13 (Q529)."""
    def run(program, game, work):
        code, _ = rrc.run(program, game, 2012, 2011, os.path.join(DB, "September 2011 ODB"), ["Italy"], ["c2"], work,
                          7200, None, {"RECOMP_SAVE_AFTER_DAYS": "270"})
        first = os.path.join(work, "user", "samesave.sav")
        found, _ = ufr.boxes(work)
        if code != 0 or not os.path.exists(first):
            return [f"{name}: the first run made no save (code {code}, {work})"]
        fails = [f"{name}: message box {b} before the save" for b in found]
        again = os.path.join(work, "again")
        os.makedirs(again, exist_ok=True)
        shutil.copyfile(first, os.path.join(again, "serie_c2.sav"))
        r, saved = cwc.load_and_play(program, game, os.path.join(again, "serie_c2.sav"), again, 120, timeout=2400)
        out = r.stderr
        if r.returncode != 0 or not saved:
            fails.append(f"{name}: the save did not load and play on (code {r.returncode}, {again})")
        if "message box" in out:
            fails.append(f"{name}: a message box after the load ({again}/stderr.txt)")
        if not re.search(r"serie c2 2012/13: 40 clubs in groups of 20 and 20", out):
            fails.append(f"{name}: no regroup into 2012/13 after the load")
        for down, up in re.findall(r"serie c2: (\d+) down from Serie C1 \(A \d+, B \d+\), (\d+) up from Serie C2", out):
            if down != up:
                fails.append(f"{name}: {down} clubs down from Serie C1 and {up} up from Serie C2 after the load")
        if len(re.findall(r"serie c: .* 2012: 20 clubs, 38 match days", out)) < 2:
            fails.append(f"{name}: fewer than 2 C2 groups of 20 with 38 match days after the load")
        return fails
    return run


def check_two(name, report, log, want):
    """Q529: the two C2 groups at the end of each season of want ({season: [a, b]}), C2/C ended, the regroup line
    of each season change into a two-group season and the balance of C1 and C2."""
    fails = []
    got = comps(report) if os.path.exists(report) else {}
    for s, counts in want.items():
        end = got.get((s, "end"), {})
        for file, n in zip(C2_FILES, counts):
            if end.get(file, (None, None))[0] != n:
                fails.append(f"{name}: {file} at the end of {s} holds {end.get(file, (None, None))[0]} clubs, want {n}")
        if end.get("ita_ser_c2c", (None, 2999))[1] != 2999:
            fails.append(f"{name}: ita_ser_c2c has year {end['ita_ser_c2c'][1]} at the end of {s} (it ends, Q529)")
    # The season changes into a two-group season (a game of changing rules off starts in one: no change into it).
    for s in sorted(want)[1 if name.endswith("-off") else 0:]:
        a, b = want[s]
        if not re.search(rf"serie c2 {s}/{(s + 1) % 100:02d}: {a + b} clubs in groups of {a} and {b}", log):
            fails.append(f"{name}: no regroup into {s}/{(s + 1) % 100:02d} ({a} and {b})")
    for down, up in re.findall(r"serie c2: (\d+) down from Serie C1 \(A \d+, B \d+\), (\d+) up from Serie C2", log):
        if down != up:
            fails.append(f"{name}: {down} clubs down from Serie C1 and {up} up from Serie C2")
    if len(re.findall(r"serie c2: \d+ down from Serie C1", log)) < len(want) - 1:
        fails.append(f"{name}: fewer chain lines of two C2 groups than season changes")
    return fails


def run_two(name, year, last, database, rules, want):
    def run(program, game, work):
        code, report = rrc.run(program, game, last, year, os.path.join(DB, database), ["Italy"], ["c2"], work,
                               18000, {"changing_rules": rules}, {"RECOMP_SEASON_CHANGE_LOG": "1"})
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        fails += check_two(name, report, log, want)
        first = min(want)
        if want[first][0] == 20 and len(re.findall(rf"serie c: .* {first}: 20 clubs, 38 match days", log)) < 2:
            fails.append(f"{name}: fewer than 2 C2 groups of 20 with 38 match days in {first}")
        if last >= 2014:
            fails += check_report(name, report, [2014], {2014: [20, 20, 20]})
            if not re.search(r"serie c 2014/15: 60 clubs in groups of 20, 20 and 20", log):
                fails.append(f"{name}: no regroup into 2014/15")
            fails += check_moves(name, log)
        return fails
    return run


RUNS = {
    "2014": run_season("2014", []),
    "2014-c2": run_season("2014-c2", ["c2"]),
    "change": run_change("change", []),
    "change-c2": run_change("change-c2", ["c2"]),
    "load": run_load("load", []),
    "load-c2": run_load("load-c2", ["c2"]),
    "c2-two": run_two("c2-two", 2010, 2014, "September 2010 ODB", "on", {2011: [20, 20], 2012: [18, 18], 2013: [18, 18]}),
    "c2-load": run_two_load("c2-load"),
    "c2-two-off": run_two("c2-two-off", 2011, 2012, "September 2011 ODB", "off", {2011: [20, 20], 2012: [20, 20]}),
}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="ita-serie-c-")
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(RUNS[n], program, game, os.path.join(keep, n)) for n in only}
        for n, fut in futures.items():
            got = fut.result()
            print(f"{n}: {'ok' if not got else 'FAIL'} ({os.path.join(keep, n)})")
            fails += got
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
