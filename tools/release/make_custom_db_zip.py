#!/usr/bin/env python3
"""Makes the zip of a custom database for the downloads (Seasons/Custom/<name>.zip).

Usage:
    python3 -I tools/release/make_custom_db_zip.py <database folder> <out folder> [--name <name>]

The zip holds one folder <name> with the 20 data files of the game (the list of make_season_zips.py), as the
season database zips do. <name> defaults to the folder's name and must be a plain name (letters, digits, dots
and spaces). The game unpacks the zip once into Databases/<name> of the save folder, where the Choose Season
screen lists it on the Custom tile (src/recomp_rt/win/seasons_db.c, seasons_db_unpack_bundled_in). The zip is
the same for the same input. The tool reads the new zip back and compares each file with the folder; a
difference stops it with exit code 1 and leaves no zip.

For the Update Team's 2026 preview: build the folder with tools/save/preview_db/build_preview_db.py first
(docs/research/2026-10-26-database-from-save.md), then:
    python3 -I tools/release/make_custom_db_zip.py <new folder> <custom dbs folder> --name "2026 Preview Data Update"
and give <custom dbs folder> to make_release.sh as --custom-dbs.
"""
import argparse
import hashlib
import os
import shutil
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_season_zips import CHUNK, DATA_FILES, NEUTRAL_NAME, STAMP  # noqa: E402


def files_of(folder):
    """{lower case name: path} of the data files of the folder, whatever their case."""
    out = {}
    for f in os.listdir(folder):
        if f.lower() in DATA_FILES and os.path.isfile(os.path.join(folder, f)):
            out[f.lower()] = os.path.join(folder, f)
    return out


def sha_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(CHUNK), b""):
            h.update(block)
    return h.hexdigest()


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("folder")
    ap.add_argument("out")
    ap.add_argument("--name")
    a = ap.parse_args(argv)
    name = a.name or os.path.basename(os.path.normpath(a.folder))
    if not NEUTRAL_NAME.match(name):
        print(f"make_custom_db_zip: {name!r} is not a plain name (letters, digits, dots and spaces)", file=sys.stderr)
        return 1
    files = files_of(a.folder)
    missing = [f for f in DATA_FILES if f not in files]
    if missing:
        print(f"make_custom_db_zip: {a.folder} lacks {', '.join(missing)}", file=sys.stderr)
        return 1
    os.makedirs(a.out, exist_ok=True)
    dest = os.path.join(a.out, name + ".zip")
    tmp = dest + ".tmp"
    try:
        with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as out:
            for f in DATA_FILES:
                zi = zipfile.ZipInfo(f"{name}/{f}", STAMP)
                zi.compress_type = zipfile.ZIP_DEFLATED
                zi.external_attr = 0o644 << 16
                zi.create_system = 3
                with open(files[f], "rb") as r, out.open(zi, "w") as w:
                    shutil.copyfileobj(r, w, CHUNK)
        with zipfile.ZipFile(tmp) as z:
            if sorted(z.namelist()) != sorted(f"{name}/{f}" for f in DATA_FILES):
                raise ValueError("the zip holds other entries")
            for f in DATA_FILES:
                h = hashlib.sha256()
                with z.open(f"{name}/{f}") as r:
                    for block in iter(lambda: r.read(CHUNK), b""):
                        h.update(block)
                if h.hexdigest() != sha_file(files[f]):
                    raise ValueError(f"{f} differs from the folder")
        os.replace(tmp, dest)
    except BaseException as e:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        if isinstance(e, ValueError):
            print(f"make_custom_db_zip: {e}", file=sys.stderr)
            return 1
        raise
    print(f"{dest}: {os.path.getsize(dest)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
