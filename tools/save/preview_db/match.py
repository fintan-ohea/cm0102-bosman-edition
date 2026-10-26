"""Matches the people and clubs of May 2026 and a database folder made by save_to_db.py.

Usage: python3 -I match.py <this folder> <May folder> <preview folder> <out match.pkl>
People: first, second and common name and birth date one year earlier (the preview is one data season later).
Clubs: same id and name, else name and nation.
"""
import sys, collections, pickle
sys.path[:0] = [sys.argv[1], sys.argv[1] + "/.."]   # this folder, and tools/save for savfile.py
from dbread import load, sname, i32, i16
may = load(sys.argv[2]); pre = load(sys.argv[3])
def people(t):
    fn = [sname(r, 0, 51) for r in t[13]]; sn = [sname(r, 0, 51) for r in t[14]]; cn = [sname(r, 0, 51) for r in t[15]]
    g = lambda tab, k: tab[k] if 0 <= k < len(tab) else ""
    keys = []
    for r in t[6]:
        keys.append((g(fn, i32(r, 4)), g(sn, i32(r, 8)), g(cn, i32(r, 0xc)), i16(r, 0x10), i16(r, 0x12), i32(r, 0x1a)))
    return keys
km, kp = people(may), people(pre)
idx = collections.defaultdict(list)
for i, k in enumerate(kp):
    idx[(k[0], k[1], k[2], k[3], k[4] + 1)].append(i)   # preview year is one earlier
m2p = {}; amb = 0
for i, k in enumerate(km):
    c = idx.get((k[0], k[1], k[2], k[3], k[4]), [])
    if len(c) > 1:
        c = [j for j in c if kp[j][5] == k[5]] or c
    if len(c) == 1: m2p[i] = c[0]
    elif len(c) > 1: amb += 1
# second key: name without the day of birth (day may move), same nation, year +-0
idx2 = collections.defaultdict(list)
used = set(m2p.values())
for j, k in enumerate(kp):
    if j not in used: idx2[(k[0], k[1], k[2], k[4] + 1, k[5])].append(j)
for i, k in enumerate(km):
    if i in m2p: continue
    c = idx2.get((k[0], k[1], k[2], k[4], k[5]), [])
    if len(c) == 1 and c[0] not in used: m2p[i] = c[0]; used.add(c[0])
print("may staff", len(km), "preview staff", len(kp), "matched", len(m2p), "ambiguous", amb)
print("may only", len(km) - len(m2p), "preview only", len(kp) - len(set(m2p.values())))
# clubs by id+name, else name+nation
def clubkeys(t): return [(sname(r, 4, 55), i32(r, 0x53)) for r in t[0]]
cm, cp = clubkeys(may), clubkeys(pre)
byname = collections.defaultdict(list)
for j, k in enumerate(cp): byname[k].append(j)
c2p = {}
for i, k in enumerate(cm):
    if i < len(cp) and cp[i] == k: c2p[i] = i; continue
    c = byname.get(k, [])
    if len(c) == 1: c2p[i] = c[0]
print("may clubs", len(cm), "preview clubs", len(cp), "matched", len(c2p), "same id", sum(1 for a, b in c2p.items() if a == b))
pickle.dump({"m2p": m2p, "c2p": c2p}, open(sys.argv[4], "wb"))
