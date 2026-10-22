"""Squads and duplicate players of a saved game (the long holiday test, long_test.py).

usage: python3 -I save_check.py <save> [<club> ...]

The saved game holds a database of the loaded nations (members club.dat, staff.dat, the name tables and
index.dat, read with tools/save/savfile.py). For each club (default: Real Madrid C.F., FC Bayern München,
Arsenal) it counts the players of the squad list (+0xd7) and fails when there are fewer than 16, when an id is
there twice, or when two of them have the same name. Then it lists every pair of player rows of one person
(duplicates.py rules). Exit 1 on a club failure; the pairs are printed, because transfers in a game can bring
two players of one name together at one club.
"""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "save"))
import savfile  # noqa: E402
from cmdb import Folder, staff_view  # noqa: E402
import duplicates  # noqa: E402

CLUBS = ("Real Madrid C.F.", "FC Bayern München", "Arsenal")
MIN_PLAYERS = 16


def folder_of(save):
    """The save's database tables as a data folder (index.dat keeps the rows of the tables the save holds)."""
    s = savfile.Sav(save)
    d = tempfile.mkdtemp(prefix="save-check-")
    for m in s.members:
        if m.name.endswith(".dat"):
            with open(os.path.join(d, m.name), "wb") as h:
                h.write(s.read_member(m.name))
    ib = open(os.path.join(d, "index.dat"), "rb").read()
    rows = []
    for i in range(8, len(ib) - 66, 67):
        name = ib[i:i + 51].split(b"\0")[0].decode("latin1")
        if not name.endswith(".dat"):
            break
        if os.path.exists(os.path.join(d, name)):      # the save leaves out some tables
            rows.append(ib[i:i + 67])
    with open(os.path.join(d, "index.dat"), "wb") as h:
        h.write(ib[:8] + b"".join(rows))
    return Folder(d)


def main(argv):
    if not argv:
        sys.exit(__doc__)
    F = folder_of(argv[0])
    st, clubs = staff_view(F)
    by = {x["id"]: x for x in st}
    bad = 0
    for c in argv[1:] or CLUBS:
        ids = [k for k, v in clubs.items() if v == c]
        if not ids or ids[0] >= len(F.rows(0)):
            print(f"{c}: not in the save")
            continue
        r = F.rows(0)[ids[0]]
        squad = [v for v in (struct.unpack_from("<i", r, 0xd7 + 4 * j)[0] for j in range(50)) if v >= 0]
        players = [v for v in squad if v in by and by[v]["np"] >= 0]
        names = [by[v]["common"] or (by[v]["first"] + " " + by[v]["second"]).strip() for v in players]
        twice, same = len(squad) - len(set(squad)), len(names) - len(set(names))
        ok = len(players) >= MIN_PLAYERS and not twice and not same
        bad += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {c}: {len(players)} players in the squad list, {twice} ids twice, "
              f"{same} names twice: {'; '.join(sorted(names))}")
    pairs, _ = duplicates.find(F)
    for (a, b), w in sorted(pairs.items()):
        print(f"pair\t{w}\t{duplicates.show(by[a])}\t{duplicates.show(by[b])}")
    print(f"{len(pairs)} pairs of one person among {sum(1 for x in st if x['np'] >= 0)} players")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
