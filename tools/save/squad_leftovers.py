#!/usr/bin/env python3
"""List, and optionally remove, the leftover players of a club's squad list in a CM 01/02 saved game.

Usage:
    python3 tools/save/squad_leftovers.py <in.sav> --club "Manchester United"              (list only)
    python3 tools/save/squad_leftovers.py <in.sav> --club "Manchester United" --out <out.sav> [--only 22442 ...]

A leftover is a player in the club's squad list who has no contract with the club: his only contract, a full one,
is with another club, which also lists him. Update Team data gives a player on loan in real life a full contract
at the loan club, and the parent club's list keeps him, so he shows in the parent's squad but never comes back.
The tool empties those slots and changes nothing else. It never changes the input file, and it stops if the
output file exists.

Where the data is (cm0102.exe 3.9.68):
- club.dat: 0x245-byte club records, id at +0, name at +4. The squad list is 50 staff ids at +0xd7, -1 for an
  empty slot (50 pointers in memory, Finance::get_transfer_budget 0x598800 reads them at 0x59884c..0x59888b).
- staff.dat: 110-byte staff records, id at +0, first and second name ids at +4 and +8, club id at +0x39.
- contract.dat: int count, count * 21 bytes, int count, then 80-byte contract records (CONTRACT_MANAGER::
  load_from_disk 0x4db5c0: the 21-byte part at 0x4db6b0, the records at 0x4db7eb and 0x4db854). A record holds
  the staff id at +0 (-1 for a free record, CONTRACT_MANAGER::end_loan 0x4d0994), the club id at +4 and the type in
  the low 6 bits of +0x35 (6 and 7 loan, the profile's Contract tab 0x87ceeb).
- database_remove_staff_from_club 0x53c190 takes a player out of the squad list by clearing his slot
  (0x53c500); it does not move the slots after it. The tool does the same.
"""
import argparse
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import savfile  # noqa: E402

CLUB_SIZE = 0x245
SQUAD = 0xD7
STAFF_SIZE = 110
CONTRACT_SIZE = 80


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def names(sav, member):
    d = sav.read_member(member)
    return [d[i:i + 51].split(b"\0")[0].decode("latin-1") for i in range(0, len(d), 60)]


def contracts(sav):
    d = sav.read_member("contract.dat")
    first = i32(d, 0)
    o = 4 + first * 21
    count = i32(d, o)
    o += 4
    if o + count * CONTRACT_SIZE > len(d):
        sys.exit("stop: contract.dat does not have the expected layout")
    out = collections.defaultdict(list)
    for k in range(count):
        r = o + k * CONTRACT_SIZE
        out[i32(d, r)].append((i32(d, r + 4), d[r + 0x35] & 0x3F))
    return out


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("input")
    p.add_argument("--club", required=True)
    p.add_argument("--out", help="write a new save without the leftovers")
    p.add_argument("--only", type=int, nargs="*", help="remove only these staff ids")
    a = p.parse_args(argv)
    if a.out and os.path.exists(a.out):
        sys.exit(f"stop: {a.out} exists")

    sav = savfile.Sav(a.input)
    clubs = bytearray(sav.read_member("club.dat"))
    rows = [k for k in range(len(clubs) // CLUB_SIZE)
            if clubs[k * CLUB_SIZE + 4:k * CLUB_SIZE + 0x37].split(b"\0")[0].decode("latin-1").lower() == a.club.lower()]
    if len(rows) != 1:
        sys.exit(f"stop: {len(rows)} clubs are named {a.club!r}")
    base = rows[0] * CLUB_SIZE
    club_id = i32(clubs, base)
    club_name = {i32(clubs, k): clubs[k + 4:k + 0x37].split(b"\0")[0].decode("latin-1")
                 for k in range(0, len(clubs), CLUB_SIZE)}
    sd = sav.read_member("staff.dat")
    staff = {i32(sd, k): sd[k:k + STAFF_SIZE] for k in range(0, len(sd), STAFF_SIZE)}
    first, second = names(sav, "first_names.dat"), names(sav, "second_names.dat")
    con = contracts(sav)

    leftovers = []
    for slot in range(50):
        sid = i32(clubs, base + SQUAD + 4 * slot)
        if sid < 0 or sid not in staff:
            continue
        mine = [c for c in con.get(sid, []) if c[0] == club_id]
        if mine:
            continue
        r = staff[sid]
        name = f"{first[i32(r, 4)]} {second[i32(r, 8)]}"
        held = ", ".join(f"{club_name.get(c, c)} (type {t})" for c, t in con.get(sid, [])) or "no contract"
        leftovers.append((slot, sid, name))
        print(f"slot {slot:2}  staff {sid:6}  {name}: plays for {club_name.get(i32(r, 0x39))}, contract {held}")
    if not leftovers:
        print(f"{club_name[club_id]}: no leftovers")
        return 0
    if not a.out:
        return 0

    removed = 0
    for slot, sid, name in leftovers:
        if a.only and sid not in a.only:
            continue
        struct.pack_into("<i", clubs, base + SQUAD + 4 * slot, -1)
        removed += 1
    sav.write_member("club.dat", clubs)
    sav.save(a.out)
    check = savfile.Sav(a.out).read_member("club.dat")
    left = [i32(check, base + SQUAD + 4 * s) for s, _, _ in leftovers if not a.only or i32(clubs, base + SQUAD + 4 * s) == -1]
    print(f"wrote {a.out}: {removed} slots emptied")
    return 0 if all(x == -1 for x in left) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
