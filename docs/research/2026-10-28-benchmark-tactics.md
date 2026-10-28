# Benchmark tactics in the downloads (2026-10-28)

The downloads carry the 800 best tactics of nakre's tactics benchmark in `tactics/` of the game folder. A player
picks one on the tactics screen: Tactics, Load. No game code changes.

## The source

- https://gitlab.com/nakre/cm0102-tactic-benchmark, commit 81b46a5 (2026-10-25). "tested tactics" holds one folder
  per tactic: the `.tct` file, a zip of it, `benchresult.txt` and an Excel sheet of the results. 3,551 `.tct` files
  in all, by many creators of the champman0102.net community. Each played 100 seasons with the same Greek league
  team against the AI tactics of 3.9.68.
- `CM Tactics.xlsx` ranks them: Sheet2 has one row per tactic (name, creator, formation, average points), Sheet1
  the forum link.
- Licence: the repository has none, and its README says nothing on reuse. The user decided to ship them with
  credit. `tactics/Benchmark tactics.txt` names the benchmark, links the repository and lists the rank, name,
  creator, formation, average points and forum link of each tactic.
- The repository is over 300 MB; a full clone times out. A blobless sparse checkout of `CM Tactics.xlsx` and
  `tested tactics/*/*.tct` (about 30 MB) is enough for the tool.

## How the game finds tactics (cm0102.exe 3.9.68)

- The "Set To Default" list is the 17 presets, `.pct` files of `CM3_TACTICS` in a fixed order
  (2026-10-16-tactics-inventory.md). New files cannot join it.
- The player's tactics are `.tct` files. "Load" of the tactics screen (`tactics_screen_handle_event` 0x89f710)
  calls `open_file_dialog` 0x58f500 with the title "Load Formation" (0xa8c3a0), the extension ".tct" (0xa8c3b0,
  pushed at 0x8a04cd), the folder of `HUMAN_MANAGER::get_folder_path` 0x5fc5b0 (kind 0) and the check
  `formation_check_file_password` 0x5b87e0. "Load Preset" (0x8a053f) takes `.pct` from `CM3_TACTICS` and shows only
  when byte [0xb7434a] is set.
- The manager's tactics folder is `<current folder>\tactics`: `utils_get_user_folder` 0x90bea0 calls `_getcwd`
  and `_makepath(out, 0, cwd, "tactics", 0)` and creates the folder. The current folder is the game folder.
- The runtime's file search lists the save folder and the game folder together (`FindFirstFileA`,
  src/recomp_rt/win/files.c). So `.tct` files in `tactics/` of the game folder show in the Load dialog next to the
  player's own, which the game saves in the save folder.

## The limit of the Load dialog

- `file_dialog_build` 0x58f7c0 adds one list item per file it finds (0x8040a0 at 0x58fc07, in the loop of
  `file_find_first` 0x4c0aa0). `Display::add_item` 0x55f990 refuses an item when the screen holds 0x4af (1,199):
  it sets a flag and returns -1, so nothing overflows.
- Past about 1,075 `.tct` files the Directory text and the Cancel and Ok buttons find no room, and no tactic can
  be loaded. Measured in the dev build: 1,000 plus 60 of the player's own works; 1,000 plus 90 does not. The
  dialog lists files only, not folders, so subfolders do not help a player.
- The dialog opens every file to check its password each time it is built. With 800 files it was up and
  drawn 2.5 s after the click on Load in the test run.
- The list is in the order of the folder, as the host gives it. It is not sorted.

## What ships

- `tools/release/make_benchmark_tactics.py <checkout> <out>` takes the 800 with the best average points (ties
  on average goal difference, then name), keeps only files whose version is in the loader's range 0x98ec54 to
  0x98ec5e (`Formation::load_from_disk` 0x5b5d10), writes names in ASCII (none of the 800 needs it) and writes the
  credits file. Average points 72.10 to 60.02.
- `make_release.sh --tactics <out>` copies `<out>/tactics/*` to `tactics/` of the game folder and lists each file
  in the download manifest. The clean files have no `tactics` folder.
- 800 leaves room for about 260 tactics of the player's own (800 plus 250 tested). 2,751 tactics of the benchmark
  are not shipped.

## Test

Dev build `~/Applications/cm0102-dev/cm0102_recomp`, a copy of the clean files with the tool's `tactics` folder
as the game folder, an empty save folder, the steps of tests/smoke/smoke_run.py to the first news, then Arsenal
Squad, Tactics, Tactics menu, Load:
- The dialog finds 800 files (RECOMP_TRACE: `FindFirstFileA(...\tactics\*.tct)`: 800 matches) and shows Ok and
  Cancel.
- "3.2.4.1 ANGEPOSTECOGLOU_" and Ok: the tactics screen shows its name and its 3-2-2-2-1 shape; after Ok and
  a new visit to the tactics screen it is still Arsenal's tactic.
- With 250 more `.tct` files in the save folder's `tactics`, the dialog still shows Ok and Cancel.
