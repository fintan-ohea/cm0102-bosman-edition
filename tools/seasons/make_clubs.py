#!/usr/bin/env python3
"""Makes the club moves table of the game from the clubs files of the research (0.6.0 slice 15, Q225, Q237).

Usage:
    python3 -I tools/seasons/make_clubs.py --out <clubs.tsv>     write the table for the game
    python3 -I tools/seasons/make_clubs.py --check               read the files, write nothing
    python3 -I tools/seasons/make_clubs.py --lists <file.tsv>    write each club list and what became of it
    python3 -I tools/seasons/make_clubs.py --missing <file.tsv>  write the real clubs that no database record matches
        [--databases <folder>[,<folder>]]                        leave out the rows that the game finds by the plain
                                                                 name in the season's database (<folder>/<name>...)

Input: docs/research/rules-0.6.0/<part>-clubs.tsv. Each row is a real club of a division at the start of a
season (columns season, division, club, db_club_id, db_club, db_division, source). db_club_id is the record id
of the club in the season's database (docs/research/2026-10-16-season-database-table.md), db_club its name
there and db_division the database division that it holds there. One club list is the rows of one part,
season and division.

Output: Seasons/clubs.tsv of the download, Windows-1252 (the text form of the game's data files), with tabs.
Line 1 is "#bosman-clubs<TAB>1<TAB><row count>", line 2 the column names, then one row per club:
    season    the start year (2001 for 2001/02)
    division  the long name of the database division (club_comp.dat +4) that the club goes in
    club_id   db_club_id
    club      db_club, the long name of the club record (club.dat +4)
    real      the real name of the club (for the log; a character that Windows-1252 lacks becomes "?")
The game moves the clubs at a new game in that season (src/recomp_rt/win/season_clubs.c).

The database division of a list (target) comes from TARGETS: one line per part and label, checked against
the db_division column of the clubs files and the key names of the original (match_club_comp_key_id
0x60efd0). A list is left out (and --lists says why) when its label names a cup entry, a guest or a play-off
("cup", "guest", "play-off", "B team": cup entrants, slice 21), when TARGETS leaves it out (a new division of
slice 20, real groups that do not match the database divisions, a mapping that leaves a database division
empty), or when no line of TARGETS matches it.
Several lists with one target in a season (the groups of a real division that the database holds in one
division) give one list. A club in two lists of a season keeps the first (in the order of the files and
their lines), and --lists names the second.
A row with no db_club_id goes out with club_id -1 and the real name: the game looks for the club by the plain
form of its name in the nation of the division, and if no club or more than one has it, the place is that of
a missing club (Q359). --missing lists these rows (the runs log the clubs that stay missing).

The stop rule: a club id that is not a number, a name that Windows-1252 cannot hold or that is longer than 51
characters, a season outside 2001 to 2099, or an empty division, club or real name stops the tool with exit
code 1. The game's loader drops the whole file on such a row.
"""
import argparse
import collections
import csv
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PARTS = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
sys.path.insert(0, HERE)
import make_rules  # noqa: E402

FORMAT = 1
LIMIT = 51          # the longest name that the loader takes (season_clubs.c): the record holds 52 bytes
COLUMNS = ["season", "division", "club", "db_club_id", "db_club", "db_division", "source"]
NOT_DIVISION = re.compile(r"\bcup\b|guest|play-off|\bB team\b", re.I)

# The database division of each list, by part and label (a regular expression on the whole label, after a
# year in brackets at the end, as in "Série A (2008)", is dropped). None: the list is left out for the reason
# given. "=": the label is the database name.
SKIP_GROUPS = "the real groups do not match the database divisions (slice 18)"
SKIP_NEW = "a division that the original has no class for (comp_file new, slice 20)"
SKIP_CUP = "cup entrants, guests or a play-off (not a division; slice 21)"
SKIP_EMPTY = "the mapping of Q292 leaves a database division empty (slice 18)"
TARGETS = [
    ("arg-usa", r"Major League Soccer", "American Major League"),     # 12 clubs kept (season_mls.c)
    ("arg-usa", r"Primera División", "Argentine Premier Division"),
    ("arg-usa", r"Primera B Nacional|Primera Nacional", "Argentine Second Division"),
    ("aus-jap-kor", r"A-League", "Australian National Soccer League"),
    ("aus-jap-kor", r"J1 League", "Japanese J-League 1"),
    ("aus-jap-kor", r"J2 League", "Japanese J-League 2"),
    ("aus-jap-kor", r"K League", "Korean League"),
    ("bra", r"Série A", "Brazilian National First Division"),
    ("bra", r"Série B", "Brazilian National Second Division"),
    ("bra", r"Série C", "Brazilian National Third Division"),
    ("den-swe", r"Allsvenskan", "Swedish Premier Division"),
    ("den-swe", r"Superettan", "Swedish First Division"),
    ("den-swe", r"Division 1 (Norra|Södra)|Ettan( Norra| Södra)?", None, SKIP_GROUPS),
    ("den-swe", r"Danish Superliga", "Danish Premier Division"),
    ("den-swe", r"Danish 1st Division", "Danish First Division"),
    ("den-swe", r"Danish 2nd Division( group [123])?", "Danish Second Division"),
    ("eng", r"Conference Premier|National League", "English Conference"),
    ("eng", r"League One", "English Second Division"),
    ("fra", r"Ligue 1", "French First Division"),
    ("fra", r"Ligue 2", "French Second Division"),
    ("fra", r"Championnat National", "French National"),
    ("fra", r"CFA|National 2", "French CFA"),
    ("fra", r"CFA 2|National 3", "French Lower Division"),
    ("ger", r"Regionalliga Nord", "German Regional Division North"),
    ("ger", r"Regionalliga Süd", "German Regional Division South"),
    ("ger", r"3\. Liga", "German Regional Division North"),   # slice 20: the 3. Liga in the GerRegional object
    ("gre-pol-cro", r"cro_a1", "Croatian First Division"),
    ("gre-pol-cro", r"cro_a2a", "Croatian Second Division North"),
    ("gre-pol-cro", r"cro_a2b", "Croatian Second Division South"),
    ("gre-pol-cro", r"gre_prm", "Greek National A Division"),
    ("gre-pol-cro", r"gre_second", "Greek National B Division"),
    ("gre-pol-cro", r"pol_first", "Polish First Division"),
    ("gre-pol-cro", r"pol_second", "Polish Second Division"),
    ("gre-pol-cro", r"new \(gre second, group 2\)", "Greek Second Division Group 2"),   # slice 20d: a new record
    ("gre-pol-cro", r"new .*", None, SKIP_NEW),
    ("hol-bel", r"Eredivisie", "Dutch Premier Division"),
    ("hol-bel", r"Eerste Divisie", "Dutch First Division"),
    ("hol-bel", r"Belgian First Division( A)?|Belgian Pro League", "Belgian First Division"),
    ("hol-bel", r"Belgian Second Division|Belgian First Division B|Challenger Pro League", "Belgian Second Division"),
    ("hol-bel", r"Belgian Third Division A", "Belgian Third Division A"),
    ("hol-bel", r"Belgian Third Division B", "Belgian Third Division B"),
    ("hol-bel", r"Belgian First Amateur Division|Belgian National Division 1", None, SKIP_EMPTY),
    ("hol-bel", r"Belgian Division 1 VV", "Belgian Third Division A"),
    ("hol-bel", r"Belgian Division 1 ACFF", "Belgian Third Division B"),
    ("ita", r"Serie A", "Italian Serie A"),
    ("ita", r"Serie B", "Italian Serie B"),
    ("ita", r"(Serie C1|Lega Pro Prima Divisione) girone A", "Italian Serie C1/A"),
    ("ita", r"(Serie C1|Lega Pro Prima Divisione) girone B", "Italian Serie C1/B"),
    ("ita", r"(Serie C2|Lega Pro Seconda Divisione) girone A", "Italian Serie C2/A"),
    ("ita", r"(Serie C2|Lega Pro Seconda Divisione) girone B", "Italian Serie C2/B"),
    ("ita", r"(Serie C2|Lega Pro Seconda Divisione) girone C", "Italian Serie C2/C"),
    # Slice 20b: the single Serie C (2014/15 on) plays its groups A, B and C in the objects of C1/A, C1/B and C2/A.
    ("ita", r"(Serie C|Lega Pro) girone A", "Italian Serie C1/A"),
    ("ita", r"(Serie C|Lega Pro) girone B", "Italian Serie C1/B"),
    ("ita", r"(Serie C|Lega Pro) girone C", "Italian Serie C2/A"),
    ("nor-fin", r"Tippeligaen|Eliteserien", "Norwegian Premier Division"),
    ("nor-fin", r"1\. divisjon", "Norwegian First Division"),
    ("nor-fin", r"Veikkausliiga", "Finnish Premier Division"),
    ("nor-fin", r"Ykkönen North zone", "Finnish First Division North"),
    ("nor-fin", r"Ykkönen South zone", "Finnish First Division South"),
    ("nor-fin", r"Ykkönen|Ykkösliiga", None, SKIP_GROUPS),
    ("por", r"Primeira Liga", "Portuguese Premier League"),
    ("por", r"Liga de Honra|Segunda Liga|LigaPro|Liga Portugal 2", "Portuguese Second League"),
    ("por", r"Segunda Divisão( B)? Zona Norte", "Portuguese Second Division B North"),
    ("por", r"Segunda Divisão( B)? Zona Centro", "Portuguese Second Division B Central"),
    ("por", r"Segunda Divisão( B)? Zona Sul", "Portuguese Second Division B South"),
    ("por", r"Segunda Divisão Série [A-D]|Campeonato .* Série [A-H]", None, SKIP_GROUPS),
    ("por", r"Liga 3 Série A", "Portuguese Second Division B North"),     # slice 20: Liga 3 in PorSecondB (Q291)
    ("por", r"Liga 3 Série B", "Portuguese Second Division B Central"),
    ("rus-tur", r"Russian Premier Division \(real: .*\)", "Russian Premier Division"),
    ("rus-tur", r"Russian First Division \(real: .*\)", "Russian First Division"),
    ("rus-tur", r"Turkish Premier Division \(real: .*\)", "Turkish Premier Division"),
    ("rus-tur", r"Turkish 2\. Division Category A \(real: .*\)", "Turkish 2. Division Category A"),
    ("rus-tur", r"Turkish 2\. Division Category B \(real: .*\)", None, SKIP_GROUPS),
    ("spa", r"Segunda División B Group I", "Spanish Second Division B1"),
    ("spa", r"Segunda División B Group II", "Spanish Second Division B2"),
    ("spa", r"Segunda División B Group III", "Spanish Second Division B3"),
    ("spa", r"Segunda División B Group IV", "Spanish Second Division B4"),
    ("spa", r"Segunda División B Subgroup .*", None, SKIP_GROUPS),
    ("spa", r"Primera (División RFEF|Federación) Group 1.*", "Spanish Second Division B1"),   # slice 20 (Q291)
    ("spa", r"Primera (División RFEF|Federación) Group 2.*", "Spanish Second Division B2"),
    ("wal-nir-irl", r"Irish Premier Division|Irish First Division|Northern Irish League Premier Division|"
                    r"Northern Irish League First Division|Welsh Premier Division", "="),
]


def season_of(text):
    m = re.match(r"^(\d{4})/\d{2}$", text.strip())
    if not m:
        raise SystemExit(f"make_clubs.py: season {text!r}")
    return int(m.group(1))


def read_lists():
    """[(part, season, label, rows)] in the order of the files and their lines."""
    lists = collections.OrderedDict()
    for path in sorted(glob.glob(os.path.join(PARTS, "*-clubs.tsv"))):
        part = os.path.basename(path)[:-len("-clubs.tsv")]
        with open(path, newline="", encoding="utf-8") as f:
            lines = list(csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE))
        if lines[0] != COLUMNS:
            raise SystemExit(f"make_clubs.py: {path}: the columns are not those of a clubs file")
        for n, cells in enumerate(lines[1:], start=2):
            if not cells:
                continue
            if len(cells) != len(COLUMNS):
                raise SystemExit(f"make_clubs.py: {path} line {n}: {len(cells)} columns")
            r = dict(zip(COLUMNS, cells))
            r["line"] = n
            ident = r["db_club_id"].strip()
            if ident and not re.match(r"^\d+$", ident):
                raise SystemExit(f"make_clubs.py: {path} line {n}: db_club_id {ident!r} is not a number")
            key = (part, season_of(r["season"]), r["division"])
            lists.setdefault(key, []).append(r)
    return [(p, s, d, rows) for (p, s, d), rows in lists.items()]


def target_of(part, season, label, rows, rule_rows):
    """(database division, None) or (None, the reason the list is left out)."""
    if NOT_DIVISION.search(label):
        return None, SKIP_CUP
    plain = re.sub(r" \((19|20)\d\d\)$", "", label)
    for entry in TARGETS:
        if entry[0] == part and re.fullmatch(entry[1], plain):
            if entry[2] is None:
                return None, entry[3]
            return (plain if entry[2] == "=" else entry[2]), None
    for r in rule_rows.get((part, season), []):
        if label in (r["name"], r["comp"]) and r["comp_file"] == "new":
            return None, SKIP_NEW
    return None, "no database division is known for this list"


def build():
    """(rows of the output, lists report, missing clubs)."""
    rule_rows = collections.defaultdict(list)
    for path in make_rules.parts():
        part = os.path.basename(path)[:-4]
        with open(path, newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE):
                rule_rows[(part, season_of(r["season"]))].append(
                    {"name": r["name"], "comp": r["comp"], "comp_file": make_rules.comp_file(r["comp_file"])})
    out, report, missing = [], [], []
    placed = {}                                    # (season, club id) -> (part, label)
    for part, season, label, rows in read_lists():
        target, why = target_of(part, season, label, rows, rule_rows)
        have = [r for r in rows if r["db_club_id"].strip()]
        for r in rows:
            if not r["db_club_id"].strip():
                missing.append((season, part, label, r["club"], target or "", r["source"]))
        if not target:
            report.append((season, part, label, len(rows), len(have), "", "left out: " + why))
            continue
        twice = []
        for r in rows:
            if not r["db_club_id"].strip():
                # No record id in the research: the game looks for the club by its name (plain form) in the
                # nation of the division (season_clubs.c), else the place is a missing club.
                out.append((season, target, -1, plain(r["club"]), plain(r["club"])))
                continue
            key = (season, int(r["db_club_id"]))
            if key in placed:
                twice.append(r["db_club"])
                continue
            placed[key] = (part, label)
            out.append((season, target, int(r["db_club_id"]), r["db_club"].strip(), plain(r["club"])))
        note = f"{len(twice)} clubs already in another list: {', '.join(twice)}" if twice else ""
        report.append((season, part, label, len(rows), len(have), target, note))
    return out, report, missing


PLAIN_DROP = {"fc", "cf", "sc", "ac", "ec", "ca", "cd", "sd", "ud", "if", "fk", "sk", "bk", "afc", "club", "clube",
              "futebol", "football", "ltda", "de", "da", "do", "das", "dos", "the", "e", "y", "and"}
PLAIN_FOLD = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty"     # bytes 0xc0 to 0xff


def plain_name(name, drop=True):
    """The plain form of a club name, as plain_words of src/recomp_rt/win/season_clubs.c: the letters of
    Windows-1252 without accents, lower case, words of letters and digits, the words of PLAIN_DROP left out
    when drop is set."""
    raw = name.encode("cp1252", "replace") if not all(ord(c) < 256 for c in name) else name.encode("latin-1")
    text = "".join(PLAIN_FOLD[b - 0xc0] if b >= 0xc0 else chr(b) for b in raw).lower()
    words = "".join(c if c.isascii() and c.isalnum() else " " for c in text).split()
    return " ".join(w for w in words if not drop or w not in PLAIN_DROP)


def word_prefix(a, b):
    """True when the words of a are the first words of b, or those of b the first words of a."""
    a, b = a.split(), b.split()
    n = min(len(a), len(b))
    return a[:n] == b[:n]


def plain_match(want, names):
    """The one name of names that the game takes for the research name want (season_clubs.c): the only name
    with the same plain form, else of several such names the only one whose full words begin want or begin
    with it. None when no name or more than one is left."""
    hits = [n for n in names if plain_name(n) == plain_name(want)]
    if len(hits) > 1:
        hits = [n for n in hits if word_prefix(plain_name(n, False), plain_name(want, False))]
    return hits[0] if len(hits) == 1 else None


def season_databases(folders):
    """{season: {plain name: [names]}} of the club.dat of each season database (databases-signatures.tsv) that
    one of the folders (a list) holds. A record is 581 bytes, its long name at +4 (52 bytes)."""
    out = {}
    with open(os.path.join(ROOT, "docs", "research", "rules-0.6.0", "databases-signatures.tsv"), newline="",
              encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            # A folder may hold the database under its name with more words after it.
            paths = [os.path.join(d, e, "club.dat") for d in folders for e in sorted(os.listdir(d))
                     if e == r["database"] or e.startswith(r["database"] + " ")]
            path = next((p for p in paths if os.path.exists(p)), None)
            if r["role"] != "season database" or not path:
                continue
            data = open(path, "rb").read()
            by_plain = collections.defaultdict(list)
            for i in range(len(data) // 581):
                name = data[i * 581 + 4:i * 581 + 56].split(b"\0")[0].decode("latin-1")
                by_plain[plain_name(name)].append(name)
            out[int(r["data_season"][:4])] = by_plain
    return out


def plain(text):
    """A real club name for the game's text: Windows-1252, a character it lacks becomes "?", 51 characters."""
    return text.strip().encode("cp1252", "replace").decode("cp1252")[:LIMIT]


def game_bytes(text):
    """The bytes of a name in the game's data: Windows-1252. Some database names of the research hold a
    Windows-1252 byte read as Latin-1 (U+0092 for the byte 0x92), so those go back as Latin-1."""
    try:
        return text.encode("cp1252")
    except UnicodeEncodeError:
        return text.encode("latin-1")


def write(path, header, rows, game=False):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    tmp = path + ".tmp"
    lines = [*header, *("\t".join(str(c) for c in r) for r in rows)]
    with open(tmp, "wb") as f:
        for line in lines:
            f.write((game_bytes(line) if game else line.encode("utf-8")) + b"\n")
    os.replace(tmp, path)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--lists")
    ap.add_argument("--missing")
    ap.add_argument("--databases")
    a = ap.parse_args(argv)
    if not (a.out or a.check or a.lists or a.missing):
        ap.error("give --out, --check, --lists or --missing")
    out, report, missing = build()
    for r in out:
        # The game's loader (season_clubs.c) drops the whole file on one bad row, so the tool stops on it.
        if not 2001 <= r[0] <= 2099 or not r[1].strip() or not r[3].strip() or not r[4].strip():
            print(f"make_clubs.py: {r[0]} {r[1]!r} {r[3]!r} {r[4]!r}: a season outside 2001 to 2099 or an empty "
                  f"division, club or real name")
            return 1
        for text in (r[1], r[3]):
            try:
                game_bytes(text)
            except UnicodeEncodeError:
                print(f"make_clubs.py: {r[0]} {text!r} has a character that Windows-1252 cannot hold")
                return 1
            if len(text) > LIMIT:
                print(f"make_clubs.py: {r[0]} {text!r} is longer than {LIMIT} characters")
                return 1
    if a.check:
        kept = sum(1 for r in report if r[5])
        print(f"make_clubs.py: {len(out)} club moves in {kept} lists; {len(report) - kept} lists left out; "
              f"{len(missing)} real clubs with no database record")
    if a.out:
        write(a.out, [f"#bosman-clubs\t{FORMAT}\t{len(out)}", "season\tdivision\tclub_id\tclub\treal"], out,
              game=True)
        print(f"make_clubs.py: {len(out)} club moves written to {a.out}")
    if a.lists:
        write(a.lists, ["season\tpart\tlist\tclubs\twith_record\tdatabase_division\tnote"], report)
    if a.missing and a.databases:
        # The game finds a club with no record id by its plain name in the nation of the division
        # (season_clubs.c). Here a row goes out when plain_match finds one club of the season's database (of
        # any nation, so this is stricter than the game).
        dbs = season_databases(a.databases.split(","))
        found = [m for m in missing if plain_match(m[3], dbs.get(m[0], {}).get(plain_name(m[3]), []))]
        missing = [m for m in missing if m not in found]
        print(f"make_clubs.py: {len(found)} rows found by the plain name, {len(missing)} missing; seasons with a "
              f"database: {len(dbs)}")
    if a.missing:
        write(a.missing, ["season\tpart\tlist\tclub\tdatabase_division\tsource"], missing)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
