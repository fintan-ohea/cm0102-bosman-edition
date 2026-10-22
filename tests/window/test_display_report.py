#!/usr/bin/env python3
"""The display report (display_report.c): written at start, on the report key and at quit.

Usage: python3 tests/window/test_display_report.py build/cm0102_recomp /Users/max/Projects/cm0102/clean-3968
Each run uses a new temporary save folder."""
import os
import subprocess
import sys
import tempfile


def main(argv):
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    failures = []

    def check(name, ok):
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failures.append(name)

    with tempfile.TemporaryDirectory() as user:
        report = os.path.join(user, "display-report.txt")
        env = dict(os.environ, RECOMP_USER_DIR=user, RECOMP_SCRIPT="3000 resize 1300 820; 4000 report; 5000 quit")
        err = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True,
                             timeout=120).stderr
        check("written at start", "display report written (at start)" in err)
        check("written on the report key", "display report written (on the report key)" in err)
        check("written at quit", "display report written (at quit)" in err)
        text = open(report).read() if os.path.exists(report) else ""
        for part in ("System: ", "Screens (SDL):", "Now: window 1300x820", "Last window changes",
                     "Settings: display = widescreen", "Game folder: "):
            check("report has " + part, part in text)
        check("the change log has the resize", "size changed: window 1300x820" in text)
        check("1300x820 is above a fit of 1: the report says sharp bilinear", "sharp: " in text)
        check("the change log has the report key", "report key: window 1300x820" in text)
        home = os.path.expanduser("~")
        name = os.path.basename(home)
        check("no account name in the paths", home not in text and ("/%s/" % name) not in text)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
