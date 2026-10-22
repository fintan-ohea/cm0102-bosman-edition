#!/usr/bin/env python3
"""Make the clean files: the free release of CM 01/02 with the official 3.9.68 patch applied.

Usage:
    python3 tools/clean_files/make_clean_files.py ISO PATCH_EXE OUT_DIR [--update-manifest]
    python3 tools/clean_files/make_clean_files.py --check OUT_DIR [--manifest FILE]
    python3 tools/clean_files/make_clean_files.py --data-3960 ISO DATA_DIR [--update-manifest]
    python3 tools/clean_files/make_clean_files.py --check DATA_DIR --manifest tools/clean_files/manifest-3960.tsv
    python3 tools/clean_files/make_clean_files.py --data-3965 PATCH_3965_EXE DATA_DIR [--update-manifest]

ISO is the free release (CM0102.iso). PATCH_EXE is the official patch (cm0102_3968.exe).
OUT_DIR must not exist. The tool stops if a hash of an input is wrong.
The tool writes the manifest of the new files next to OUT_DIR (OUT_DIR.manifest.tsv), so that the
file does not end up in a download. It then compares the files with the committed manifest and fails
with the paths that differ. It never writes over the committed manifest, except with
--update-manifest, which replaces the committed file on purpose.
With --check, the tool compares OUT_DIR with the manifest and changes nothing.

With --data-3960, the tool makes only the 3.9.60 data set (2001/02 data): the database files of the
free release (group "CM3 Data" of data1.cab) that the 3.9.68 patch replaces, that is the *.dat files
of PATCH_FILES, and the ten .lng files of the same group (the translated names of clubs, nations,
cities, comps and stadiums). DATA_DIR must not exist. The clean files and their manifest do not change.
The folder holds no other file, because the game reads every other Data file from the clean files. Its
manifest is tools/clean_files/manifest-3960.tsv. The runtime reads it with RECOMP_DATA_DIR=DATA_DIR.

With --data-3965, the tool makes only the 3.9.65 data set (2001/02 data, December update): the same
20 database files and the ten .lng files from the official 3.9.65 patch (cm0102v3965.exe, archived at
web.archive.org/web/20030430004731id_/http://www3.sigames.com/download/official/CM%2001-02/cm0102v3965.exe).
Its update.dat holds 47 sizes, then the files. Entries 14 to 33 are the database files in the order of
DATA_SET_FILES. Entries 34 to 43 are the .lng files in the order of UPDATE_3965_LNG. Entry 45 has a size
but no bytes in update.dat.

Why the .lng files: in a language other than English, database_load_language_names 0x5339a0 compares the
counts in the header of <lang>.lng with the database. The 3.9.68 .lng files fit only the 3.9.68 database
(6,682 cities, 8,917 stadiums). The 3.9.60 files hold 5,418 cities and 7,099 stadiums, the 3.9.65 files
5,464 and 7,161, the counts of their own databases. Its manifest is
tools/clean_files/manifest-3965.tsv. docs/research/2026-10-15-data-set-inventory.md has the facts.

A download holds each extra data set in the game folder as DATA_SET_FOLDERS names it. The option
"database" in the settings file chooses the data set for a new game.

Manifest format (tools/clean_files/manifest.tsv): lines starting with # are comments; each other
line is "sha256<TAB>size<TAB>path", with the path relative to OUT_DIR and "/" between folders.

Needs bsdtar (macOS has it) and unshield (Homebrew).

How the facts were found (2026-10-14, static analysis with the REA tools, no CrossOver run):
- update.dat: FUN_00403240 of the patch's setup.exe (the "-pack" path) writes 46 sizes, then the
  files. FUN_00403430 reads them back in the same order. The names and folders are the table at
  0x410070. The install loop at 0x40272a deletes each old file before it writes the new one.
- The patch asks "Delete the quick start files (recommended) ?" (FUN_00402ff0), so the clean files
  have no qstart folder.
- The folders of the cabinet's file groups come from the components in data1.hdr.
"""
import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_MANIFEST = os.path.join(HERE, "manifest.tsv")

ISO_SIZE = 303628288
ISO_SHA256 = "853134bbfc9e774424e62f9694ad7fe9dd7361c8bab93033f82fc906b31a5c53"
PATCH_SIZE = 23458816
PATCH_SHA256 = "b3f2b7d2ace6e86d0974e5ad68ae7a87a1e8fc3494c90d00cc3758392dcf24f1"

# File groups of data1.cab and the folder each one installs to. The folders come from the
# components in data1.hdr (<TARGETDIR>\Data and so on). Groups with no files (Exe, Help - *,
# Pictures Stadiums) are not listed. The quick start groups (~QS *) are left out, because the
# patch calls them out of date and recommends to delete them.
CAB_GROUPS = {
    "CM3 Data": "Data",
    "SI Data": "Data",
    "Tactics": "Data",
    "Pictures Std": "Pictures",
    "Pictures cfg": "Pictures",
    "Picture Tools": "",
    "Hall Of Fame": "",
    "History": "History",
    "Sounds": "Sounds",
    "Editor": "Editor",
}

# The file table of the patch's setup.exe (46 entries at 0x410070: name, folder). update.dat is
# 46 little-endian 32-bit sizes in this order, then the files one after the other.
PATCH_FILES = [
    ("", "cm0102.exe"), ("", "cm0102_GDI.exe"),
    ("Data", "dan.lng"), ("Data", "eng.lng"), ("Data", "esp.lng"), ("Data", "fra.lng"),
    ("Data", "ita.lng"), ("Data", "nor.lng"), ("Data", "por.lng"), ("Data", "swe.lng"),
    ("Data", "hol.lng"), ("Data", "ger.lng"), ("Data", "language.ldb"),
    ("Data", "player_setup.cfg"), ("Data", "weather.cfg"), ("", "changes.txt"),
    ("Data", "city.dat"), ("Data", "club.dat"), ("Data", "club_comp.dat"),
    ("Data", "club_comp_history.dat"), ("Data", "colour.dat"), ("Data", "common_names.dat"),
    ("Data", "continent.dat"), ("Data", "first_names.dat"), ("Data", "index.dat"),
    ("Data", "nat_club.dat"), ("Data", "nation.dat"), ("Data", "nation_comp.dat"),
    ("Data", "nation_comp_history.dat"), ("Data", "officials.dat"), ("Data", "second_names.dat"),
    ("Data", "stadium.dat"), ("Data", "staff.dat"), ("Data", "staff_comp.dat"),
    ("Data", "staff_comp_history.dat"), ("Data", "staff_history.dat"), ("", "hall_of_fame.bin"),
    ("Editor", "cm0102ed.CNT"), ("Editor", "cm0102ed.exe"), ("Editor", "cm0102ed.exe.manifest"),
    ("Editor", "CM0102ED.GID"), ("Editor", "Cm0102ed.hlp"), ("Editor", "faq.txt"),
    ("Editor", "WhatsNew.txt"), ("Editor", "ZipDll.dll"), ("Editor", "ZipSFX.bin"),
]


def fail(msg):
    sys.exit("make_clean_files: " + msg)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def check_input(path, size, digest, what):
    if not os.path.isfile(path):
        fail(f"{what} not found: {path}")
    if os.path.getsize(path) != size or sha256(path) != digest:
        fail(f"{what} has the wrong size or SHA-256: {path}")


def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        fail(f"{cmd[0]} failed: {r.stderr.strip() or r.stdout.strip()}")
    return r.stdout


def install_cab(work, out):
    """Extract the free release's InstallShield cabinets and lay the files out as setup does."""
    iso_dir = os.path.join(work, "iso")
    os.mkdir(iso_dir)
    run(["bsdtar", "-xf", ARGS.iso, "data1.cab", "data1.hdr"], iso_dir)
    count = 0
    for group, folder in CAB_GROUPS.items():
        gdir = tempfile.mkdtemp(dir=work)
        run(["unshield", "-g", group, "-j", "-d", gdir, "x", os.path.join(iso_dir, "data1.cab")], iso_dir)
        dest = os.path.join(out, folder)
        os.makedirs(dest, exist_ok=True)
        # unshield puts the files in a folder named after the group; each group is flat.
        for d, _, files in os.walk(gdir):
            for name in sorted(files):
                src, dst = os.path.join(d, name), os.path.join(dest, name)
                if os.path.exists(dst):
                    # Pictures Std and Pictures cfg both hold the same pics.cfg.
                    if sha256(src) != sha256(dst):
                        fail(f"two groups give different files for {folder}/{name}")
                    continue
                os.replace(src, dst)
                count += 1
    if count == 0:
        fail("no files came out of data1.cab")
    return count


def apply_patch(work, out):
    """Do what the patch's setup.exe does: write each file of update.dat over the old one."""
    patch_dir = os.path.join(work, "patch")
    os.mkdir(patch_dir)
    run(["bsdtar", "-xf", ARGS.patch, "update.dat"], patch_dir)
    with open(os.path.join(patch_dir, "update.dat"), "rb") as f:
        sizes = struct.unpack("<46I", f.read(46 * 4))
        if 46 * 4 + sum(sizes) != os.path.getsize(f.name):
            fail("update.dat does not have the expected format")
        for (folder, name), size in zip(PATCH_FILES, sizes):
            dest_dir = os.path.join(out, folder)
            os.makedirs(dest_dir, exist_ok=True)
            # setup.exe deletes the old file, then writes the new one with its own spelling.
            for old in os.listdir(dest_dir):
                if old.lower() == name.lower():
                    os.remove(os.path.join(dest_dir, old))
            with open(os.path.join(dest_dir, name), "wb") as g:
                g.write(f.read(size))


# The database files: the *.dat files that the patch replaces. The 3.9.60 data set is these files
# from the free release.
DATA_SET_FILES = [name for folder, name in PATCH_FILES if folder == "Data" and name.endswith(".dat")]
LNG_FILES = [name for folder, name in PATCH_FILES if folder == "Data" and name.endswith(".lng")]
DEFAULT_MANIFEST_3960 = os.path.join(HERE, "manifest-3960.tsv")
DEFAULT_MANIFEST_3965 = os.path.join(HERE, "manifest-3965.tsv")
# The folder of each extra data set in the game folder of a download (src/recomp_rt/win/files.c).
DATA_SET_FOLDERS = {"3.9.60": "Data-3.9.60", "3.9.65": "Data-3.9.65"}

PATCH_3965_SIZE = 19395072
PATCH_3965_SHA256 = "3e6c1a248eaf7602abfa2e397fb0571a00ec8ac27ca03d8b91db840c2fcfcbe4"
UPDATE_3965_COUNT = 47
UPDATE_3965_FIRST_DATA = 14
UPDATE_3965_EMPTY = 45      # a size with no bytes in update.dat
UPDATE_3965_FIRST_LNG = 34
# The language of entries 34 to 43, found by the names they share with the 3.9.68 .lng files.
UPDATE_3965_LNG = ["eng.lng", "esp.lng", "fra.lng", "ita.lng", "nor.lng", "por.lng", "swe.lng", "hol.lng",
                   "ger.lng", "dan.lng"]


def make_data_3960(out, manifest):
    """Extract the free release's own database files (3.9.60) into the new folder out."""
    parent = os.path.dirname(out)
    os.makedirs(parent, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=parent, prefix=".data-3960-") as work:
        iso_dir = os.path.join(work, "iso")
        os.mkdir(iso_dir)
        run(["bsdtar", "-xf", ARGS.iso, "data1.cab", "data1.hdr"], iso_dir)
        gdir = tempfile.mkdtemp(dir=work)
        run(["unshield", "-g", "CM3 Data", "-j", "-d", gdir, "x", os.path.join(iso_dir, "data1.cab")], iso_dir)
        found = {}
        for d, _, files in os.walk(gdir):
            for name in files:
                found[name.lower()] = os.path.join(d, name)
        stage = os.path.join(work, "out")
        os.mkdir(stage)
        for name in DATA_SET_FILES + LNG_FILES:
            if name.lower() not in found:
                fail(f"group CM3 Data of data1.cab has no {name}")
            os.replace(found[name.lower()], os.path.join(stage, name))
        os.replace(stage, out)
    title = "3.9.60 data set: the database and .lng files of CM0102.iso. Made by make_clean_files.py --data-3960."
    return finish_data_set(out, manifest, title)


def finish_data_set(out, manifest, title):
    rows = listing(out)
    own = out + ".manifest.tsv"
    write_manifest(rows, own, title)
    print(f"{len(rows)} files, {sum(r[1] for r in rows)} bytes in {out}; manifest of these files: {own}")
    if ARGS.update_manifest:
        write_manifest(rows, manifest, title)
        print(f"replaced the committed manifest {manifest}")
        return 0
    return check(out, manifest)


def make_data_3965(patch, out, manifest):
    """Extract the database files of the official 3.9.65 patch into the new folder out."""
    parent = os.path.dirname(out)
    os.makedirs(parent, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=parent, prefix=".data-3965-") as work:
        run(["bsdtar", "-xf", os.path.abspath(patch), "update.dat"], work)
        with open(os.path.join(work, "update.dat"), "rb") as f:
            sizes = struct.unpack(f"<{UPDATE_3965_COUNT}I", f.read(UPDATE_3965_COUNT * 4))
            if UPDATE_3965_COUNT * 4 + sum(sizes) - sizes[UPDATE_3965_EMPTY] != os.path.getsize(f.name):
                fail("update.dat of the 3.9.65 patch does not have the expected format")
            stage = os.path.join(work, "out")
            os.mkdir(stage)
            f.seek(UPDATE_3965_COUNT * 4 + sum(sizes[:UPDATE_3965_FIRST_DATA]))
            for i, name in enumerate(DATA_SET_FILES):
                with open(os.path.join(stage, name), "wb") as g:
                    g.write(f.read(sizes[UPDATE_3965_FIRST_DATA + i]))
            f.seek(UPDATE_3965_COUNT * 4 + sum(sizes[:UPDATE_3965_FIRST_LNG]))
            for i, name in enumerate(UPDATE_3965_LNG):
                with open(os.path.join(stage, name), "wb") as g:
                    g.write(f.read(sizes[UPDATE_3965_FIRST_LNG + i]))
        os.replace(stage, out)
    title = "3.9.65 data set: the database and .lng files of cm0102v3965.exe. Made by make_clean_files.py --data-3965."
    return finish_data_set(out, manifest, title)


def exe_version(path):
    """The version text that the game shows. The exe has no version resource, only the string
    "3.9.xx" in its data. Returns it if there is exactly one, else None."""
    data = open(path, "rb").read()
    found = set(re.findall(rb"\x003\.9\.(\d\d)\x00", data))
    return f"3.9.{found.pop().decode()}" if len(found) == 1 else None


def listing(root):
    rows = []
    for d, dirs, files in os.walk(root):
        dirs.sort()
        for name in files:
            p = os.path.join(d, name)
            rows.append((os.path.relpath(p, root), os.path.getsize(p), sha256(p)))
    return sorted(rows, key=lambda r: r[0].lower())


def write_manifest(rows, path, title="Clean files: CM0102.iso with cm0102_3968.exe applied. Made by make_clean_files.py."):
    with open(path, "w") as f:
        f.write(f"# {title}\n")
        f.write("# sha256\tsize\tpath\n")
        for rel, size, digest in rows:
            f.write(f"{digest}\t{size}\t{rel}\n")


def read_manifest(path):
    rows = []
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        digest, size, rel = line.rstrip("\n").split("\t", 2)
        rows.append((rel, int(size), digest))
    return rows


def check(out, manifest):
    want = read_manifest(manifest)
    have = listing(out)
    if have == want:
        print(f"ok: {len(have)} files match {manifest}")
        return 0
    wd, hd = {r[0]: r for r in want}, {r[0]: r for r in have}
    for rel in sorted(set(wd) | set(hd)):
        if wd.get(rel) != hd.get(rel):
            print("differs:", rel)
    return 1


def main():
    global ARGS
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", metavar="OUT_DIR")
    ap.add_argument("--data-3960", action="store_true", help="make only the 3.9.60 data set: ISO DATA_DIR")
    ap.add_argument("--data-3965", action="store_true", help="make only the 3.9.65 data set: PATCH_3965_EXE DATA_DIR")
    ap.add_argument("--manifest", help="the committed manifest to compare with")
    ap.add_argument("--update-manifest", action="store_true", help="replace the committed manifest")
    ap.add_argument("iso", nargs="?")
    ap.add_argument("patch", nargs="?")
    ap.add_argument("out", nargs="?")
    ARGS = ap.parse_args()
    if ARGS.check:
        return check(ARGS.check, ARGS.manifest or DEFAULT_MANIFEST)
    if ARGS.data_3960:
        if not ARGS.iso or not ARGS.patch or ARGS.out:
            ap.error("give ISO and DATA_DIR with --data-3960")
        for tool in ("bsdtar", "unshield"):
            if not shutil.which(tool):
                fail(f"{tool} not found (brew install {tool})")
        out = os.path.abspath(ARGS.patch)
        if os.path.exists(out):
            fail(f"{out} exists; give a new folder")
        check_input(ARGS.iso, ISO_SIZE, ISO_SHA256, "free release")
        return make_data_3960(out, ARGS.manifest or DEFAULT_MANIFEST_3960)
    if ARGS.data_3965:
        if not ARGS.iso or not ARGS.patch or ARGS.out:
            ap.error("give PATCH_3965_EXE and DATA_DIR with --data-3965")
        if not shutil.which("bsdtar"):
            fail("bsdtar not found")
        out = os.path.abspath(ARGS.patch)
        if os.path.exists(out):
            fail(f"{out} exists; give a new folder")
        check_input(ARGS.iso, PATCH_3965_SIZE, PATCH_3965_SHA256, "3.9.65 patch")
        return make_data_3965(ARGS.iso, out, ARGS.manifest or DEFAULT_MANIFEST_3965)
    ARGS.manifest = ARGS.manifest or DEFAULT_MANIFEST
    if not (ARGS.iso and ARGS.patch and ARGS.out):
        ap.error("give ISO, PATCH_EXE and OUT_DIR, or --check OUT_DIR")
    for tool in ("bsdtar", "unshield"):
        if not shutil.which(tool):
            fail(f"{tool} not found (brew install {tool})")
    out = os.path.abspath(ARGS.out)
    if os.path.exists(out):
        fail(f"{out} exists; give a new folder")
    check_input(ARGS.iso, ISO_SIZE, ISO_SHA256, "free release")
    check_input(ARGS.patch, PATCH_SIZE, PATCH_SHA256, "official patch")

    parent = os.path.dirname(out)
    os.makedirs(parent, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=parent, prefix=".clean-files-") as work:
        stage = os.path.join(work, "out")
        os.mkdir(stage)
        n = install_cab(work, stage)
        apply_patch(work, stage)
        version = exe_version(os.path.join(stage, "cm0102.exe"))
        if version != "3.9.68":
            fail(f"the patched cm0102.exe reports {version}, not 3.9.68")
        os.replace(stage, out)
    rows = listing(out)
    own = out + ".manifest.tsv"
    write_manifest(rows, own)
    print(f"{n} files from data1.cab, {len(PATCH_FILES)} from update.dat; {len(rows)} files in {out}")
    print(f"cm0102.exe version {version}; manifest of these files: {own}")
    if ARGS.update_manifest:
        write_manifest(rows, ARGS.manifest)
        print(f"replaced the committed manifest {ARGS.manifest}")
        return 0
    return check(out, ARGS.manifest)


if __name__ == "__main__":
    sys.exit(main())
