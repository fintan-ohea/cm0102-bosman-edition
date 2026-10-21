#!/usr/bin/env python3
"""Options box check (spec 2026-10-16-features-0.5.0, slice 6; #56): the Bosman Edition Options box.

Usage:
    python3 tests/smoke/options_box_run.py <cm0102_recomp> <game folder> <out folder> [--modes classic,widescreen]
        [--dir <fixtures folder>]

Runs at the same time:
    side-<mode>   for each mode: loads start.sav (tests/fixtures) and opens the profile of Vieira from the
                  Arsenal squad. On the profile it opens the box from the side bar drop-down "Game Options",
                  switches attribute_colours off on the Graphics tab and save_backups off on the Game tab, and
                  closes the box. The profile below is built again.
    tactics       classic: opens the box from the side bar on the tactics screen (0x899ea0), whose input does
                  not pass a click to the side bar: the main loop handles the item (options_screen.c).
    readonly      classic: opens the box, makes the user folder read only, switches attribute_colours off.
    start         widescreen, a new user folder with database = folder:My Update: opens the box from the
                  start screen, sets the display mode to Classic, switches Sharp picture off and on (applied at
                  once, user32.c) and closes the box. Then a second start with the same user folder.

Checks:
    - side runs: settings.txt has attribute_colours = off and save_backups = off, and every other line is as
      before (the comments too). The profile before the box shows at least 20 attribute values with their
      attribute colours; the profile built again after the box closes shows them with none
      (RECOMP_COLOUR_LOG=1, columns.c).
    - tactics: the box opens from the side bar on the tactics screen.
    - readonly: settings.txt keeps attribute_colours = on, the option keeps its old value in memory, and the
      box shows the note "Not saved" (picture readonly.bmp).
    - start: settings.txt has display = classic and sharp_picture = on after the first start, the first start
      draws at 1280x800, and the second start draws at 800x600 (the restart note of the display mode).
Pictures (with .png copies) in <out>/<run>/: screen.bmp and game.bmp (the two tabs), screen-off.bmp and
game-off.bmp (after the clicks), profile-before.bmp and profile.bmp; tactics/box.bmp; readonly/readonly.bmp;
start/box.bmp, start/game.bmp and start/second.bmp.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

# Positions for 800x600. The box scales as every area does. The side bar drop-down items keep their size,
# so the item has its own widescreen position after "|". Box rows: y = 192 + 35 * row; On and the
# first value at x 371, Off and the second value at 452; tabs Screen 304, Game 584 at y 155; Close 444, 524.
GAME_OPTIONS = "click 45 344"
OPTIONS_ITEM = "click 175 417 | 225 525"
SIDE_STEPS = [
    "1000 click 45 165",                        # the manager's menu
    "2500 click 150 156 | 199 203",   # Arsenal Squad
    "6000 click 210 369 | 336 492",   # Vieira
    "9000 dump {d}/profile-before.bmp",
    "10000 " + GAME_OPTIONS,
    "12000 " + OPTIONS_ITEM,                    # Bosman Edition Options
    "14000 dump {d}/screen.bmp",
    "15000 click 452 332",                      # Attribute colours: Off
    "16500 dump {d}/screen-off.bmp",
    "17500 click 584 155",                      # the Game tab
    "19000 dump {d}/game.bmp",
    "20000 click 452 402",                      # Save backups: Off
    "21500 dump {d}/game-off.bmp",
    "22500 click 444 524",                      # Close
    "25000 dump {d}/profile.bmp",
    "26000 quit",
]
TACTICS_STEPS = [
    "1000 click 45 165",                        # the manager's menu
    "2500 click 150 156",                       # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 169 527",                       # Tactics
    "wait screen:0x899ea0",
    "1500 dump {d}/tactics.bmp",
    "2000 " + GAME_OPTIONS,
    "3500 dump {d}/drop-down.bmp",
    "4000 " + OPTIONS_ITEM,
    "6000 dump {d}/box.bmp",
    "7000 click 444 524",                       # Close
    "wait screen:0x899ea0",
    "1500 quit",
]
READONLY_STEPS = [
    "1000 " + GAME_OPTIONS,
    "3000 " + OPTIONS_ITEM,
    "9000 click 452 332",                       # Attribute colours: Off, after the folder is read only
    "11000 dump {d}/readonly.bmp",
    "12000 quit",
]
START_STEPS = fixtures.LOAD_STEPS[:fixtures.LOAD_STEPS.index("wait setup") + 1] + [
    "1000 dump {d}/setup.bmp",
    "1500 click 277 425",                       # Bosman Edition Options (menu B: left cell of the fourth row)
    "3500 dump {d}/box.bmp",
    "4000 click 452 192",                       # Display mode: Classic
    "4500 click 452 227",                       # Sharp picture: Off
    "5000 dump {d}/box-sharp-off.bmp",
    "5500 click 371 227",                       # Sharp picture: On
    "6000 click 584 155",                       # the Game tab
    "7000 dump {d}/game.bmp",
    "7500 click 444 524",                       # Close
    "wait setup",
    "1000 quit",
]
SECOND_STEPS = ["1000 click 400 300", "wait setup", "1500 dump {d}/second.bmp", "2000 quit"]

VALUE = re.compile(r'recomp: profile value "([^"]*)" colour (0x[0-9a-f]+)')
BAND_COLOURS = {0xFA8A, 0xFD84, 0xA70C, 0x37E6}      # columns.c COLOUR_*


def size(bmp):
    return struct.unpack_from("<ii", open(bmp, "rb").read(), 18)


def png(folder):
    for f in sorted(os.listdir(folder)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(folder, f), "--out",
                            os.path.join(folder, f[:-4] + ".png")], capture_output=True)


def play(program, game, mode, user, out, steps, env=None):
    os.makedirs(out, exist_ok=True)
    r, _ = fixtures.run(program, game, mode, user, smoke_run.scaled_steps(mode, out, steps), env or {}, 600)
    with open(os.path.join(out, "stderr.txt"), "a") as f:
        f.write(r.stderr)
    png(out)
    return r.returncode


def readonly_run(program, game, folder, out, work):
    """Plays READONLY_STEPS and makes the user folder read only once the box is open."""
    import time
    user = os.path.join(work, "readonly", "user")
    home = os.path.join(work, "readonly", "home")
    os.makedirs(user)
    os.makedirs(home)
    os.makedirs(out, exist_ok=True)
    shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, "classic", [])
    steps = smoke_run.scaled_steps("classic", out, fixtures.LOAD_STEPS + READONLY_STEPS)
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCRIPT="; ".join(steps))
    log_path = os.path.join(out, "stderr.txt")
    with open(log_path, "w") as log:
        proc = subprocess.Popen([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                stderr=log)
        made_readonly = False
        while proc.poll() is None:
            if not made_readonly and "recomp: options box opened" in open(log_path, errors="replace").read():
                os.chmod(user, 0o555)
                made_readonly = True
            time.sleep(0.1)
    os.chmod(user, 0o755)
    png(out)
    return proc.returncode, made_readonly, open(os.path.join(user, "settings.txt")).read()


def changed_lines(before, after):
    """The lines that differ, as (old, new) pairs, when the files have the same number of lines; else None."""
    a, b = before.splitlines(), after.splitlines()
    if len(a) != len(b):
        return None
    return [(x, y) for x, y in zip(a, b) if x != y]


def side_run(program, game, folder, mode, out, work):
    user = os.path.join(work, "side-" + mode, "user")
    os.makedirs(user)
    shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, mode, [])
    steps = [s for s in fixtures.LOAD_STEPS] + SIDE_STEPS
    # The first start writes the missing option lines; keep that file to compare with.
    code = play(program, game, mode, user, out, steps, {"RECOMP_COLOUR_LOG": "1"})
    return code, user


def main(argv):
    args = argv[1:]
    opts = {"--modes": "classic,widescreen", "--dir": fixtures.DEFAULT_DIR}
    rest = []
    i = 0
    while i < len(args):
        if args[i] in opts:
            opts[args[i]] = args[i + 1]
            i += 2
        else:
            rest.append(args[i])
            i += 1
    if len(rest) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(p) for p in rest)
    modes = opts["--modes"].split(",")
    work = tempfile.mkdtemp(prefix="cm0102-options-box-")
    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS" if passed else "FAIL") + ": " + text)
        ok &= bool(passed)

    # The settings file as the first start writes it: the same steps up to the load, with no box.
    def reference(mode):
        user = os.path.join(work, "ref-" + mode, "user")
        os.makedirs(user)
        fixtures.write_settings(user, mode, [])
        play(program, game, mode, user, os.path.join(work, "ref-" + mode, "out"), ["1000 quit"])
        return open(os.path.join(user, "settings.txt")).read()

    def start_run():
        user = os.path.join(work, "start", "user")
        os.makedirs(user)
        fixtures.write_settings(user, "widescreen", [("database", "folder:My Update")])
        s = os.path.join(out, "start")
        c1 = play(program, game, "widescreen", user, s, START_STEPS)
        after_first = open(os.path.join(user, "settings.txt")).read()
        c2 = play(program, game, "classic", user, s, SECOND_STEPS)
        return c1, c2, after_first

    def tactics_run():
        user = os.path.join(work, "tactics", "user")
        os.makedirs(user)
        shutil.copyfile(os.path.join(opts["--dir"], "start.sav"), os.path.join(user, "start.sav"))
        fixtures.write_settings(user, "classic", [])
        return play(program, game, "classic", user, os.path.join(out, "tactics"), fixtures.LOAD_STEPS + TACTICS_STEPS)

    with ThreadPoolExecutor(2 * len(modes) + 3) as pool:
        refs = {m: pool.submit(reference, m) for m in modes}
        sides = {m: pool.submit(side_run, program, game, opts["--dir"], m, os.path.join(out, "side-" + m), work)
                 for m in modes}
        start = pool.submit(start_run)
        tactics = pool.submit(tactics_run)
        readonly = pool.submit(readonly_run, program, game, opts["--dir"], os.path.join(out, "readonly"), work)
        for m in modes:
            code, user = sides[m].result()
            check(code == 0, f"side-{m}: the run ends with code 0")
            before, after = refs[m].result(), open(os.path.join(user, "settings.txt")).read()
            diff = changed_lines(before, after)
            check(diff is not None and sorted(diff) == [("attribute_colours = on", "attribute_colours = off"),
                                                        ("save_backups = on", "save_backups = off")],
                  f"side-{m}: settings.txt changes only attribute_colours and save_backups to off ({diff})")
            log = open(os.path.join(out, "side-" + m, "stderr.txt"), errors="replace").read()
            check("recomp: options box opened" in log, f"side-{m}: the box opens from the side bar")
            before_box, _, after_box = log.partition("recomp: options box opened")
            after_close = after_box.split("recomp: option save_backups", 1)[-1]

            def values_of(text):
                return [(int(t), int(c, 16)) for t, c in VALUE.findall(text) if t.strip().isdigit()
                        and 1 <= int(t) <= 20]
            first, again = values_of(before_box), values_of(after_close)
            check(len(first) >= 20 and all(c in BAND_COLOURS for _, c in first),
                  f"side-{m}: the profile before the box shows {len(first)} values, all with attribute colours")
            check(len(again) >= 20 and not any(c in BAND_COLOURS for _, c in again),
                  f"side-{m}: the profile built again after the box shows {len(again)} values, "
                  f"{sum(c in BAND_COLOURS for _, c in again)} with an attribute colour")
        c1, c2, after_first = start.result()
        check(c1 == 0 and c2 == 0, "start: both starts end with code 0")
        lines = after_first.splitlines()
        check("display = classic" in lines and "sharp_picture = on" in lines,
              "start: settings.txt has display = classic and sharp_picture = on")
        s = os.path.join(out, "start")
        check(size(os.path.join(s, "box.bmp")) == (1280, 800) and size(os.path.join(s, "second.bmp")) == (800, 600),
              "start: the first start draws at 1280x800, the second at 800x600")
        log = open(os.path.join(s, "stderr.txt"), errors="replace").read()
        check("recomp: options box opened from the start screen" in log, "start: the box opens from the start screen")
        check(tactics.result() == 0, "tactics: the run ends with code 0")
        log = open(os.path.join(out, "tactics", "stderr.txt"), errors="replace").read()
        check("recomp: options box opened from the side bar" in log, "tactics: the box opens on the tactics screen")
        code, made, text = readonly.result()
        log = open(os.path.join(out, "readonly", "stderr.txt"), errors="replace").read()
        check(code == 0 and made, "readonly: the run ends with code 0, the folder was read only")
        check("attribute_colours = on" in text.splitlines() and "attribute_colours = off" not in text,
              "readonly: settings.txt keeps attribute_colours = on")
        check("recomp: option attribute_colours = off not saved, kept on" in log,
              "readonly: the option keeps its old value in memory")
    print(f"pictures in {out}/<run>/*.png; work folder {work}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
