"""Undo game creation in a data folder made from the preview save, with May 2026 as the reference.

Usage: python3 -I merge.py <tools dir> <May folder> <preview folder> <match.pkl> <new folder>

The preview folder (save_to_db.py) keeps the Update Team's ids, names, clubs and people. Game creation changed
some of its fields; this puts them back:
- people that May 2026 also has (match.pkl, by name and birth date one year earlier): fields that creation writes
  or rescales take May's value, and fields that May leaves unset (0, -2, PA -1/-2, empty dates) become unset again;
- people that May 2026 does not have: rescaled fields are unset (0) or divided back (reputations / 50);
- clubs and nations: May's unset markers come back; nations take May's ranking block;
- careers: the preview's (player careers) plus May's non-player careers; award history: May's, remapped.
Dates of May rows move one year earlier: the preview is one data season later.
"""
import collections, os, pickle, shutil, struct, sys

sys.path[:0] = [sys.argv[1], sys.argv[1] + "/.."]   # this folder, and tools/save for savfile.py
from dbread import load, sname, i32, i16, SIZES      # noqa: E402

# Player bytes that creation rescales (May value -> another scale, data check classify.py): never copied.
PLAYER_RESCALED = [0x1e, 0x23, 0x24, 0x26, 0x27, 0x2a, 0x2b, 0x31, 0x32, 0x33, 0x35, 0x37, 0x38, 0x39, 0x3a,
                   0x3e, 0x41, 0x43, 0x45]
PLAYER_KEPT = [o for o in range(0x0e, 0x46) if o not in PLAYER_RESCALED]
NONPL_RESCALED = [0x10, 0x11, 0x17, 0x18, 0x19, 0x1b, 0x1e, 0x21]
NONPL_KEPT = [o for o in range(0x0e, 0x44) if o not in NONPL_RESCALED]
PERSONAL = range(0x56, 0x5e)       # staff personal attributes: 0 = unset in the data
EMPTY_YEAR = 1900


def put32(r, o, v): struct.pack_into("<i", r, o, v)
def put16(r, o, v): struct.pack_into("<h", r, o, v)


def shift_date(r, o):
    """A May date in the preview's season: one year earlier, unless empty (year 0 or 1900)."""
    y = struct.unpack_from("<H", r, o + 2)[0]
    if y not in (0, EMPTY_YEAR):
        struct.pack_into("<H", r, o + 2, y - 1)


def main(may_dir, pre_dir, match_path, out):
    if os.path.exists(out):
        sys.exit(f"{out} exists")
    may, pre = load(may_dir), load(pre_dir)
    M = pickle.load(open(match_path, "rb"))
    p2m = {j: i for i, j in M["m2p"].items()}
    c2p = M["c2p"]
    stats = collections.Counter()

    # ---- staff, player, non-player rows
    for j, r in enumerate(pre[6]):
        i = p2m.get(j)
        m = may[6][i] if i is not None else None
        put16(r, 0x18, i16(r, 0x12))              # +0x18: the year of birth in the data, the age in a game
        r[0x5e] = 1
        if m is not None:
            stats["staff matched"] += 1
            for o in PERSONAL:
                r[o] = m[o]
            r[0x52:0x56] = m[0x52:0x56]
            if i32(m, 0x24) == -1:                # creation's national squads
                r[0x24:0x39] = m[0x24:0x39]
                shift_date(r, 0x29); shift_date(r, 0x31)
            for o in (0x31, 0x46):                # empty contract ends: creation picks one
                if struct.unpack_from("<H", m, o + 2)[0] == EMPTY_YEAR:
                    r[o:o + 8] = m[o:o + 8]
            if struct.unpack_from("<H", m, 0x40)[0] == 0:
                r[0x3e:0x46] = m[0x3e:0x46]
        else:
            stats["staff new"] += 1
            for o in PERSONAL:
                r[o] = 0
            put32(r, 0x52, 0)
        p = i32(r, 0x61)
        if p >= 0:
            pr = pre[10][p]
            mp = may[10][i32(m, 0x61)] if m is not None and i32(m, 0x61) >= 0 else None
            if mp is not None:
                stats["player matched"] += 1
                for o in PLAYER_RESCALED:
                    pr[o] = mp[o]
                for o in PLAYER_KEPT:
                    if mp[o] == 0:
                        pr[o] = 0
                pr[0x09:0x0f] = mp[0x09:0x0f]     # reputations: creation recomputes them
                if i16(mp, 0x07) < 0:
                    pr[0x07:0x09] = mp[0x07:0x09]  # potential ability codes -1/-2
                if i16(mp, 0x05) < 55:
                    pr[0x05:0x07] = mp[0x05:0x07]  # creation lowers low current abilities
            else:
                stats["player new"] += 1
                for o in PLAYER_RESCALED:
                    pr[o] = 0
                for o in (0x09, 0x0b, 0x0d):
                    put16(pr, o, max(1, round(i16(pr, o) / 50)))
        q = i32(r, 0x69)
        if q >= 0:
            nr = pre[9][q]
            mn = may[9][i32(m, 0x69)] if m is not None and i32(m, 0x69) >= 0 else None
            if mn is not None:
                stats["non-player matched"] += 1
                for o in NONPL_RESCALED:
                    nr[o] = mn[o]
                for o in NONPL_KEPT:
                    if mn[o] == 0:
                        nr[o] = 0
                nr[0x04:0x0e] = mn[0x04:0x0e]
            else:
                stats["non-player new"] += 1
                for o in NONPL_RESCALED:
                    nr[o] = 0
                for o in (0x08, 0x0a, 0x0c):
                    put16(nr, o, max(0, round(i16(nr, o) / 50)))

    # ---- clubs: May's unset markers
    INT_FIELDS = [0x53, 0x57, 0x5b, 0x60, 0x65, 0x69, 0x6e, 0x73, 0x77, 0x7b] + list(range(0x83, 0xc3, 4)) + \
        [0xc3, 0xc7, 0xcb, 0xcf, 0xd3] + list(range(0xd7, 0x1df, 4))
    for i, j in c2p.items():
        mc, pc = may[0][i], pre[0][j]
        for o in INT_FIELDS:
            if i32(mc, o) == -2 and i32(pc, o) != -2:
                pc[o:o + 4] = mc[o:o + 4]; stats["club -2 back"] += 1
        for o in (0x73, 0x77, 0x7b):
            if i32(mc, o) == 0:
                put32(pc, o, 0)
        pc[0x6d] = mc[0x6d]
    matched_clubs = set(c2p.values())
    for j, pc in enumerate(pre[0]):
        if j not in matched_clubs:
            pc[0x6d] = 0
            for o in (0x73, 0x77, 0x7b):
                put32(pc, o, 0)
            stats["club new"] += 1

    # ---- nations: the block that creation fills (ids -2, ranking doubles)
    for a, b in zip(may[4], pre[4]):
        if sname(a, 4, 55) == sname(b, 4, 55):
            b[0x86:0xd6] = a[0x86:0xd6]

    # ---- careers: May's careers of people with no player row in the preview
    hist = list(pre[17])
    for h in may[17]:
        sid, cid = i32(h, 4), i32(h, 10)
        j = M["m2p"].get(sid)
        if j is None or i32(pre[6][j], 0x61) >= 0:
            continue
        n = bytearray(h)
        put32(n, 4, j); put32(n, 10, c2p.get(cid, -1))
        put16(n, 8, i16(n, 8) - 1)
        hist.append(n); stats["career rows from May"] += 1
    for k, h in enumerate(hist):
        put32(h, 0, k)
    pre[17] = hist

    # ---- award history: May's rows with names, staff and clubs remapped
    def name_index(t):
        d = {}
        for k, r in enumerate(t):
            d.setdefault(sname(r, 0, 51), k)
        return d
    fmay = [sname(r, 0, 51) for r in may[13]]; smay = [sname(r, 0, 51) for r in may[14]]
    fpre, spre = name_index(pre[13]), name_index(pre[14])
    aw = []
    for h in may[18]:
        n = bytearray(h)
        put16(n, 8, i16(n, 8) - 1)
        for o in range(0x0a, 58 - 15, 16):
            f, s, st, cl = struct.unpack_from("<4i", n, o)
            f2 = fpre.get(fmay[f], -1) if 0 <= f < len(fmay) else f
            s2 = spre.get(smay[s], -1) if 0 <= s < len(smay) else s
            struct.pack_into("<4i", n, o, f2, s2, M["m2p"].get(st, -1) if st >= 0 else st,
                             c2p.get(cl, -1) if cl >= 0 else cl)
        aw.append(n)
    pre[18] = aw
    stats["award rows"] = len(aw)

    write(pre_dir, pre, out)
    for k, v in stats.items():
        print(f"{k}: {v}")


def write(template, tables, out):
    """Same layout as the template: its index order, staff parts in staff.dat, other tables in their own file."""
    tidx = open(os.path.join(template, "index.dat"), "rb").read()
    os.makedirs(out)
    files, entries = collections.OrderedDict(), []
    for k in range(22):
        o = 8 + k * 67
        name = tidx[o:o + 51].split(b"\0")[0].decode("latin-1")
        tid, _, _, fmt = struct.unpack_from("<4i", tidx, o + 51)
        blob = b"".join(bytes(r) for r in tables[tid])
        cur = files.setdefault(name, bytearray())
        e = bytearray(67)
        e[:len(name)] = name.encode("latin-1")
        struct.pack_into("<4i", e, 51, tid, len(tables[tid]), len(cur), fmt)
        cur += blob
        entries.append(bytes(e))
    with open(os.path.join(out, "index.dat"), "wb") as f:
        f.write(struct.pack("<2i", 0, 0) + b"".join(entries))
    for name, b in files.items():
        with open(os.path.join(out, name), "wb") as f:
            f.write(b)
    for name in sorted(os.listdir(template)):
        if not os.path.exists(os.path.join(out, name)):
            shutil.copy2(os.path.join(template, name), os.path.join(out, name))


if __name__ == "__main__":
    main(*sys.argv[2:7])
