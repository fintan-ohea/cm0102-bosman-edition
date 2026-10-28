# A database folder from the 2026 Data Update Preview Save (2026-10-26)

The Update Team's "2026 Data Update Preview Save" (champman0102.co.uk, the save of the next database release)
becomes a custom database for a new game: `Databases/2026 Preview Data Update (merged)` in the user folder,
68 files, the file set of May 2026.

Build, in one command (the save must be unzipped first; read downloaded files with `python3 -I`):

    python3 -I tools/save/preview_db/build_preview_db.py "<save>.sav" "<user folder>/Databases/May 2026 Data Update" "<new folder>"

It runs three steps with the files in between in a temporary folder:
1. `save_to_db.py`: the tables of the save as a database folder (sections "What the save is" to "What the
   converter changes"). This folder alone loads, but game creation is slow (section "Test of step 1").
2. `match.py`: matches people and clubs of May 2026 and that folder.
3. `merge.py`: undoes game creation with May 2026 as the reference (section "Step 3").

## What the save is

- Container version 3 (stored, not compressed), 251 parts. Game date day 166 of 2001 (16 June 2001, general.dat):
  the first day of a new game, saved after game creation and its run-up. The manager is "October 2026 Preview Save"
  at Limerick F.C.
- Its data is one data season later than May 2026: every date (births, contracts, competition history) is one year
  earlier, as the date-shift notes (docs/research/2026-10-17-season-date-shifts.md) describe for Update Team data.
  Example: the English Premier Division winner Liverpool is in the row of 2001 in May 2026 and of 2000 in the save.

## Why the tables convert

- `save_database` 0x52dad0 writes both a save and data files; only the open and write calls differ (zip
  0x9326e0/0x932e10 or files 0x54d5b0/0x945ce6). It turns pointers back into ids first (0x52db12..). So each table
  part of the save has the record layout of the data file.
- In file mode the writer gives each table that shares a file with the one before its own name: Preferences.dat
  (0x530af5), nonplayer.dat (0x530d63), player.dat (0x530fc8). May 2026 keeps those four in staff.dat with offsets
  (ids 6, 9, 10, 22). The loader reads name and offset from each index entry, so either layout loads. The new
  folder uses the May layout.
- Note for GLOSSARY users: id 9 (68-byte rows) is the non-player table and id 10 (70-byte rows) the player table.
  The staff row links the player row at +0x61 and the non-player row at +0x69 (checked on the data: +0x61 is set
  for 106,985 staff = the player count). The 2026-10-13 database inventory has these two the other way round.
- index.dat of a save: two header words ([0xae2344] = 127 friendly comp rows, [0xae2348] = 16 name reserve rows),
  22 entries, a byte, 0x1cc stack bytes and the club table (0x531336..0x531518). The loader reads both header words
  (0x52873b, 0x528783) and subtracts the name reserve from the name counts (0x528a8d..). A data folder has 0, 0 and
  no tail.

## What the converter changes

- Name tables: drops the last 16 rows (reserve rows, nation -1; the loader adds its own, 0x52add4).
- Staff: drops the last 16 rows (the manager rows of a game, 1b in the game-creation inventory), with their 16
  non-player rows, 10 unused non-player rows at the end and their 16 preference rows; renumbers the links.
  Clears Limerick's manager slot (+0xcf), which held the save's manager (May 2026 has no manager there either).
- nation_comp.dat: keeps the 38 rows of the save's index; the part also holds the 127 friendly rows that
  [0xae2344] counts (written at 0x52f978, read back at 0x52bbd4).
- staff_history.dat: the save has no such part (the game frees the table at creation, 0x527ee0 called at
  0x7c0ccd). Rebuilt from the career lists: player stats.dat holds a 39-byte row per player after
  8 + count*32 bytes, with the first and last node offsets at +0x18/+0x1c; player stats history.tmp holds 47-byte
  nodes (43-byte entry + next offset), read by FILE_LLIST::get_nodes 0x58f120. The PlayerStats constructor
  0x7bfd50 turns each data row into two entries, category 6 (flags 0x30) and 2 (0x10), with year +0, club id +2,
  apps +6, goals +8 (0x7c09e3..0x7c0b64). The converter takes the category-6 entries in that exact pattern with
  year < 2001: 359,116 rows. 5,073 entries that the run-up archived (match stats, flag bits) are left out.
- staff_comp_history.dat: empty (count 0). The save does not hold it.
- Files that are not tables (.pct, .cfg, .RGN): copied from May 2026. The save does not hold them.

## Test of step 1

Scripted run (`tests/smoke/smoke_run.py` steps, own RECOMP_USER_DIR, `database = folder:2026 Preview Data Update`,
season 2001/02, changing rules off, England only): the data loads, game creation finishes, Arsenal's list position
gives AFC Wimbledon, the first news screen shows 14.7.01, and the game saves after 3 days (result 1, exit 0).

Game creation is slow: 369 s to the name screen, against 14 s for May 2026, 13 s for May 2026 played as 2001/02
(a copy with one byte added, so the hash check does not know it), and 9 s for 3.9.68. Without career rows it takes
75 s. Causes found:
- Stack samples (gdb, SIGSTOP every 25 s): nearly all time in qsort 0x945333 from PlayerStats::add_language
  0x7c6a30 with sort on, called from the career loop of 0x7bfd50 (0x7c098c). A gdb hit count: 61,567 calls of
  0x7c6a30 against 6,646 for May 2026.
- The trim (database_trim_staff_tables 0x524d70) keeps far more people: with England only, the new game has 57,400
  staff (42,895 players) and 5,212 clubs; May 2026 has 10,415 staff (6,909 players) and 2,600 clubs.
- The staff rows are in the state after player initialisation: +0x56..+0x5d are set for everyone (May 2026: mostly 0,
  unset), +0x18 holds the age, +0x52 is set, +0x5e mixes 0, 1, 2, 5, 6 (May 2026: 1). The threshold of a career
  language is (20 - staff+0x56) * 10 (0x7c09bf..0x7c09cd), so set values give many more languages. Which field makes the trim
  keep more people is not found (the trim rules are not read).

## Limits of step 1

- Careers of staff without a player row (managers' playing careers) and rows before 1900 are lost: the
  constructor skips them (0x7c07ae..0x7c080d, 0x7c0790). The preview's source data had 458,678 career rows
  (the count in the save's index); the folder has 359,116.
- The loan byte (+0x0e) of a career row is not kept by the game; it is 0 in every row of May 2026 too.
- Award history (staff_comp_history) is empty. The date-shift notes say the Update Team award rows do not resolve
  anyway (Q350).
- The tables carry the state after game creation and the run-up to 16 June 2001: player initialisation 0x535a80
  (attributes, reputations, contracts, values), generated non-playing staff 0x527240, moves and results of the
  leagues that play in the run-up. A new game runs creation again on top of that.
- euro.cfg, player_setup.cfg, weather.cfg and the tactics files are May 2026's; the preview may change them.
- The runtime knows Update Team folders by hash (`seasons_table.c`; May 2026 plays as 2025/26). The new folder is
  unknown, so it plays as a 2001/02 game with the Update Team dates (ages right, years labelled 2001), and later
  start seasons measure its date shift from the birth dates (`measure_folder` in `season_dates.c`).
- Game creation is slow and the game is larger for the same leagues (section "Test in the dev build").

## Step 3: undo game creation (the folder to play)

`merge.py` (with `match.py` and `dbread.py`) changes the folder of step 1,
with May 2026 as the reference for what game creation changed. Evidence is the data
(matched people), not code: a byte-by-byte comparison splits the player and non-player bytes into bytes that creation keeps
when set and bytes it rescales.
- Match: 112,821 people by first/second/common name and birth date one year earlier; 39,753 preview people are
  new, ~31,600 May people are gone. Clubs: 9,565 matched by id+name or name+nation; 1,182 new.
- Matched people: rescaled bytes (player 0x1e 0x23 0x24 0x26 0x27 0x2a 0x2b 0x31-0x33 0x35 0x37-0x3a 0x3e 0x41
  0x43 0x45; non-player 0x10 0x11 0x17-0x19 0x1b 0x1e 0x21), reputations (player +9/+0xb/+0xd, non-player
  +4..+0xd), personal attributes +0x56..+0x5d, +0x52, PA codes -1/-2, CA below 55, creation's national squads,
  empty contract ends and join dates: May's value (dates one year earlier). Kept bytes that May leaves at 0: 0.
  Everything else (clubs, jobs, contracts, nations, CA/PA, positions, kept attributes) comes from the preview.
- New people: rescaled bytes and personal attributes 0 (the game makes them, as for 70-85 % of May's people),
  reputations divided by 50 (creation multiplies +0xd by exactly 50, +0xb about 50).
- Staff +0x18 = year of birth, +0x5e = 1, as in May. Clubs: May's -2 markers (18,095) and 0 in +0x6d/+0x73/
  +0x77/+0x7b come back; new clubs get 0 there. Nations: May's bytes 0x86..0xd5 (ids -2, ranking doubles).
- Careers: the preview's 359,116 player rows + 532 May rows of people without a player row. Award history:
  May's 2,253 rows, year one earlier, names/staff/clubs remapped (unmatched -> -1).

Test (same script, England only): name screen after 11 s (May 2026: 14 s, step 1 folder: 369 s); trim keeps 10,600
staff, 6,890 players, 2,750 clubs (May: 10,415 / 6,909 / 2,600); first news 14.7.01, 3-day save works.
Spot checks: Salah Trabzonspor CA 160 (May: Liverpool 170), Haaland and Mbappé as in May, everyone a year older.

Limits of the merged build: new people's reputations are approximate (/50) and their creation-made kept bytes
stay as the save has them; a May value that the Update Team changed in a rescaled byte cannot be seen and stays
May's; people renamed or with a changed birth date count as gone + new; awards of 2025/26 are missing.

## Shipped in the downloads (2026-10-28)

The downloads carry the folder of step 3 as a custom database (Q417 leaves one database per season on Choose
Season, so it is not a version of 2025/26):
- `tools/release/make_custom_db_zip.py <folder> <out> --name "2026 Preview Data Update"` writes
  `<out>/2026 Preview Data Update.zip`: one folder with the 20 data files, the same zip for the same input,
  read back and compared with the folder.
- `make_release.sh --custom-dbs <out>` copies the zips to `Seasons/Custom/` of the game folder and adds them to the
  download manifest.
- The game: `seasons_db_unpack_bundled_in` (src/recomp_rt/win/seasons_db.c), called by the Choose Season scan,
  unpacks each zip once into `Databases/<name>` of the save folder, through `unpack-tmp` and a rename as a season
  unpack. `bundled-databases.txt` in the save folder names the ones done: a folder that the player removes or
  renames does not come back, and an existing folder of that name is never replaced. A zip that lacks a data
  file is not unpacked and is tried again at the next scan. The folder is unknown to the signatures table, so
  the screen lists it on the Custom tile, where the player names its season.
- Test: `tests/seasons/test_seasons_db.c` (test_bundled).
