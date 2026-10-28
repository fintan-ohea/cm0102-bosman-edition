#!/usr/bin/env python3
"""Picks the benchmark tactics that the downloads carry in the tactics folder of the game folder.

Usage:
    python3 -I tools/release/make_benchmark_tactics.py <benchmark checkout> <out folder> [--count 800]

<benchmark checkout> is a checkout of https://gitlab.com/nakre/cm0102-tactic-benchmark with "CM Tactics.xlsx"
and the .tct files of "tested tactics" (a sparse checkout of those is enough, about 30 MB). The tool takes the
<count> tactics with the best average points in the benchmark (Sheet2 of CM Tactics.xlsx, ties on the average
goal difference, then the name) and copies their .tct files to <out folder>/tactics, with the credits file
"Benchmark tactics.txt" (rank, name, creator, formation, average points and forum link of each). Give
<out folder> to make_release.sh as --tactics.

The game lists the .tct files of <game folder>\\tactics in the Load dialog of the tactics screen (the folder of
utils_get_user_folder 0x90bea0, the dialog of tactics_screen_handle_event 0x89f710 with the extension .tct at
0x8a04cd). Each file takes one display item, and the table holds 1199 (Display::add_item 0x55f990): with about
1075 files the dialog has no room left for its Ok and Cancel buttons. 800 leaves room for about 260 tactics
of the player's own (docs/research/2026-10-28-benchmark-tactics.md).

A file name that is not plain ASCII is written in ASCII (Estándar becomes Estandar), so the manifest path is the
same on every system. A file whose version is outside the loader's range (0x98ec54 to 0x98ec5e,
Formation::load_from_disk 0x5b5d10) is left out. The checkout is data from the internet: the tool only reads
it. The out folder must not exist.
"""
import argparse
import os
import re
import shutil
import struct
import sys
import unicodedata
import xml.etree.ElementTree as ET
import zipfile

NS = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
VERSIONS = range(0x98EC54, 0x98EC5F)
CREDITS = "Benchmark tactics.txt"


def sheets(path):
    """{sheet name: [{column letter: text}]} of an .xlsx file."""
    z = zipfile.ZipFile(path)
    shared = []
    if "xl/sharedStrings.xml" in z.namelist():
        for si in ET.fromstring(z.read("xl/sharedStrings.xml")).iter(NS + "si"):
            shared.append("".join(t.text or "" for t in si.iter(NS + "t")))
    names = [s.get("name") for s in ET.fromstring(z.read("xl/workbook.xml")).iter(NS + "sheet")]
    files = sorted((n for n in z.namelist() if re.fullmatch(r"xl/worksheets/sheet\d+\.xml", n)),
                   key=lambda n: int(re.search(r"\d+", n).group()))
    out = {}
    for name, f in zip(names, files):
        rows = []
        for r in ET.fromstring(z.read(f)).iter(NS + "row"):
            row = {}
            for c in r.iter(NS + "c"):
                v = c.find(NS + "v")
                if v is None:
                    t = c.find(".//" + NS + "t")
                    value = t.text if t is not None else None
                else:
                    value = shared[int(v.text)] if c.get("t") == "s" else v.text
                row[re.sub(r"\d", "", c.get("r"))] = value
            rows.append(row)
        out[name] = rows
    return out


def key(name):
    return unicodedata.normalize("NFC", name).strip().lower()


def ascii_name(name):
    """The name in plain ASCII: accents dropped, other signs removed."""
    s = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode()
    return re.sub(r"\s+", " ", s).strip()


def number(text, default=float("-inf")):
    try:
        return float(text)
    except (TypeError, ValueError):
        return default


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("checkout")
    ap.add_argument("out")
    ap.add_argument("--count", type=int, default=800)
    a = ap.parse_args(argv)
    if os.path.exists(a.out):
        sys.exit(f"make_benchmark_tactics.py: {a.out} exists.")

    files = {}
    tested = os.path.join(a.checkout, "tested tactics")
    for folder, _, names in os.walk(tested):
        for f in names:
            if f.lower().endswith(".tct"):
                files.setdefault(key(f[:-4]), os.path.join(folder, f))
                files.setdefault(key(os.path.basename(folder)), os.path.join(folder, f))
    book = sheets(os.path.join(a.checkout, "CM Tactics.xlsx"))
    links = {key(r["B"]): r.get("AL") for r in book["Sheet1"][1:] if r.get("B")}
    ranked = [r for r in book["Sheet2"][1:] if r.get("T") and r.get("AA")]
    ranked.sort(key=lambda r: (-number(r["AA"]), -number(r.get("Z")), key(r["T"])))

    picked, seen, skipped = [], set(), []
    for r in ranked:
        if len(picked) == a.count:
            break
        path = files.get(key(r["T"]))
        if not path:
            skipped.append((r["T"], "no .tct file"))
            continue
        data = open(path, "rb").read()
        if len(data) < 4 or struct.unpack_from("<I", data)[0] not in VERSIONS:
            skipped.append((r["T"], "version outside the loader's range"))
            continue
        name = ascii_name(os.path.basename(path)[:-4])
        if not name or key(name) in seen:
            skipped.append((r["T"], "empty or repeated file name"))
            continue
        seen.add(key(name))
        picked.append((r, path, name))
    if len(picked) < a.count:
        sys.exit(f"make_benchmark_tactics.py: only {len(picked)} tactics, {a.count} wanted.")

    dest = os.path.join(a.out, "tactics")
    os.makedirs(dest)
    lines = [
        "Benchmark tactics",
        "",
        f"The {a.count} tactics in this folder are the best of the tactics benchmark of nakre and the champman0102.net",
        "community: https://gitlab.com/nakre/cm0102-tactic-benchmark. Each tactic played 100 seasons with the same",
        "Greek league team against the AI tactics of patch 3.9.68. The tactics are the work of their creators, named",
        "below. Load one on the tactics screen: Tactics, Load.",
        "",
        "Rank\tName\tCreator\tFormation\tAverage points\tLink",
    ]
    for rank, (r, path, name) in enumerate(picked, 1):
        shutil.copyfile(path, os.path.join(dest, name + ".tct"))
        lines.append("\t".join([str(rank), name, r.get("U") or "", r.get("V") or "", f"{number(r['AA']):.2f}",
                                links.get(key(r["T"])) or ""]))
    with open(os.path.join(dest, CREDITS), "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(picked)} tactics in {dest}, average points {number(picked[0][0]['AA']):.2f} to"
          f" {number(picked[-1][0]['AA']):.2f};"
          f" {len(skipped)} passed over: {skipped[:5]}")


if __name__ == "__main__":
    main(sys.argv[1:])
