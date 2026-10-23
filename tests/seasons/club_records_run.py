#!/usr/bin/env python3
"""Game check of the new club records (0.6.0 slice 15b, doc docs/research/2026-10-19-club-records.md).

Usage:
    python3 tests/seasons/club_records_run.py <cm0102_recomp> <game folder> --year <Y> --database <folder>
        --nations <a,b,...> [--last <Y+1>] [--sources <folder>] [--keep <work folder>] [--timeout <s>]

It starts a new game in season Y with the season database and the nations given (England is always selected),
up to the first start report (rules_report_check.py --start-only), with RECOMP_RECORDS_DUMP so the game writes the
merged files of its load. --sources is the folder of the unpacked Update Team databases that the squads come from
(default: /Users/max/Projects/cm0102/test-fixtures/databases); the game knows each by its content.

--last Y+1 plays the season through to the start of the next one (no match engine) and checks that each new club
of a league is in a division at that start. Checks:
- the log names the records of the season, and every row of Seasons/club_records.tsv of Y is a new club;
- in the merged club.dat each new club has its name, short name, nation, stadium and the squad of sources.tsv
  less the people that the start database holds; every national team id moved by the number of new clubs;
- the staff rows of each squad name the new club (+0x39) and the club lists them (+0xd7);
- the rules report holds each new club in the division of its row (and rules_report_check finds no club_list
  difference that names it).
Prints PASS or FAIL lines. The merged files hold database content: they stay in the work folder (outside the repo).
"""
import csv
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import make_club_records  # noqa: E402
import rules_report_check as rrc  # noqa: E402

SOURCES = "/Users/max/Projects/cm0102/test-fixtures/databases"
MIN_SQUAD = 16          # 11 and 5 substitutes: the squad that a match needs (inference; the fill aims at 20)


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def tables(folder):
    """{table id: (file bytes, offset, count, format)} of index.dat."""
    idx = open(os.path.join(folder, "index.dat"), "rb").read()
    out = {}
    for i in range(8, len(idx) - 66, 67):
        name = idx[i:i + 51].split(b"\0")[0].decode("latin-1")
        t, n, off, fmt = struct.unpack_from("<iiii", idx, i + 51)
        out[t] = (name, off, n, fmt)
    return out


def rows(folder, t, size, files):
    name, off, n, _ = files[t]
    data = open(os.path.join(folder, name), "rb").read()
    return [data[off + k * size:off + (k + 1) * size] for k in range(n)]


def text(r, off, n=51):
    return r[off:off + n].split(b"\0")[0].decode("latin-1")


def merged_file(dump, data, name):
    p = os.path.join(dump, name)
    return dump if os.path.exists(p) else data


def check_dump(dump, data, season, records):
    """Differences between the merged files and the records of the season."""
    out = []
    files_d, files_m = tables(data), tables(dump)
    c, nat = files_d[0][2], files_d[1][2]
    if files_m[0][2] != c + len(records):
        return [f"club.dat holds {files_m[0][2]} clubs, the data {c} and the season {len(records)} new ones"]
    clubs = rows(dump, 0, 0x245, files_m)
    nats = rows(merged_file(dump, data, files_m[1][0]), 1, 0x245, files_m)
    for k, r in enumerate(nats):
        if struct.unpack_from("<i", r, 0)[0] != c + len(records) + k:
            out.append(f"national team row {k} has id {struct.unpack_from('<i', r, 0)[0]}, not {c + len(records) + k}")
            break
    staff_size = 0x9d if files_m[6][3] == 1 else 0x6e
    staff = rows(dump, 6, staff_size, files_m)
    n_staff_data = files_d[6][2]
    for k, rec in enumerate(records):
        r = clubs[c + k]
        cid = struct.unpack_from("<i", r, 0)[0]
        if cid != c + k or text(r, 4) != rec[3] or text(r, 0x38, 26) != rec[4]:
            out.append(f"new club {k}: id {cid}, name {text(r, 4)!r}, short {text(r, 0x38, 26)!r}; want {rec[3]!r}")
            continue
        last_div = struct.unpack_from("<i", r, 0x5b)[0]
        in_div = sum(1 for o in clubs[:c] if struct.unpack_from("<i", o, 0x57)[0] == last_div)
        if r[0x5f] <= in_div:
            out.append(f"{rec[3]}: last place +0x5f is {r[0x5f]}, inside the {in_div} clubs of its last division "
                       f"(review 2: the UEFA seeding and the cups read it)")
        squad = [s for s in struct.unpack_from("<50i", r, 0xd7) if s != -1]
        for s in squad:
            if not 0 <= s < len(staff) or struct.unpack_from("<i", staff[s], 0x39)[0] != cid:
                out.append(f"{rec[3]}: squad member {s} does not name the club")
                break
        print(f"  {rec[3]}: {len(squad)} players from {rec[10] or 'no source'} (the fill follows in the game)")
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    year = int(arg(argv, "--year"))
    database = os.path.abspath(arg(argv, "--database")) if "--database" in argv else None
    words = [x.strip() for x in arg(argv, "--nations", "England").split(",")]
    if words[0].lower() == "all":                   # Select All (every league; Korea and Russia are rows 17 on),
        nations = ["all"] + [n for n in rrc.NATION_ROWS  # less the rows named with a minus ("all,-denmark")
                             if "-" + n.lower() in {w.lower() for w in words}]
    else:
        nations = ["England"] + [n for n in rrc.NATION_ROWS if n.lower() in {w.lower() for w in words} and n != "England"]
    work = os.path.abspath(arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-records-")
    dump = os.path.join(work, "merged")
    os.makedirs(dump, exist_ok=True)
    env = {"RECOMP_RECORDS_SOURCES": arg(argv, "--sources", SOURCES), "RECOMP_RECORDS_DUMP": dump,
           "RECOMP_CLUB_LOG": "1"}
    last = int(arg(argv, "--last", year))
    code, report = rrc.run(program, game, last, year, database, nations, [], os.path.join(work, "run"),
                           int(arg(argv, "--timeout", 1800 if last == year else 10800)), env_more=env,
                           start_only=last == year)
    failed = [] if code == 0 else [f"the run ended with code {code}"]
    records = [r for r in make_club_records.build()[0] if r[0] == year]
    log = open(os.path.join(work, "run", "stderr.txt"), encoding="latin-1").read()
    if records and f"recomp: club records of {year}: {len(records)} clubs" not in log:
        failed.append(f"the log names no {len(records)} club records of {year}")
    # The squad fill (Q509) runs in the game after the moves: the log names each club under 16 players.
    for line in log.split("\n"):
        if line.startswith("recomp: club records: ") and f"fewer than {MIN_SQUAD}" in line:
            failed.append(line[len("recomp: club records: "):])
        if line.startswith("recomp: club records: squad fill of"):
            print("  " + line[len("recomp: club records: "):])
    data = database or os.path.join(game, "Data")
    if records and os.path.exists(os.path.join(dump, "club.dat")):
        failed += check_dump(dump, data, year, records)
    elif records:
        failed.append("the game wrote no merged club.dat")
    if os.path.exists(report):
        seasons, comps = rrc.read_report(report, clubs := [])
        divisions = rrc.division_records(data)
        # The list lines of season_clubs.c: a class that takes fewer clubs than the real list keeps the clubs that
        # the database has there first (slice 15), so a new record can fall out until slice 18 sets the count.
        short = {}
        for line in log.split("\n"):
            cells = line.split("\t")
            if line.startswith("recomp: club moves: list: ") and len(cells) > 14:
                named = {c.split()[0]: c.split()[-1] for c in cells[3:] if c.split()}
                real, cls = int(named["real"]), int(named["class"])
                if real > cls:
                    short[cells[1]] = (real, cls)
        for rec in records:
            div = divisions.get(rec[1])
            held = {c["name"] for c in clubs if c["phase"] == "start" and c["division"] == div}
            leagues = {c["record"] for c in comps if c["kind"] == "league" and c["clubs"] > 0}
            if div in leagues and rec[3] not in held and rec[1] in short:
                print(f"  {rec[3]}: not kept, {rec[1]} takes {short[rec[1]][1]} of {short[rec[1]][0]} real clubs "
                      f"(the class count, slice 18)")
            elif div in leagues and rec[3] not in held:
                failed.append(f"{rec[3]} is not in {rec[1]} at the start")
            elif div in leagues:
                print(f"  {rec[3]} plays in {rec[1]}")
            elif rec[1] in short:
                print(f"  {rec[3]}: not kept, {rec[1]} takes {short[rec[1]][1]} of {short[rec[1]][0]} real clubs "
                      f"(the class count, slice 18)")
            else:
                print(f"  {rec[3]}: the report holds no league of {rec[1]} (record {div})")
        for d in rrc.club_differences(seasons, comps, clubs, data):
            if d[2].split(":")[0] not in short and any(rec[3] in d[2] or rec[2] in d[2] for rec in records):
                failed.append(f"club_list names a new club: {d[2][:300]}")
        # A full season (--last Y+1): the end-of-season step runs only for a league whose matches are all played
        # (rules report doc), so a new club that the next start holds in a division of its nation played its season.
        if last > year:
            for rec in records:
                start = [c for c in clubs if c["season"] == year + 1 and c["phase"] == "start" and c["name"] == rec[3]]
                was = {c["division"] for c in clubs if c["season"] == year and c["phase"] == "start" and c["name"] == rec[3]}
                if not was:
                    continue
                if not start:
                    failed.append(f"{rec[3]}: in no division at the start of {year + 1}")
                else:
                    names = {v: k for k, v in divisions.items()}
                    print(f"  {rec[3]}: {year + 1} start in {names.get(start[0]['division'], start[0]['division'])}")
            if not any(s["season"] == year + 1 for s in seasons):
                failed.append(f"the report has no start of {year + 1}")
    else:
        failed.append("the run wrote no report")
    for f in failed:
        print("FAIL  " + f)
    print("PASS" if not failed else f"FAILED: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
