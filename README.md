<p align="center">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/logo-sm.png" alt="CM 01/02 Bosman Edition" width="320">
</p>

<h1 align="center">CM 01/02: Bosman Edition</h1>

<p align="center">
  Championship Manager 01/02, rebuilt to run natively on Mac, Windows and Linux.<br>
  <a href="https://maxevans60.github.io/cm0102-bosman-edition/"><b>Download</b></a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases">All releases</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/milestones">Roadmap</a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/issues/new/choose">Report a problem</a>
</p>

<p align="center">
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases"><img alt="Latest release" src="https://img.shields.io/github/v/release/maxevans60/cm0102-bosman-edition?include_prereleases&label=release"></a>
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/maxevans60/cm0102-bosman-edition/total"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20Steam%20Deck-blue">
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/commits/main"><img alt="Last commit" src="https://img.shields.io/github/last-commit/maxevans60/cm0102-bosman-edition"></a>
</p>

<p align="center">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/s1@2x.png" alt="The squad screen in widescreen" width="49%">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/s3@2x.png" alt="A match in widescreen" width="49%">
</p>

## What it is

The Bosman Edition is the 2002 game, version 3.9.68, translated instruction by instruction from the original Windows program into C and compiled for each computer. No emulator runs underneath. The match engine, the transfers and every decision of the game are the ones from 2002.

Each download is ready to play. It holds the game files of the free release that Eidos published in 2008, with the last official patch. It needs no CD and no Wine.

## Features

- Runs on macOS 11 or later (Apple silicon and Intel), Windows 10 and 11 (64-bit), and 64-bit Linux. The Linux AppImage also runs on the Steam Deck in desktop mode.
- Widescreen at 1280 × 800 in a window you can resize, with sharp scaling. The original 800 × 600 layout is one setting away.
- Any season from 2001/02 to 2025/26, with a database for every season in the download, and that season's leagues, cups and rules. In a long game the rules change each July as they did in real life.
- An Assistant menu on the squad screen picks the best team, the fittest team or an under-21 team, scored by the key attributes of each position.
- Clickable player and club names, coloured attributes, condition and morale.
- Holiday until the next match, faster days on holiday and a Fastest commentary speed.
- Save names with your club and the date, and the last three copies of each save in a backups folder.
- Keyboard shortcuts, and a window that opens where you left it.

Each feature is a line in `settings.txt` in the save folder and can be switched off. In the base game (2001/02, 3.9.68, changing rules off) no feature changes a match result. The [site](https://maxevans60.github.io/cm0102-bosman-edition/#questions) lists every setting with its default.

## Install

| | |
|---|---|
| Mac | Open the `.dmg` and drag the game to Applications. If macOS says it cannot check the app, open System Settings, then Privacy & Security, and click Open Anyway. |
| Windows | Run `Setup.exe`. If Windows shows "Windows protected your PC", click More info, then Run anyway. |
| Linux and Steam Deck | Download the `.AppImage`, allow it to run as a program in its Properties, and double-click it. On the Steam Deck, do this in Desktop Mode. The trackpad is the mouse. |

Saved games are in `~/Library/Application Support/cm0102` on a Mac, `%APPDATA%\cm0102` on Windows and `~/.local/share/cm0102` on Linux. A new version does not touch them.

### Start a game with the data updates and the benchmark tactics

From 0.6.0 the download holds the season databases, the Update Team's 2026 preview data and 800 benchmark tactics. You do not install any of them by hand.

1. Install the game as above and start it.
2. Click Start New Game. In Setup Game, click the season button at the top left. Choose Season opens.
3. Pick a database:
   - For a season from 2001/02 to 2025/26, click its tile. The first time you start a season, the game unpacks its database into `Databases` in the save folder.
   - For the 2026 preview data, click the Custom database tile. Its database is "2026 Preview Data Update" (if you have other folders in `Databases`, click Next folder until it shows). Use the arrows next to Season to set 2025/26.
4. Click the yellow Start button, choose your leagues and your club, and start the game as usual.
5. On the tactics screen, click Tactics, then Load. The list shows the 800 benchmark tactics next to your own. Pick one and click Ok. `tactics/Benchmark tactics.txt` in the game folder ranks them and names the creator of each.

The ten best tactics of the benchmark, by average points per season over 100 seasons. Each name links to its forum post.

| Rank | Tactic | Creator | Formation | Average points |
|---|---|---|---|---|
| 1 | [21421](https://champman0102.net/viewtopic.php?p=64298#p64298) | kvlz | 0-2-1-4-2-1 | 72.10 |
| 2 | [d_champman_v2](https://champman0102.net/viewtopic.php?p=103944#p103944) | Sky18akavenator | 0-2-1-4-2-1 | 71.47 |
| 3 | [Demolishc](https://champman0102.net/viewtopic.php?p=89905#p89905) | scemoka | 0-2-1-6-0-1 | 70.83 |
| 4 | [Demolishk](https://champman0102.net/viewtopic.php?p=66301#p66301) | scemoka | 0-2-0-5-2-1 | 70.49 |
| 5 | [262 AMM](https://champman0102.net/viewtopic.php?p=100032#p100032) | scemoka | 0-2-1-4-1-2 | 69.69 |
| 6 | [2-7-1 MMC](https://champman0102.net/viewtopic.php?f=30&p=61415#p61415) | Germaniac | 0-2-1-4-2-1 | 69.53 |
| 7 | [262 MMJ](https://champman0102.net/viewtopic.php?f=81&t=2174) | scemoka | 0-2-1-5-0-2 | 69.48 |
| 8 | [262 MMC](https://champman0102.net/viewtopic.php?p=61162#p61162) | scemoka | 0-2-1-4-1-2 | 69.28 |
| 9 | [Demolishx](https://champman0102.net/viewtopic.php?f=81&t=1834) | scemoka | 0-2-1-6-0-1 | 69.06 |
| 10 | [262 MMJ_4](https://champman0102.net/viewtopic.php?p=82292#p82292) | FELIGOL | 0-2-1-5-0-2 | 68.99 |

The Load list holds about 1,075 tactics. Past that, the dialog has no room for its Ok and Cancel buttons, so keep at most about 250 tactics of your own in the `tactics` folder of the save folder.

## Roadmap

0.6.0 (released TODO-DATE) added Choose Season: any season from 2001/02 to 2025/26 with its own squads, leagues, cups and rules, and rules that change each July in a long game.

Planned for 0.7.0 ([#45](https://github.com/maxevans60/cm0102-bosman-edition/issues/45)): easy online play with friends. The host gets a short join code and friends type it in, with no extra patch or VPN.

## How it works

The original program is about 9,400 functions of 32-bit x86 code. The recompiler in [`tools/recomp`](tools/recomp) translates each function into C. A small runtime in [`src/recomp_rt`](src/recomp_rt) replaces the Windows calls the game makes (windows, drawing, files, sound, input) with SDL2. Fixes and features hook the game through checked addresses: the build stops if a hooked address no longer matches the original. A smoke test plays the game from a new game to the first news, and further tests play a match and compare saved games byte for byte.

## Build from source

The repository holds no part of the original game. To build it, you need your own copy of the 2008 free release (`CM0102.iso`) and the official 3.9.68 patch (`cm0102_3968.exe`).

```bash
python3 -m pip install capstone
python3 tools/clean_files/make_clean_files.py CM0102.iso cm0102_3968.exe ../clean-3968
cmake -S . -B build -DCM0102_EXE=../clean-3968/cm0102.exe -DRECOMP_PYTHON=python3
cmake --build build -j 8 --target cm0102_recomp
```

The first command checks each file against [`tools/clean_files/manifest.tsv`](tools/clean_files/manifest.tsv) and stops on any difference. The tool needs `bsdtar` and `unshield`. The build needs CMake 3.20, a C compiler, Python 3 with `capstone`, and SDL2. [`tools/release/make_release.sh`](tools/release/make_release.sh) makes the Mac, Windows and Linux downloads.

## Report a problem or ask for a feature

Open an [issue](https://github.com/maxevans60/cm0102-bosman-edition/issues/new/choose). For a problem, say which computer you use, which version of the game, and what you clicked before it happened.

For a problem with the picture or the window, such as blurred text or a picture that stays small, add the display report. Press Ctrl+Shift+D in the game (Cmd+Shift+D on a Mac). The game copies the report to the clipboard, so you can paste it into the issue or a forum post. It also saves it as `display-report.txt` in the save folder. The report lists your system, your screens and their scale, and how the game scales its picture. It does not include your account name. Many features came from players on Reddit, and the release notes credit each one.

## Credits

- Sports Interactive made Championship Manager 01/02, and Eidos published it and made it free in 2008.
- u/gundenaf on Reddit asked for the Linux version, the database choice, the polygon analyser, the HD layout and the attribute groups.
- The CM 01/02 Update Team at [champman0102.co.uk](https://champman0102.co.uk) makes the yearly databases that the custom databases feature will load.
- nakre and the [champman0102.net](https://champman0102.net) community made and tested the benchmark tactics in the downloads ([cm0102-tactic-benchmark](https://gitlab.com/nakre/cm0102-tactic-benchmark)). Each tactic is the work of its creator; `tactics/Benchmark tactics.txt` in the game folder names them.

## Legal

The Bosman Edition is an unofficial fan project. It is not affiliated with or endorsed by Sports Interactive, SEGA, Eidos or Square Enix. Championship Manager is a trademark of Square Enix. The downloads include the game files from the 2008 free release.

From 0.6.0 the code in this repository is released under the [GNU General Public License v3.0](LICENSE). Releases up to 0.5.0 were released under CC0 1.0. The licence covers only the code written for this project. It does not cover the original game, its data, its logo, the Update Team databases or the benchmark tactics, which belong to their owners.

The function map (`docs/research/function-map/`) and the data map (`docs/research/data-map/`) are under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), as in their own repository, [cm0102-function-map](https://github.com/maxevans60/cm0102-function-map).
