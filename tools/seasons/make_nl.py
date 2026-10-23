#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_nl_table.c: the editions of the Nations Leagues and the real leagues of each
edition (0.6.0 slice 28, season_nl.h).

Usage:
    python3 -I tools/seasons/make_nl.py            write season_nl_table.c
    python3 -I tools/seasons/make_nl.py --check    exit 1 when season_nl_table.c is not the file that the
                                                   tables would give (ctest season_nl_table)

The editions: EDITIONS below, one per row of docs/research/rules-0.6.0/int-continental.tsv whose comp is a
Nations League and whose format gives a league phase. The sizes, group counts and moves are read from the
row's clubs, format, up and down text by hand and written here, because the text has no one form; the tool
checks each edition against its row (season, the total of the clubs column, each size in the format text).
The dates of the UEFA editions: docs/research/rules-0.6.0/nations-league-dates.tsv (the draw, the first day
of each matchday, the first semi-final and the final; MOVED_DATES for a decision).
The real leagues: docs/research/rules-0.6.0/nations-league-tiers.tsv (edition, confed, league, group, nation,
two sources). The nation names become the names of the game's nation.dat (NAMES), with the other names that a
database or a rename of slice 27 can give after a "/".
"""
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULES = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
TIERS = os.path.join(RULES, "nations-league-tiers.tsv")
DATES = os.path.join(RULES, "nations-league-dates.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_nl_table.c")

# row id: (confed, edition, season, sizes, groups, up_winners, down_last, down_lowest)
# down_last: 1 per league whose group last places go down. down_lowest: more nations of a league that go
# down, the lowest of the ranking (a play-out that the game does not play: decision for Max, see the doc).
EDITIONS = {
    # "League A 12, B 12, C 15, D 16; each league 4 groups of 3 or 4"; up "group winners of B, C, D promoted";
    # down "none in the end (format revamp)".
    "int-continental-190": ("UEFA", "2018-19", 2018, (12, 12, 15, 16), (4, 4, 4, 4), 1, (0, 0, 0, 0),
                            (0, 0, 0, 0)),
    # "League A, B, C 16 each (4 groups of 4), D 7 (groups of 4 and 3)"; up "group winners of B, C, D"; down
    # "last-placed teams of A and B groups; 2 League C teams by play-outs".
    "int-continental-212": ("UEFA", "2020-21", 2020, (16, 16, 16, 7), (4, 4, 4, 2), 1, (1, 1, 0, 0),
                            (0, 0, 2, 0)),
    # "League A, B, C 16 (4 groups of 4), D 7"; up "group winners of B, C, D"; down "last-placed teams of A
    # and B groups; 1 League C team by play-out". 55 entered, 54 played (Russia).
    # The league phase starts on 1 June 2022 (row int-continental-223, season 2021/22): season 2021.
    "int-continental-234": ("UEFA", "2022-23", 2021, (16, 16, 16, 7), (4, 4, 4, 2), 1, (1, 1, 0, 0),
                            (0, 0, 1, 0)),
    # "League A, B, C 16 (4 groups of 4), D 6 (2 groups of 3)"; up "group winners of B, C, D; play-off
    # winners"; down "last-placed teams of A and B; two lowest League C teams".
    "int-continental-256": ("UEFA", "2024-25", 2024, (16, 16, 16, 6), (4, 4, 4, 2), 1, (1, 1, 0, 0),
                            (0, 0, 2, 0)),
    # "League A, B, C 16 (4 groups of 4), D 6 (2 groups of 3)"; up "group winners of B and C, all League D
    # teams"; down "two worst last-placed League A teams". The next edition (2028-29, three leagues of 18) is a
    # proposal after the table (Q248).
    "int-continental-278": ("UEFA", "2026-27", 2026, (16, 16, 16, 6), (4, 4, 4, 2), 1, (0, 0, 0, 0),
                            (2, 0, 0, 0)),
    # "League A 12 (4 groups of 3), B 16, C 13"; up "group winners of B (to A) and C (to B)"; down
    # "last-placed teams of A groups (to B) and B groups (to C)".
    "int-continental-204": ("CONCACAF", "2019-20", 2019, (12, 16, 13), (4, 4, 4), 1, (1, 1, 0), (0, 0, 0)),
    # "league phase to 28 March 2023 (League A 12 teams, structure unchanged from 2019-20: inference)"; up
    # "group winners of B and C"; down "none (no relegation from 2022-23)". The league phase starts 2 June
    # 2022 (row int-continental-226, season 2021/22).
    # The league phase starts on 2 June 2022 (row int-continental-226, season 2021/22): season 2021.
    "int-continental-237": ("CONCACAF", "2022-23", 2021, (12, 16, 13), (4, 4, 4), 1, (0, 0, 0), (0, 0, 0)),
    # "League A 16: 12 in two groups of 6 (4 matches each), top 4 plus 4 top-ranked teams to quarter-finals;
    # League B 16 (4 groups of 4); League C 9 (3 groups of 3)"; up "group winners of B and C, best League C
    # runner-up"; down "5th and 6th of A groups, 4th of B groups".
    # Decision (slice 28 part b, recommended answer): League A of 16 plays 4 groups of 4, home and away, like the
    # UEFA leagues, and its group winners play the finals of four. The class holds no Swiss system, no groups of
    # 6 with 4 matches each and no byes or quarter-finals. The last of each A group goes down (4, as the real
    # 5th and 6th of the two groups of 6).
    "int-continental-248": ("CONCACAF", "2023-24", 2023, (16, 16, 9), (4, 4, 3), 1, (1, 1, 0), (0, 0, 0)),
    # As 2023-24 (League A 16 with 4 byes, B 16, C 9 from the moves); up "group winners of B; group winners of
    # C and the best C runner-up"; down "5th and 6th of A groups ...; 4th of B groups".
    "int-continental-259": ("CONCACAF", "2024-25", 2024, (16, 16, 9), (4, 4, 3), 1, (1, 1, 0), (0, 0, 0)),
    "int-continental-281": ("CONCACAF", "2026-27", 2026, (16, 16, 9), (4, 4, 3), 1, (1, 1, 0), (0, 0, 0)),
}

# Names of the tiers file -> names of the game's nation.dat (3.9.68 and the Update Team databases), with the
# other names after "/" (a later database, or a rename of slice 27, season_intl.c).
NAMES = {
    "Netherlands": "Holland",
    "Bosnia and Herzegovina": "Bosnia-Herzegovina",
    "Czech Republic": "Czech Republic/Czechia",
    "Czechia": "Czech Republic/Czechia",
    "North Macedonia": "FYR of Macedonia/North Macedonia",
    "Macedonia": "FYR of Macedonia/North Macedonia",
    "Turkey": "Turkey/Türkiye",
    "Türkiye": "Turkey/Türkiye",
    "Antigua and Barbuda": "Antigua & Barbuda",
    "Saint Kitts and Nevis": "St Kitts & Nevis",
    "Saint Vincent and the Grenadines": "St Vincent & The Grenadines",
    "Trinidad and Tobago": "Trinidad & Tobago",
    "Turks and Caicos Islands": "Turks and Caicos Islands",
    "Curaçao": "Curaçao/Netherlands Antilles",
    "Saint Martin": "Saint-Martin",
    "United States Virgin Islands": "US Virgin Islands",
    "U.S. Virgin Islands": "US Virgin Islands",
}
LEAGUES = "ABCD"

# The best runners-up of a league that go up too, by league (the up column of the row: "best League C
# runner-up" of CONCACAF 2023-24 and 2024-25; 2026-27 as 2024-25).
UP_SECOND = {
    "int-continental-248": (0, 0, 1),
    "int-continental-259": (0, 0, 1),
    "int-continental-281": (0, 0, 1),
}

# Rows whose format does not give every size: the sizes are checked against the row named here. 2022-23:
# "structure unchanged from 2019-20: inference" (row 237). 2024-25 and 2026-27: League C 9 from the moves of
# 2023-24 (3 winners and the best runner-up up, 4 down from B), as row 248 gives it.
SIZES_FROM = {
    "int-continental-237": "int-continental-204",
    "int-continental-259": "int-continental-248",
    "int-continental-281": "int-continental-248",
}


def fail(text):
    raise SystemExit("make_nl.py: " + text)


def read(name):
    with open(os.path.join(RULES, name), newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def editions():
    rows = {r["id"]: r for r in read("int-continental.tsv")}
    out = []
    for row, (confed, ed, season, sizes, groups, up, down, lowest) in EDITIONS.items():
        r = rows.get(row)
        if not r:
            fail(f"{row}: no such row")
        if int(r["season"][:4]) != season and not (row in ("int-continental-234", "int-continental-237")
                                                   and season == int(r["season"][:4]) - 1):
            fail(f"{row}: season {r['season']}, the tool gives {season}")
        total = re.match(r"^(\d+)", r["clubs"])
        if not total or int(total.group(1)) not in (sum(sizes), sum(sizes) + 1):
            fail(f"{row}: clubs {r['clubs']!r}, the sizes give {sum(sizes)}")
        fmt = rows[SIZES_FROM.get(row, row)]["format"]
        for n in sizes:
            if str(n) not in fmt and str(n) not in r["clubs"]:
                fail(f"{row}: the size {n} is not in the format")
        year = int(ed[:4])
        when = {}
        ds = dates()
        key = ed if confed == "UEFA" else "concacaf-" + ed     # the CONCACAF rows: "concacaf-2019-20"
        for ev in ("draw", "md1", "md2", "md3", "md4", "md5", "md6", "sf1", "final"):
            if (key, ev) not in ds:
                fail(f"{row}: no date {ev} of {key} in nations-league-dates.tsv")
            y, mo, d = ds[(key, ev)]
            when[ev] = (d, mo, y - year)
        out.append({"confed": confed, "edition": ed, "season": season, "year": year, "sizes": sizes,
                    "groups": groups, "up": up, "down": down, "lowest": lowest, "when": when, "row": row})
    out.sort(key=lambda e: (e["confed"] != "UEFA", e["confed"], e["season"]))
    return out


# Dates that a decision moves (Q277: a late edition plays at its planned time). The 2020-21 finals were
# planned for June 2021 and moved to October 2021 because Euro 2020 moved to June 2021; in the game Euro 2020
# plays in June 2020 (slice 27), so the finals keep June 2021. The days are those of the 2018-19 finals, two
# years on (inference: the planned days are not known).
# The European Championships whose qualifying has Nations League paths (int-continental rows 189 and 233): the
# group matchdays of the year after the campaign year come from the dates file (events euro<year>_qual_md<k>).
# Paths: Euro 2020 "4 paths of 4" (row 189), Euro 2024 "3 paths" (row 233), the 2022 World Cup UEFA qualifying
# (3 paths of 4: the 10 runners-up and the 2 best 2020-21 Nations League group winners, int-world rows of the
# 2022 cycle). key: (prefix of the events, finals year, paths, 1 for a World Cup)
# Euro 2028 (docs/research/2026-10-23-euro-2028-qualifying.md): paths 0, the play-off shape comes from the host
# places that the groups leave in use (season_nl_euro.c): 2 paths of 4, 3 paths of 4 or 4 two-leg ties.
QUALIFYING = {"euro2020": (2020, 4, 0), "euro2024": (2024, 3, 0), "wc2022": (2022, 3, 1), "euro2028": (2028, 0, 0)}

MOVED_DATES = {
    ("2020-21", "sf1"): "2021-06-02",
    ("2020-21", "final"): "2021-06-06",
    # The 2019-20 CONCACAF finals were planned for 4 to 7 June 2020 (row int-continental-204) and played in June
    # 2021 (Q277: the planned time).
    ("concacaf-2019-20", "sf1"): "2020-06-04",
    ("concacaf-2019-20", "final"): "2020-06-07",
    # The 2026-27 CONCACAF finals: only the window 25 to 28 March 2027 is known (dates file). Decision: the
    # semi-finals on its first day and the final on its last.
    ("concacaf-2026-27", "sf1"): "2027-03-25",
    ("concacaf-2026-27", "final"): "2027-03-28",
}

# The CONCACAF Gold Cups that the class copa_america can play (docs/research/2026-10-18-gold-cup-class.md): the
# rows of int-continental.tsv with comp "CONCACAF Gold Cup" and 12 or 16 teams. 2002 and 2003 (4 groups of 3)
# cannot be played. The first day of each edition: event "start" of "goldcup-<year>" in the dates file.
GOLD_COMP = "CONCACAF Gold Cup"
GOLD_FIRST = 2005


def dates():
    out = {}
    if not os.path.exists(DATES):
        return out
    with open(DATES, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            d = MOVED_DATES.get((r["edition"], r["event"]), r["date"])
            m = re.match(r"^(\d{4})-(\d{2})-(\d{2})$", d)
            if m:
                out[(r["edition"], r["event"])] = tuple(int(x) for x in m.groups())
    return out


def gold_hosts(r):
    """The hosts of a Gold Cup row: the format text starts "host United States and Canada;" (or "host A, B and
    C;"). The names of the game's nation.dat, ";" between hosts. An unknown host (2027: "host unknown (TBA)")
    gives the United States, the host of 13 of the 14 editions from 2005 (decision)."""
    m = re.match(r"^host (.*?);", r["format"])
    text = m.group(1) if m else ""
    if not text or "unknown" in text:
        return "United States"
    names = [n.strip() for n in re.split(r",| and ", text) if n.strip()]
    return ";".join(NAMES.get(n, n) for n in names)


def gold_cups():
    ds = dates()
    out = []
    for r in read("int-continental.tsv"):
        if r["comp"] != GOLD_COMP or not r["real_season"][:4].isdigit():
            continue
        year = int(r["real_season"][:4])
        if year < GOLD_FIRST:
            continue
        teams = int(r["clubs"]) if r["clubs"].isdigit() else 0
        if teams not in (12, 16):
            fail(f'{r["id"]}: {teams} teams, the class plays 12 or 16')
        key = (f"goldcup-{year}", "start")
        if key not in ds:
            fail(f"no date start of goldcup-{year} in nations-league-dates.tsv")
        y, mo, d = ds[key]
        if y != year:
            fail(f"goldcup-{year} starts in {y}")
        out.append({"year": year, "teams": teams, "day": d, "month": mo, "row": r["id"], "hosts": gold_hosts(r)})
    out.sort(key=lambda g: g["year"])
    return out


def tiers(known):
    if not os.path.exists(TIERS):
        return []
    out = []
    with open(TIERS, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            key = (r["confed"], r["edition"])
            if key not in known or r["league"] not in LEAGUES:
                continue                                # the CONCACAF qualifying round, a league "Q"
            group = int(r["group"]) if r["group"].strip().isdigit() else 0
            name = r["nation"].strip()
            out.append({"confed": r["confed"], "edition": r["edition"], "league": LEAGUES.index(r["league"]),
                        "group": group, "nation": NAMES.get(name, name),
                        "out": 1 if "disqualified" in r.get("note", "").lower() else 0})
    out.sort(key=lambda t: (t["confed"] != "UEFA", t["confed"], t["edition"], t["league"], t["group"],
                            t["nation"]))
    return out


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def text():
    eds = editions()
    known = {(e["confed"], e["edition"]) for e in eds}
    lines = [
        "/* The editions of the Nations Leagues and the real leagues of each edition (0.6.0 slice 28, season_nl.h).",
        " * Made by tools/seasons/make_nl.py from docs/research/rules-0.6.0/int-continental.tsv and",
        " * nations-league-tiers.tsv: do not edit by hand. */",
        '#include "season_nl.h"',
        "",
        "const SeasonNlEdition season_nl_editions[] = {",
    ]
    for e in eds:
        def arr(v):
            v = list(v) + [0] * (4 - len(v))
            return "{" + ", ".join(str(x) for x in v) + "}"
        def day(ev):
            d = e["when"].get(ev)
            return "{" + ", ".join(str(x) for x in d) + "}" if d else "{0, 0, 0}"
        mds = "{" + ", ".join(day(f"md{k}") for k in range(1, 7)) + "}"
        lines.append(f'    {{{c_str(e["confed"])}, {c_str(e["edition"])}, {e["season"]}, {e["year"]}, '
                     f'{len(e["sizes"])}, {arr(e["sizes"])}, {arr(e["groups"])}, {e["up"]}, {arr(e["down"])}, '
                     f'{arr(e["lowest"])}, {arr(UP_SECOND.get(e["row"], ()))},')
        lines.append(f'     {day("draw")}, {mds}, {day("sf1")}, {day("final")}, {c_str(e["row"])}}},')
    lines += [
        "};",
        "const int season_nl_edition_count = (int)(sizeof season_nl_editions / sizeof season_nl_editions[0]);",
        "",
        "const SeasonNlQualifying season_nl_qualifying[] = {",
    ]
    ds = dates()
    for prefix, (euro, paths, world_cup) in QUALIFYING.items():
        days = []
        for k in range(1, 11):
            key = (prefix, f"{prefix}_qual_md{k}")
            if key not in ds:
                fail(f"no date {key[1]} in nations-league-dates.tsv")
            y, mo, d = ds[key]
            days.append(f"{{{d}, {mo}, {y - (euro - 2)}}}")
        po = []
        for ev in ("po_sf", "po_final"):
            key = (prefix, f"{prefix}_{ev}")
            if key not in ds:
                fail(f"no date {key[1]} in nations-league-dates.tsv")
            y, mo, d = ds[key]
            po.append(f"{{{d}, {mo}, {y - (euro - 2)}}}")
        lines.append(f"    {{{euro}, {{{', '.join(days)}}}, {paths}, {po[0]}, {po[1]}, {world_cup}}},")
    lines += [
        "};",
        "const int season_nl_qualifying_count = (int)(sizeof season_nl_qualifying / sizeof season_nl_qualifying[0]);",
        "",
        "const SeasonNlGoldCup season_nl_gold_cups[] = {",
    ]
    for g in gold_cups():
        lines.append(f'    {{{g["year"]}, {g["teams"]}, {{{g["day"]}, {g["month"]}, 0}}, {c_str(g["row"])}, '
                     f'{c_str(g["hosts"])}}},')
    lines += [
        "};",
        "const int season_nl_gold_cup_count = (int)(sizeof season_nl_gold_cups / sizeof season_nl_gold_cups[0]);",
        "",
        "const SeasonNlTier season_nl_tiers[] = {",
    ]
    t = tiers(known)
    for x in t:
        lines.append(f'    {{{c_str(x["confed"])}, {c_str(x["edition"])}, {x["league"]}, {x["group"]}, '
                     f'{c_str(x["nation"])}, {x["out"]}}},')
    if not t:
        lines.append('    {"", "", 0, 0, "", 0},')
    lines += [
        "};",
        f"const int season_nl_tier_count = {len(t)};",
        "",
    ]
    return "\n".join(lines)


def main(argv):
    body = text()
    if "--check" in argv:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if old != body:
            print("make_nl.py: season_nl_table.c differs from the tables; run tools/seasons/make_nl.py")
            return 1
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(body)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
