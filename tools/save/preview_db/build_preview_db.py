"""One command: preview save + May 2026 folder -> a new data folder.

Usage: python3 -I build_preview_db.py <save.sav> <May folder> <new folder>
Runs save_to_db.py, match.py and merge.py from this folder, with the in-between files in a temporary folder.
"""
import os, subprocess, sys, tempfile

here = os.path.dirname(os.path.abspath(__file__))
save, may, out = sys.argv[1:4]
if os.path.exists(out):
    sys.exit(f"{out} exists; pick a new folder")
with tempfile.TemporaryDirectory() as tmp:
    raw, match = os.path.join(tmp, "from-save"), os.path.join(tmp, "match.pkl")
    for step in (["save_to_db.py", save, may, raw], ["match.py", may, raw, match],
                 ["merge.py", may, raw, match, out]):
        subprocess.run([sys.executable, "-I", os.path.join(here, step[0]), here] + step[1:], check=True)
print(f"done: {out}")
