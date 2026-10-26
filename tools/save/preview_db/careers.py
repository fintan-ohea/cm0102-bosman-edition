"""Rebuild staff_history.dat rows from a save's player stats (see notes for the addresses)."""
import struct, collections

START_YEAR = 2001     # rows of this year or later never come from the data (0x7c079f)

def rebuild(s):
    ps = s.read_member("player stats.dat"); h = s.read_member("player stats history.tmp")
    st = s.read_member("staff.dat")
    ncat = struct.unpack_from("<i", ps, 4)[0]
    base = 8 + ncat * 32
    npl = struct.unpack_from("<i", ps, base)[0]; base += 4
    owner = {}
    for i in range(len(st) // 110):
        p = struct.unpack_from("<i", st, i * 110 + 0x61)[0]
        if p != -1: owner[p] = i
    rows, stats = [], collections.Counter()
    for p in range(npl):
        first, last = struct.unpack_from("<ii", ps, base + p * 39 + 0x18)
        if first == last == 0 and p not in owner: continue
        ents, off, guard = [], first, 0
        while True:
            if off < 0 or off + 47 > len(h): stats["bad offset"] += 1; break
            ents.append(h[off:off + 43])
            if off == last: break
            off = struct.unpack_from("<i", h, off + 43)[0]; guard += 1
            if guard > 2000: stats["loop"] += 1; break
        if first == last == 0 and not any(ents): continue
        cats = [e[0x2a] >> 3 for e in ents]
        stats.update(f"cat{c}" for c in cats)
        for e in ents:
            # An entry made from a data row (0x7c09e3..0x7c0b64): category 6, flag bits 0, apps at +6,
            # goals at +8, -1 at +0x14 and +0x26, the rest zero. Entries that the game archived
            # in the run-up to the start date carry match stats or flag bits and are left out.
            year, club = struct.unpack_from("<hi", e, 0)
            if e[0x2a] != 0x30 or year >= START_YEAR or e[7] or e[9] or any(e[10:0x14]) \
                    or e[0x14:0x16] != b"\xff\xff" or any(e[0x16:0x26]) or e[0x26:0x2a] != b"\xff" * 4:
                stats["left out"] += 1 if e[0x2a] >> 3 == 6 else 0
                continue
            # The loan byte +0x0e: 0 in every row of the Update Team data.
            rows.append((owner.get(p, -1), year, club, 0, e[6], e[8]))
    return rows, stats, npl
