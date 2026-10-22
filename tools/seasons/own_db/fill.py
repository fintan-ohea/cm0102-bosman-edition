"""Fill (Q382): FM players whom the template lacks, taken with their ratings and position from an older Update Team
database (season settings FILL), and appended to the template's tables before convert.py writes the facts.

fill(T, F, fp, M, L, S_) -> {FM person index: new staff id}. T is the template, F the fill database (cmdb.Folder).

Who: an FM player (job bit 1, not a player-coach or player-manager) with a date of birth, at an FM club that maps
to a template club in a division (leagues.pkl), whom match.py did not match. First he is looked for in the template
itself (people.py: names that agree, dates up to two days apart, the same nation): one hit maps him to that row,
which convert.py then moves to his FM club (2026-10-21: such players came in twice). Else he is found in F by first
and second name or common name and date of birth (F's birth shift from the date shift research), else by surname
and the date one day either way when that is unique, else by club (F's club id is the template's for most clubs),
one surname word and the first word of the first name (or the common name), with the birth year two years either
way (the older data has some years wrong). An F row whom the template also holds (the exact name and date, or
people.py) is skipped. duplicates.py merges the rows that these tests still miss.

What is copied (record layouts: include/database.h and the staff.dat of the Update Team databases, format 2):
- the staff row (110 bytes): names, date of birth, nations, caps, the rest as F has it. +0x61 is the player row,
  +0x65 the preferences row (its id is the staff id), +0x69 the non-player row (-1 for a player). The club, job,
  joined date and contract are set later by convert.py from the FM facts. National club: none.
- the player row (table 10, 70 bytes: the id, then the ratings and positions).
- a preferences row (table 22, 52 bytes): the id and no favourite clubs or people (-1), as most rows have.
- the names (tables 13, 14, 15, 60 bytes: text, id, nation id, count), reused when the template has the text.
- the careers (staff_history.dat, 17 bytes: id, staff, year, club, 3 bytes) before the start season. Club ids
  map by id when the name is the same in both, else by long name and nation.
Dates are written in the template's convention (the template's shift subtracted from the real year), because
convert.py then adds the template's shifts to every row. The careers of the new rows count as not kept.
"""
import collections
import datetime
import struct

from cmdb import cstr, norm, swords
from people import PLACEHOLDER_DAYS, days_apart_ok, names_agree
from seasons import template_shifts

CUM = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]


def _i(r, o):
    return struct.unpack_from("<i", r, o)[0]


def _md(day):
    m = max(i for i in range(12) if CUM[i] <= day)
    return m + 1, day - CUM[m] + 1


def _set_count(buf, table, count, offset=None):
    for i in range(8, len(buf) - 66, 67):
        if _i(buf, i + 51) == table:
            struct.pack_into("<i", buf, i + 55, count)
            if offset is not None:
                struct.pack_into("<i", buf, i + 59, offset)
            return
    raise SystemExit(f"fill: index.dat has no table {table}")


def fill(T, F, fp, M, L, S_):
    tsh, fsh = template_shifts(S_.TEMPLATE), template_shifts(S_.FILL)
    pmap, clubmap = M["pmap"], M["clubmap"]
    in_div = {c for c, d in L["cur"].items() if d >= 0}
    matched = set(pmap)

    def names(F_, t):
        return {_i(r, 0x33): cstr(r, 0, 51) for r in F_.rows(t)}
    ffn, fsn, fcn = names(F, 13), names(F, 14), names(F, 15)
    tfn, tsn, tcn = names(T, 13), names(T, 14), names(T, 15)

    def keys(first, second, common, dob):
        out = [(norm(first), norm(second), dob)]
        if common:
            out.append(("c", norm(common), dob))
        return out

    # The template's people under their real dates of birth, to skip people it already holds.
    have = set()
    for r in T.rows(6):
        d, y = struct.unpack_from("<hH", r, 0x10)
        if y > 1900:
            c = _i(r, 0x0c)
            have.update(keys(tfn.get(_i(r, 4), ""), tsn.get(_i(r, 8), ""), tcn.get(c, "") if c >= 0 else "",
                             (_md(d), y + tsh["birth"])))
    # The template's people with a player row by real date of birth (forum report, 2026-10-21): the templates store
    # some dates one day later than FM, and some names split another way, so the exact "have" keys miss them and the
    # same player came in twice. people.py says when two rows are one person.
    tbyday = collections.defaultdict(list)
    for r in T.rows(6):
        d, y = struct.unpack_from("<hH", r, 0x10)
        if y <= 1900 or _i(r, 0x61) < 0:
            continue
        try:
            day = datetime.date(y + tsh["birth"], *_md(d)).toordinal()
        except ValueError:
            continue
        c = _i(r, 0x0c)
        tbyday[day].append((_i(r, 0), _i(r, 0x1a), tfn.get(_i(r, 4), ""), tsn.get(_i(r, 8), ""),
                            tcn.get(c, "") if c >= 0 else "", d in PLACEHOLDER_DAYS))

    def in_template(first, second, common, day, nation=None):
        """The template rows that are this person (people.py), any nation when nation is None."""
        out = set()
        for dd in range(-2, 3):
            for tid, tnat, f, s_, c, new_year in tbyday.get(day + dd, ()):
                if nation is not None and tnat != nation:
                    continue
                k = names_agree(first, second, common or "", f, s_, c)
                jan1 = new_year or datetime.date.fromordinal(day).timetuple().tm_yday - 1 in PLACEHOLDER_DAYS
                if k and days_apart_ok(k, abs(dd), jan1):
                    out.add(tid)
        return out

    # F's players by name and real date of birth.
    fidx, sidx, cidx = collections.defaultdict(list), collections.defaultdict(list), collections.defaultdict(list)
    frows = F.rows(6)
    for i, r in enumerate(frows):
        if r[0x5f] != 2:
            continue
        d, y = struct.unpack_from("<hH", r, 0x10)
        if y <= 1900:
            continue
        dob = (_md(d), y + fsh["birth"])
        c = _i(r, 0x0c)
        k = keys(ffn.get(_i(r, 4), ""), fsn.get(_i(r, 8), ""), fcn.get(c, "") if c >= 0 else "", dob)
        if any(x in have for x in k):
            continue
        try:
            fday = datetime.date(dob[1], *dob[0]).toordinal()
        except ValueError:
            fday = None
        if fday and in_template(ffn.get(_i(r, 4), ""), fsn.get(_i(r, 8), ""), fcn.get(c, "") if c >= 0 else "", fday):
            continue
        for x in k:
            fidx[x].append(i)
        last = norm(fsn.get(_i(r, 8), "")).split()
        if last:
            sidx[(last[-1], dob)].append(i)
        if _i(r, 0x39) >= 0:
            first = norm(ffn.get(_i(r, 4), "")).split()
            for w in swords(fsn.get(_i(r, 8), ""), fcn.get(c, "") if c >= 0 else ""):
                cidx[(_i(r, 0x39), w)].append((i, dob[1], first[0] if first else "",
                                               norm(fcn.get(c, "") if c >= 0 else "")))
    want, used = {}, set()
    found, taken = {}, set(pmap.values())
    for n, x in enumerate(fp):
        if n in matched or x["job"] != 1 or not x["dob"] or x["dob"].year <= 1900:
            continue
        if clubmap.get(x["club"]) not in in_div:
            continue
        # The template's own row first: the FM player takes it (convert.py moves it to his FM club).
        t = in_template(x["fn"], x["sn"], x["cn"], x["dob"].toordinal(), M["natmap"].get(x["nat"])) - taken
        if len(t) == 1:
            found[n] = t.pop()
            taken.add(found[n])
            continue
        dob = ((x["dob"].month, x["dob"].day), x["dob"].year)
        c = set()
        for k in keys(x["fn"], x["sn"], x["cn"], dob):
            c |= set(fidx.get(k, ()))
        if not c:
            last = norm(x["sn"]).split()
            for dd in (0, -1, 1):
                dt = x["dob"] + datetime.timedelta(days=dd)
                hit = sidx.get((last[-1] if last else "", ((dt.month, dt.day), dt.year)), [])
                if len(hit) == 1:
                    c = set(hit)
                    break
        if len(c) != 1:
            # F's club is the template club that the FM club maps to (F and the template share most club ids), and
            # one surname word is the same; the birth year two years either way. The first word of the first name, or
            # the common name, must agree too: a surname word alone took the older database's Sergio Ramos for FM's
            # Felipe Ramos of Real Madrid Castilla (forum report, 2026-10-21).
            t = clubmap[x["club"]]
            first = norm(x["fn"]).split()
            first = first[0] if first else ""
            c = set()
            for w in swords(x["sn"], x["cn"] or ""):
                c |= {i for i, y, f, cn in cidx.get((t, w), ())
                      if abs(y - x["dob"].year) <= 2 and ((first and f == first) or (cn and cn == norm(x["cn"] or "")))}
        if len(c) == 1:
            i = c.pop()
            if i not in used:
                want[n] = i
                used.add(i)
    print(f"fill: {len(found)} FM players found in the template itself (a date one or two days off, a name written "
          f"another way)")
    if not want:
        return found

    # Names: reuse the template's row with the same text (the same nation first), else append.
    def name_ids(t, fnames):
        rows = T.rows(t)
        by = collections.defaultdict(dict)
        for r in rows:
            by[cstr(r, 0, 51)][_i(r, 0x37)] = _i(r, 0x33)
        frow = {_i(r, 0x33): r for r in F.rows(t)}
        new, nxt, out = [], len(rows), {}

        def get(fid):
            if fid < 0 or fid not in frow:
                return fid
            if fid in out:
                return out[fid]
            r = frow[fid]
            txt, nat = cstr(r, 0, 51), _i(r, 0x37)
            hit = by.get(txt)
            if hit:
                v = hit.get(nat, next(iter(hit.values())))
            else:
                nonlocal nxt
                rr = bytearray(r)
                struct.pack_into("<i", rr, 0x33, nxt)
                new.append(rr)
                by[txt][nat] = nxt
                v = nxt
                nxt += 1
            out[fid] = v
            return v
        return get, new
    gf, nf = name_ids(13, ffn)
    gs, ns = name_ids(14, fsn)
    gc, nc = name_ids(15, fcn)

    # Clubs of F -> template clubs (careers).
    trows = {_i(r, 0): r for t in (0, 1) for r in T.rows(t)}
    tkey = {(cstr(r, 4, 51), _i(r, 0x53)): c for c, r in trows.items()}
    cmap = {}
    for t in (0, 1):
        for r in F.rows(t):
            c, nm = _i(r, 0), cstr(r, 4, 51)
            if c in trows and cstr(trows[c], 4, 51) == nm:
                cmap[c] = c
            else:
                cmap[c] = tkey.get((nm, _i(r, 0x53)), -1)

    for t in (6, 9, 10, 22):
        T.table(t)                                  # each staff.dat table lies inside the file
    staff_b = T.raw["staff.dat"]
    idx = T.idx
    n6, n9, n10, n22 = (idx[t]["count"] for t in (6, 9, 10, 22))
    fplayer = {_i(r, 0): r for r in F.rows(10)}
    new_staff, new_player, new_pref, out = [], [], [], {}
    fh = collections.defaultdict(list)
    fhist, _, _, n_fhist = F.table(17)              # checked against the file length
    wanted_f = set(want.values())
    for o in range(0, n_fhist * 17, 17):
        who = _i(fhist, o + 4)
        if who in wanted_f:
            fh[who].append(bytearray(fhist[o:o + 17]))
    hist_new = []
    for n, i in sorted(want.items()):
        sid, pid = n6 + len(new_staff), n10 + len(new_player)   # no gap in the ids when a person is skipped
        r = bytearray(frows[i])
        struct.pack_into("<i", r, 0, sid)
        struct.pack_into("<iii", r, 4, gf(_i(r, 4)), gs(_i(r, 8)), gc(_i(r, 0x0c)))
        for off, f_, t_ in ((0x12, fsh["birth"], tsh["birth"]), (0x18, fsh["birth"], tsh["birth"]),
                            (0x2b, fsh["contract"], tsh["contract"]), (0x33, fsh["contract"], tsh["contract"]),
                            (0x40, fsh["joined"], tsh["joined"]), (0x48, fsh["contract"], tsh["contract"])):
            y = struct.unpack_from("<H", r, off)[0]
            if y >= 1901:
                struct.pack_into("<H", r, off, y + f_ - t_)
        struct.pack_into("<i", r, 0x24, -1)
        struct.pack_into("<i", r, 0x39, -1)
        p = fplayer.get(_i(r, 0x61))
        if p is None:
            continue
        p = bytearray(p)
        struct.pack_into("<i", p, 0, pid)
        struct.pack_into("<iii", r, 0x61, pid, sid, -1)
        pref = bytearray(b"\xff" * 52)
        struct.pack_into("<i", pref, 0, sid)
        new_staff.append(r)
        new_player.append(p)
        new_pref.append(pref)
        out[n] = sid
        for h in fh.get(i, []):
            y = struct.unpack_from("<H", h, 8)[0] + fsh["staff_history"]
            club = cmap.get(_i(h, 0x0a), -1)
            if y >= S_.START or club < 0:
                continue
            struct.pack_into("<iH", h, 4, sid, y - tsh["staff_history"])
            struct.pack_into("<i", h, 0x0a, club)
            hist_new.append(h)

    # staff.dat: staff rows, non-player rows, player rows, preferences rows, in that order.
    o9, o10, o22 = idx[9]["off"], idx[10]["off"], idx[22]["off"]
    b6, b9 = staff_b[:n6 * 110], staff_b[o9:o9 + n9 * 68]
    b10, b22 = staff_b[o10:o10 + n10 * 70], staff_b[o22:o22 + n22 * 52]
    b6 = b6 + b"".join(new_staff)
    b10 = b10 + b"".join(new_player)
    b22 = b22 + b"".join(new_pref)
    ib = T.raw["index.dat"]
    _set_count(ib, 6, n6 + len(new_staff), 0)
    _set_count(ib, 9, n9, len(b6))
    _set_count(ib, 10, n10 + len(new_player), len(b6) + len(b9))
    _set_count(ib, 22, n22 + len(new_pref), len(b6) + len(b9) + len(b10))
    T.raw["staff.dat"] = bytearray(b6 + b9 + b10 + b22)
    for t, f, new in ((13, "first_names.dat", nf), (14, "second_names.dat", ns), (15, "common_names.dat", nc)):
        cnt = idx[t]["count"]
        T.raw[f] = bytearray(T.raw[f][:cnt * 60] + b"".join(new))
        _set_count(ib, t, cnt + len(new))
    sh = T.raw["staff_history.dat"]
    cnt = idx[17]["count"]
    rows = [bytearray(sh[o:o + 17]) for o in range(0, cnt * 17, 17)] + hist_new   # new staff ids come last
    for j, h in enumerate(rows):
        struct.pack_into("<i", h, 0, j)
    T.raw["staff_history.dat"] = bytearray(b"".join(rows))
    _set_count(ib, 17, len(rows))
    T.reindex()
    print(f"fill from {S_.FILL}: {len(want)} found, {len(out)} added, {len(nf)}/{len(ns)}/{len(nc)} new names, "
          f"{len(hist_new)} career rows")
    out.update(found)
    return out
