"""Build a CM 01/02 data folder from a saved game made on the first day of a new game.

Usage: python3 -I save_to_db.py <tools dir> <save.sav> <template data folder> <new folder>

The tables come from the save. The save holds them in the layout of the data files: `save_database`
0x52dad0 writes both, and only the open and write calls differ (zip 0x9326e0/0x932e10, files
0x54d5b0/0x945ce6). What the converter changes:
- drops the 16 reserve rows that the loader adds at the end of the three name tables and the 16
  manager rows at the end of the staff table (with their player, non-player and preference rows);
- rebuilds staff_history.dat from the save's player career lists (player stats.dat and
  player stats history.tmp), the reverse of the PlayerStats constructor 0x7bfd50;
- writes staff_comp_history.dat empty (the save does not hold it);
- copies the files that are not tables (.pct, .cfg, .RGN) from the template folder.
"""
import os, shutil, struct, sys

sys.path[:0] = [sys.argv[1], sys.argv[1] + "/.."]   # this folder, and tools/save for savfile.py
from savfile import Sav            # noqa: E402
from careers import rebuild        # noqa: E402

RESERVE = 16                       # name reserve [0xae2348] and the manager rows of the staff table
STAFF, NONPLAYER, PLAYER, PREF = 110, 68, 70, 52


def rows(b, size):
    assert len(b) % size == 0, size
    return [bytearray(b[i:i + size]) for i in range(0, len(b), size)]


def i32(r, o):
    return struct.unpack_from("<i", r, o)[0]


def put(r, o, v):
    struct.pack_into("<i", r, o, v)


def main(save_path, template, out):
    if os.path.exists(out):
        sys.exit(f"{out} exists; pick a new folder")
    s = Sav(save_path)
    tables = {}

    for name in ("first_names.dat", "second_names.dat", "common_names.dat"):
        t = rows(s.read_member(name), 60)
        for r in t[-RESERVE:]:
            assert i32(r, 55) == -1, f"{name}: the last {RESERVE} rows are not reserve rows"
        tables[name] = t[:-RESERVE]

    staff = rows(s.read_member("staff.dat"), STAFF)
    nonpl = rows(s.read_member("nonplayer.dat"), NONPLAYER)
    player = rows(s.read_member("player.dat"), PLAYER)
    pref = rows(s.read_member("Preferences.dat"), PREF)
    keep = len(staff) - RESERVE
    dropped = set(range(keep, len(staff)))
    staff = staff[:keep]
    # Player (+0x61), preference (+0x65) and non-player (+0x69) rows still in use, in their old order.
    used = {0x61: set(), 0x65: set(), 0x69: set()}
    for r in staff:
        for o in used:
            if i32(r, o) != -1:
                used[o].add(i32(r, o))
    remap = {}
    for o, table in ((0x61, player), (0x65, pref), (0x69, nonpl)):
        old = sorted(used[o])
        remap[o] = {v: k for k, v in enumerate(old)}
        new = [table[v] for v in old]
        for k, r in enumerate(new):
            if i32(r, 0) == old[k]:
                put(r, 0, k)
        table[:] = new
    for r in staff:
        for o in remap:
            if i32(r, o) != -1:
                put(r, o, remap[o][i32(r, o)])

    clubs = {n: rows(s.read_member(n), 581) for n in ("club.dat", "nat_club.dat")}
    cleared = 0
    for t in clubs.values():
        for r in t:
            for o in range(0xcf, 0x1df, 4):          # the staff slots of a club, manager to coaches
                if i32(r, o) in dropped:
                    put(r, o, -1)
                    cleared += 1

    hist, stats, _ = rebuild(s)
    hist_rows = []
    for sid, year, club, loan, apps, goals in hist:
        if sid < 0 or sid >= keep:
            continue
        hist_rows.append(struct.pack("<iihiBBB", len(hist_rows), sid, year, club if club >= 0 else -1,
                                     loan, apps, goals))

    blob = {
        "club.dat": b"".join(clubs["club.dat"]),
        "nat_club.dat": b"".join(clubs["nat_club.dat"]),
        "first_names.dat": b"".join(tables["first_names.dat"]),
        "second_names.dat": b"".join(tables["second_names.dat"]),
        "common_names.dat": b"".join(tables["common_names.dat"]),
        "staff_history.dat": b"".join(hist_rows),
        "staff_comp_history.dat": b"",
    }
    for name in ("colour.dat", "continent.dat", "nation.dat", "officials.dat", "stadium.dat",
                 "staff_comp.dat", "city.dat", "club_comp.dat", "nation_comp.dat",
                 "club_comp_history.dat", "nation_comp_history.dat"):
        blob[name] = s.read_member(name)
    parts = [(6, b"".join(staff), STAFF), (9, b"".join(nonpl), NONPLAYER),
             (10, b"".join(player), PLAYER), (22, b"".join(pref), PREF)]

    # Index entries: the save's format flags, the order of the template's index.dat.
    sidx = s.read_member("index.dat")
    sizes = {0: 581, 1: 581, 2: 58, 3: 198, 4: 290, 5: 78, 7: 43, 11: 101, 21: 56, 12: 107, 16: 107,
             13: 60, 14: 60, 15: 60, 17: 17, 18: 58, 19: 26, 20: 26, 6: 110, 9: 68, 10: 70, 22: 52}
    fmt = {}
    for k in range(22):
        o = 8 + k * 67
        tid, count = struct.unpack_from("<2i", sidx, o + 51)
        fmt[tid] = struct.unpack_from("<i", sidx, o + 63)[0]
        name = sidx[o:o + 51].split(b"\0")[0].decode("latin-1")
        # The nation comp part also holds the 127 friendly rows that follow the index count
        # ([0xae2344], written at 0x52f978, read back at 0x52bbd4). A data folder has none.
        if name in blob and name not in tables and len(blob[name]) > count * sizes[tid]:
            print(f"{name}: keeps {count} of {len(blob[name]) // sizes[tid]} rows")
            blob[name] = blob[name][:count * sizes[tid]]
    tidx = open(os.path.join(template, "index.dat"), "rb").read()
    os.makedirs(out)
    staff_blob = b"".join(p[1] for p in parts)
    staff_off = {}
    off = 0
    for tid, b, _ in parts:
        staff_off[tid] = off
        off += len(b)
    entries = []
    for k in range(22):
        o = 8 + k * 67
        name = tidx[o:o + 51].split(b"\0")[0].decode("latin-1")
        tid = struct.unpack_from("<i", tidx, o + 51)[0]
        if tid in staff_off:
            size = dict((p[0], p[2]) for p in parts)[tid]
            count = len(dict((p[0], p[1]) for p in parts)[tid]) // size
            offset = staff_off[tid]
        else:
            count, offset = len(blob[name]) // sizes[tid], 0
        e = bytearray(67)
        e[:len(name)] = name.encode("latin-1")
        struct.pack_into("<4i", e, 51, tid, count, offset, fmt.get(tid, 1))
        entries.append(bytes(e))
        print(f"{name:24s} id {tid:2d} count {count:7d} offset {offset:9d} format {fmt.get(tid, 1)}")
    with open(os.path.join(out, "index.dat"), "wb") as f:
        f.write(struct.pack("<2i", 0, 0) + b"".join(entries))
    with open(os.path.join(out, "staff.dat"), "wb") as f:
        f.write(staff_blob)
    for name, b in blob.items():
        with open(os.path.join(out, name), "wb") as f:
            f.write(b)
    for name in sorted(os.listdir(template)):
        if not os.path.exists(os.path.join(out, name)):
            shutil.copy2(os.path.join(template, name), os.path.join(out, name))
    print(f"club staff slots cleared: {cleared}; career rows: {len(hist_rows)}; list stats: {dict(stats)}")


if __name__ == "__main__":
    main(*sys.argv[2:5])
