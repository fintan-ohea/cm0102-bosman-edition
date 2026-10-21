# Expected files of the rules report on main (batch F)

Date 2026-10-21. All 66 files of `tests/seasons/rules_report_expected/` were run again with `--update-expected` on the build of origin/main 8d5466e (`ref-builds/main-batchF-dir`, batch G only changes the club_records.cpp 794 box path and adds the display report). 55 files changed (510 lines added, 1444 removed), 11 did not change.

## How

- One run for each file with the arguments that its name gives: the year, the nations (`all` and `-nation` for "all without"), the extras (`conference`, `regional`, `c2`, `spain`, `portugal`), `--start-only` for a `-start` file, `--changing-rules on` for a `changing-` file. The `--last` of a season file is the last season in the old file (a season file of 2 seasons has `--last` year + 1; 2001-england 2027).
- The database of each year is the one of `2026-10-16-season-database-table.md`: 2002 the 3.9.68 data (no `--database`), 2003 October 2003, 2004 and 2006 the own databases (Summer 2004, Summer 2006, unpacked from `own-dbs/*.zip`), 2005 September 2005, 2007 November 2007, and so on to December 2025.
- Up to 4 games at a time under the lead's limits (HOLD, 16 games, load 24). No run showed a message box.
- Two runs failed on 120 s setup waits at load above 20 (2008-england-germany: `wait screen:0x821900`; the 14-nation 2010 file: `wait typing`); both were run again alone.

## What changed, and why

The tool compares the report with the rules table. A line leaves or enters when the game or the table changed. Each group below was read: the lines that leave or enter come from a rule of one slice, with its doc.

| Group | Lines | Cause |
|---|---|---|
| `name` lines gone, and the competition name in other lines (the text in brackets) | about 240 | the names of the season are applied to the game (slice 15 names table, slice 16 season change): the game's name is the table's name |
| New competitions `new_*`: clubs, bench, subs_used, name, idle, not_held lines | about 225 | competitions that the slices make (Suruga Bank, Gold Cup, Nations Leagues, Finalissima, Recopa, Sudamericana, Conference League, state championships): `2026-10-18-new-comps.md`, `2026-10-18-nations-league.md`, `2026-10-18-gold-cup-class.md`, `2026-10-18-uefa-2021-2023.md` |
| Brazilian regional groups and state championships (`bra_reg_*`, `new_sul`, `not_in_game bra`) | about 260 | slice 29: `2026-10-20-brazil-states.md` |
| Club World Cup, Intercontinental Cup, Inter-American Cup, Asian and South American club cups: clubs, bench, not_held, up | about 340 | slice 26: the cups wait idle when the table does not hold them, and the Club World Cup has its real formats: `2026-10-18-club-world-formats.md`, `2026-10-18-club-world-idle.md`, `2026-10-18-club-world-parts.md` |
| World Cup qualifying, Euro, Africa, Asia, Oceania, Gold Cup, Olympic Games, Confederations Cup: clubs, idle, not_held | about 120 | slices 27 and 28: `2026-10-18-international.md`, `2026-10-18-wc-europe-paths.md`, `2026-10-18-national-team-comps.md` |
| `playoffs` lines (France, Germany, Italy, the 2001 files) | about 80 | slice 19: the play-off kind (places before the other side of a tie, the new count form): `2026-10-18-playoffs-splits-calendar.md` |
| `uefa_cup` and `european_cup` format and clubs, `not_in_game uefa`, Super Cup bench | about 45 | slices 24 and 25: the Conference League and the league phase: `2026-10-18-uefa-2021-2023.md`, `2026-10-19-uefa-league-phase.md` |
| `club_list` lines gone (22 groups, none new) | about 360 | the club lists of the divisions are now exact: club records and the club data of slices 18 and 29: `2026-10-19-club-records.md`, `2026-10-17-club-moves-and-names.md` |
| `down eng_third` 2018 on | 12 | League Two relegates 2 from 2018/19 in the game now (slice 18, league sizes): `2026-10-18-league-sizes.md` |

## Lines with a second look

- `2001 idle fra_first (French CFA)` and `(French Lower Division)` (2001-all-conference-regional-c2.tsv, 2 lines gone): objects of that record hold no year at +0x40, and the report skips such objects since slice 18 (`rules_report_check.py`, "report skips objects with no year", commit 089eca7). The old file was made before that rule.
- `2022 down gre_prm` and `2022 up gre_second` (2 files, 4 lines gone): the Greek second tier of 2022 is two groups (slice 20: `2026-10-20-new-divisions.md`); the group rows and their texts changed ("Super League 2 North").

No line is left without a cause.
