#!/usr/bin/env python3
"""48-team World Cup check (0.6.0 slice 29): the 2026 finals, their qualifying and the play-off tournament in a
season jump.

Usage:
    python3 tests/seasons/season_wc48_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>] [--reuse]

Each run is a season jump of tests/seasons/rules_report_check.py (England, the quick result engine) with the
test logs RECOMP_SEASON_INTL_LOG=1 and RECOMP_SEASON_CHANGE_LOG=1. The runs:
    2023    October 2023, 2023/24 to 2026/27, changing rules on: the full 2026 qualifying, the play-off
            tournament, the finals of 48, the host news, then the 2030 cycle
    2025    December 2025, 2025/26 to 2027/28, changing rules on: the finals of 48 from a start after the
            qualifying began (the draw fill, Q388), then the 2030 cycle (the six hosts, the sections of 2028)
    off     October 2023, 2023/24 to 2026/27, changing rules off: the rule edition of the start season is the
            2026 World Cup, so the finals have 48 teams too
    2030    December 2025, 2025/26 to 2029/30, changing rules on: the 2030 cycle to its draw (O265, O259 A11): no
            host in qualifying (three CONMEBOL hosts left out, two European paths), UEFA 14, CONMEBOL 3, CONCACAF 6,
            CAF 8, AFC 8, OFC 1 and 2 from the tournament, a draw of 48 with no fill
    save    December 2025, changing rules on: the jump saves after SAVE_AFTER days (about 22 June 2026, in the
            group stage of the finals) and stops; a second game loads that save in a new user folder and plays to
            the end of 2027/28: no message box, the round of 32 and the 2030 cycle in the loaded game's log

The checks (each run):
    - the jump ends with code 0 and the report holds the end of the last season
    - the log lines of LINES (the draw of 48 in 12 groups, the round of 32, the host slot of the United States,
      the seeding day)
    - QUALIFIERS: the qualifiers that reach add_qualified_team for the 2026 World Cup, by confederation (the three
      hosts come from the host row): UEFA 16, CONMEBOL 6, CONCACAF 3, CAF 9, AFC 8, OFC 1 at least, 2 more from
      the play-off tournament, 45 in all
    - no draw after a full qualifying is short (log "short after a full qualifying"), no play-off without a team
    - on: the host news of the 2026 World Cup names three hosts
    - no 2026 qualifying line names Russia (Q288, Q365), and the European pool of 2026 is logged
A run takes about 25 to 35 minutes. --jobs (default 2) runs that many at the same time. --reuse checks the
logs already in the --keep folder again without a new run.
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import season_intl_run as sir  # noqa: E402
import season_nl_run as snr  # noqa: E402
import smoke_run  # noqa: E402

RUNS = {
    "2023": (2023, 2026, "October 2023", "on"),
    "2025": (2025, 2027, "December 2025", "on"),
    "off": (2023, 2026, "October 2023", "off"),
    "save": (2025, 2027, "December 2025", "on"),
    "2030": (2025, 2029, "December 2025", "on"),
}
SAVE_AFTER = 345
LINES = {
    "2023": ["world_cup 2026 draw of 48 teams in 12 groups", "world_cup 2026 round of 32",
             "world_cup 2026 seeds on 1 April", "wc play-off 2026: seeds", "wc play-off 2026: finals",
             "wc qualifying 2026: path A", "wc qualifying 2026: Europe draws", "world_cup 2026 host",
             "wc qualifying 2026: Europe pool of 54 nations (Russia out"],
    "2025": ["world_cup 2026 draw of 48 teams in 12 groups", "world_cup 2026 round of 32",
             # A start after the qualifying began: the real draw and play-off tournament (O259 A9, O274).
             "world_cup 2026: the real draw, 48 of 48 teams",
             "wc play-off 2026: seeds Democratic Republic of Congo, Iraq; semi-finals New Caledonia v Jamaica, "
             "Bolivia v Surinam",
             "world_cup 2030 has 12 stages and 48 slots", "world_cup 2030 host Uruguay",
             "wc qualifying 2030: Europe draws"],
    "off": ["world_cup 2026 draw of 48 teams in 12 groups", "world_cup 2026 round of 32"],
    # The 2030 shape (O265, O259 A11): six hosts, none in qualifying, 42 qualifiers.
    "2030": ["world_cup 2030 draw of 48 teams in 12 groups", "wc qualifying 2030: CONMEBOL leaves out 3 hosts",
             "wc qualifying 2030: path B:"],
}
QUALIFIERS = {"2023": 2026, "off": 2026, "2030": 2030}
BASE = {"UEFA": 16, "CONMEBOL": 6, "CONCACAF": 3, "CAF": 9, "AFC": 8, "OFC": 1}
# 2030 (O265): each confederation keeps its 2026 total with its hosts counted in it.
BASES = {2026: BASE, 2030: {"UEFA": 14, "CONMEBOL": 3, "CONCACAF": 6, "CAF": 8, "AFC": 8, "OFC": 1}}
HOSTS = {2030: ["Morocco", "Portugal", "Spain", "Argentina", "Paraguay", "Uruguay"]}
NEWS = {"2023": "The 2026 World Cup plays in"}
BASE_ENV = {"RECOMP_STAFF_RESERVE": "23000", "RECOMP_SEASON_INTL_LOG": "1", "RECOMP_SEASON_CHANGE_LOG": "1"}
QUALIFIER = re.compile(r"season intl: world_cup (\d+) qualifier (.*) \((\w+|\?)\)$")


def check(name, start, last, rules, work):
    report = os.path.join(work, "report.tsv")
    if not os.path.exists(report):
        return [f"{name}: no report ({work})"]
    seasons, _ = rrc.read_report(report)
    fails = []
    if not seasons or seasons[-1]["season"] != last or seasons[-1]["phase"] != "end":
        fails.append(f"{name}: the report does not end with the end of {last}/{(last + 1) % 100:02d}")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    for text in LINES.get(name, []):
        if not any(text in x for x in log):
            fails.append(f"{name}: no log line {text!r}")
    for x in log:
        if "short after a full qualifying" in x and "world_cup" in x:
            fails.append(f"{name}: a short list after a full qualifying: {x.strip()}")
        if "season intl: wc play-off" in x and " none" in x:
            fails.append(f"{name}: a play-off without a team: {x.strip()}")
        if "no room for the knockout" in x or "no knockout stage" in x or "no paths" in x:
            fails.append(f"{name}: {x.strip()}")
    for x in log:
        if "Russia" in x and "Europe pool of" not in x and \
                ("wc qualifying 2026" in x or "world_cup 2026 qualifier" in x or "wc play-off 2026" in x):
            fails.append(f"{name}: Russia in the 2026 qualifying: {x.strip()}")
    # A fill, a qualifier in already and a full World Cup are faults in every run and every year (O334).
    for x in log:
        if "season intl: world_cup " in x and " filled" in x and not x.rstrip().endswith(" 0 filled"):
            fails.append(f"{name}: a World Cup draw takes teams from the fill: {x.strip()}")
        if "is in already" in x or "world_cup is full" in x:
            fails.append(f"{name}: {x.strip()}")
    year = QUALIFIERS.get(name)
    if year:
        count = {}
        teams = []
        for x in log:
            m = QUALIFIER.search(x)
            if m and int(m.group(1)) == year:
                count[m.group(3)] = count.get(m.group(3), 0) + 1
                teams.append(m.group(2))
        extra = 0
        base = BASES[year]
        for conf, want in base.items():
            got = count.get(conf, 0)
            if got < want or (conf == "UEFA" and got != want):
                fails.append(f"{name}: {conf} gives {got} qualifiers for {year}, the table {want}")
            extra += max(0, got - want)
        total = sum(base.values()) + 2
        if sum(count.values()) != total or extra != 2:
            fails.append(f"{name}: {sum(count.values())} qualifiers for {year} ({count}), {total} with 2 from the "
                         f"play-off tournament")
        if len(set(teams)) != len(teams):
            fails.append(f"{name}: a nation qualifies twice for {year}")
        for host in HOSTS.get(year, []):
            if host in teams:
                fails.append(f"{name}: the host {host} qualifies for {year} through qualifying")
        # Only the 2030 shape (O265) has two paths; 2026 keeps its four paths A to D.
        for x in log:
            if year in HOSTS and f"wc qualifying {year}: path C" in x:
                fails.append(f"{name}: {x.strip()}")
    want = NEWS.get(name)
    if want:
        news = [x for x in log if "season change: news" in x and want in x]
        if not news:
            fails.append(f"{name}: no host news {want!r}")
        elif "3 host nations" not in news[0]:
            fails.append(f"{name}: the host news does not name three hosts: {news[0].strip()}")
    return fails


def save_run(program, game, keep, reuse):
    """A save inside the finals of 48 and a load (slice 28's load_and_jump, season_nl_run.py)."""
    start, last, database, _ = RUNS["save"]
    base = os.path.join(keep, "save") if keep else tempfile.mkdtemp(prefix="cm0102-season-wc48-save-")
    first, second = os.path.join(base, "save-a"), os.path.join(base, "save-b")
    db = sir.database_folder(database)
    if not reuse:
        os.makedirs(base, exist_ok=True)
        save = os.path.join(first, "user", smoke_run.SAVE_NAME)
        if not os.path.exists(save):
            code, _ = rrc.run(program, game, last, start, db, ["England"], [], first, 7200, {"changing_rules": "on"},
                              dict(BASE_ENV, RECOMP_SAVE_AFTER_DAYS=str(SAVE_AFTER)))
            if code != 0 or not os.path.exists(save):
                return [f"save: the first game did not save (code {code}, {first})"]
        snr.BASE_ENV = BASE_ENV                          # the intl test log in the loaded game
        code = snr.load_and_jump(program, game, db, save, second, last, 7200)
        if code != 0:
            return [f"save: the loaded game ended with code {code} ({second})"]
    log_a = open(os.path.join(first, "stderr.txt"), encoding="latin-1").read().split("\n")
    log_b = open(os.path.join(second, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails = [f"save: the loaded game: {x.strip()}" for x in log_b if "recomp: message box from" in x]
    for x in log_a + log_b:                              # O334: in every run
        if "season intl: world_cup " in x and " filled" in x and not x.rstrip().endswith(" 0 filled"):
            fails.append(f"save: a World Cup draw takes teams from the fill: {x.strip()}")
        if "is in already" in x or "world_cup is full" in x:
            fails.append(f"save: {x.strip()}")
    if not any("world_cup 2026 draw of 48 teams in 12 groups" in x for x in log_a):
        fails.append("save: the first game made no draw of 48 before the save")
    if any("world_cup 2026 round of 32" in x for x in log_a):
        fails.append("save: the first game reached the round of 32 before the save (save earlier)")
    if not any("season jump: done" in x for x in log_b):
        fails.append(f"save: the loaded game did not reach the end of {last}/{(last + 1) % 100:02d}")
    for text in ("world_cup 2026 round of 32", "world_cup 2030 has 12 stages and 48 slots"):
        if not any(text in x for x in log_b):
            fails.append(f"save: no log line {text!r} in the loaded game")
    return fails


def one(program, game, name, keep, reuse):
    if name == "save":
        return save_run(program, game, keep, reuse)
    start, last, database, rules = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-wc48-{name}-")
    if not reuse:
        code, _ = rrc.run(program, game, last, start, sir.database_folder(database), ["England"], [], work, 10800,
                          {"changing_rules": rules}, BASE_ENV)
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    return check(name, start, last, rules, work)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(RUNS)
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else None
    reuse = "--reuse" in argv
    fails = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for r in pool.map(lambda n: one(program, game, n, keep, reuse), only):
            fails += r
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
