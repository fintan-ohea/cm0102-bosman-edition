# Player editor (fork of fintan-ohea)

Status: Slices 1 to 5 built on `feature/save-editor`, 2026-10-24 (`src/recomp_rt/win/editor.c`). Decisions taken (end of this page).

## Problem

Players who want to change a saved game use the GK Save Game Editor (Graeme Kelly, v4.0, with the v4.1 fix of Nick+Co). It is a separate Windows program from about 2009. It needs Wine on Linux and a save written with Save Compressed off. It also needs the path of `cm0102.exe`, which the Bosman Edition keeps inside the download. The player closes the game, edits, saves in the editor and loads the game again.

## Solution

An editor inside the game. A player opens the profile of any player, clicks Edit, changes values in a box and clicks Save. The change is in the running game at once and goes into the saved game at the next save, like any other change of the game. No separate program, no Wine and no uncompressed save.

The editor is an option, `player_editor`, off by default. With it off the game is the same game as without this fork (the same-save check passes). The option exists because an edit changes the game on purpose, which the project's same game rule does not allow for a feature that is always on.

## What it edits first

In order. Each step is a slice of its own, and each ends with a working, tested game.

1. **The 31 attributes of a player** (Finishing, Pace, Determination and the rest), each 1 to 20, as the profile shows them. This is what the GK editor is used for most, and it touches one record.
2. **Current ability and potential ability** of a player. They are hidden values (glossary): no screen of the original shows them. The box shows and edits them (decision 1).
3. **The player's condition, morale and injury**: fit a player again, or end an injury. Built: Condition - and + (5% of the stored condition) and Full (100%), Morale - and + (1 to 20, shown with the game's word), and Cure, which ends the injury and its recovery today as if their days had passed. Research: the condition, injury and morale section of `docs/research/2026-10-24-player-editor-attributes.md`.
4. **The contract**: wage, end date and clauses (`docs/research/2026-10-15-contract-clauses.md` is the starting point). Built: the wage (- and + by 100, -10% and +10%) and the end date (a year earlier or later, never before today; not for a rolling, expired or free contract). Clauses are not in the box yet. Research: the contract section of `docs/research/2026-10-24-player-editor-attributes.md`.
   - **Contract type** (2026-10-30): a row under the attributes shows the type as the profile's Contract tab names it ("Full Time Monthly Contract", "Part Time Contract" and so on). For a monthly or fixed term contract, one button switches Monthly and Fixed term, the other Part time and Full time. A monthly contract that becomes fixed term ends on 30 June of the next season (the year after next from July, else next year); the end date buttons then move it. Trial and loan contracts keep their type. Cancel puts the type back. Research: the contract section of the research note.
   - **Release player** (2026-10-30): a button between Save and Cancel, for a player with a contract at a club who is not on loan. The first click turns it into "Click again to release"; the second ends the contract and takes the player out of the club today, as a free transfer with no payout, and closes the box, since a release cannot be undone. It runs the game's own steps for one player of the free transfer queue, without the board confidence and dislike penalties. Research: the release section of the research note.
5. **The club's money**: balance and transfer budget (`cash.cpp` in the data map, 52 globals). Built: the bank balance and the transfer funds of the player's club, a row of the player editor box (-1m and +1m each), because a side bar item that only the option makes would move memory on every screen. The funds have no field: Finance::get_transfer_budget (0x598800) works them out each time, so the editor keeps an adjustment per club that a wrap of 0x598800 adds to the result, and the save part keeps the adjustments (research note). The box shows both amounts as the game writes money. On 2026-10-25 the row moved to the Edit Club tab of the club screen (docs/specs/2026-10-25-club-editor-spec.md); the adjustments stay in this part.

Not in this plan (apart from Release player, slice 4): moving a player to another club, new players, new clubs, staff who are not players. A move touches the squads of two clubs, the contract, the history and the transfer lists, and a wrong one corrupts the save. The GK editor does it with its validation pass; it is a later plan if wanted.

## How it fits in the game

- **Where**: the item "Edit player" at the end of the Action drop-down of a player's profile, after "Add/Edit Manager Note" (the item create at 0x874653, made for every profile). It shows only when `player_editor = on` and the person is a player. The plan said "not during a match"; slice 1 has no such test, because the only match pointer the runtime knows (0xb67a64, `columns.c`) has no static evidence. An edit during a match changes the values the match reads from then on.
- **The box**: a screen of the runtime on the game's screen stack, built the way the Options screen is built (`src/recomp_rt/win/options_screen.c`): the frame of the Game Settings box (0x823950), pushed with flag 1 so the game draws it over the profile. One row per value: the name in font 3, the value as a yellow framed label, and green `-` and `+` buttons. Save and Cancel at the bottom. Text entry for typed numbers is a later improvement.
- **When it applies**: Save writes the values into the game's memory. The profile is built again when the box closes, so it shows the new values. Nothing is written to the save file until the player saves the game.
- **The option**: one line in the settings file, made with the `add-option` skill of this repository. The Graphics and Game tabs of the Options screen hold nine rows each, so the row "Player editor" is on a third tab, Tools. The tests that click the Game tab click its new place (x 444).
- **Same game**: with the option on, a profile gets one more widget, which moves memory as any new screen does. The game from that point can differ from the same game with the option off, even with no edit. The same-save runs `editor-on` and `editor-off` never open a profile, so they show that the option alone changes nothing.

## The data (3.9.68)

Facts read from the code of the original. Addresses are those of the original program.

- The staff table is at `[0xae23bc]`, 0x6e bytes a row, with the count at `[0xae2364]` (`injury_init.c`, `staff_reserve.c`).
- Staff `+0x61` holds the player data, 0 for staff who are not players (`picker.c`, `compare.c`).
- Player data `+0x05` is a s16, the current ability, 1 to 200 (`picker.c`).
- `0x5431e0` (staff_get_player_attribute, Database.cpp) checks the staff and an attribute index 1 to 31, then calls `0x543540` with (staff, player data, index). Read at 0x5431e0..0x54327f.
- `0x543540` (staff_compute_player_attribute) jumps through a table at `0x543a94` on index - 1 (0x5435b7). Some attributes are a stored byte that it returns as it is: the cases at 0x5435be, 0x54365b, 0x543665, 0x5436c7 and 0x5436d1 read `+0x1b`, `+0x1c`, `+0x1d`, `+0x1f` and `+0x20`. Others combine a signed stored byte with the current ability: the case at 0x5435c8 takes byte `+0x24` and the current ability `+0x05`, computes `x = (2 * byte + ability) * [0x967888] + [0x9688b0]`, then `(x * x * [0x967880] + x) * [0x968a80] + [0x968968]`, keeps it between `[0x967890]` and `[0x968ea8]` and rounds it (`0x945640`). The case at 0x54366f does the same with byte `+0x1e`. The case at 0x5436db tests `+0x0f == 0x14` first (inference: a goalkeeper check) and uses byte `+0x43`.

So the editor cannot always write the number the player types. For a stored attribute it writes the byte. For a computed one it must find the stored byte that gives the typed value at the player's current ability. The safe way is to try each byte value and ask the game's own `0x543540` for the result, not to copy the formula. Some values may not be reachable at a low current ability; the box then shows the nearest value it could set. A change of current ability changes every computed attribute, so the box applies current ability first and the attributes after it.

Slice 1 starts by reading the whole jump table at `0x543a94` and writing the 31 rows (index, stored or computed, byte offset) into a research note, with the address of each case, as `CLAUDE.md` asks. Potential ability's place in the player data is not read yet.

## Tests

- **Same game, option off**: `tests/smoke/smoke_run.py --same-save` with `player_editor = off` gives the same saves as `main`. A new run `editor-on` (option on, box never opened) must also give the same save.
- **Pictures**: the smoke run in widescreen and classic against our own reference pictures (`~/Documents/cm0102-smoke-ref`, recorded from `main` 144ed82c). With the option off nothing may change.
- **An edit works**: a scripted run opens a profile, sets an attribute, saves the game, loads the save in a new copy of the program and reads the value back through `0x5431e0`.
- **Code test**: the search for a stored byte finds the typed value, or the nearest one, for a set of current abilities, and never writes a byte out of its range.

## Building it on this machine

The fork builds with capstone 5 from `~/Documents/cm0102-venv` (`-DRECOMP_PYTHON`) and with Homebrew left out of `PATH`. Ubuntu's `python3-capstone` 4.0.2 gives a program that stops at start in the font library.

## Decisions

fintan-ohea, 2026-10-24.

1. **Hidden values.** The editor box shows and edits current ability and potential ability. This was the one place in the fork that showed hidden values, and only with `player_editor = on`. On 2026-10-30 the Hidden tab of the profile (`hidden_tab`, docs/specs/2026-10-30-hidden-tab-spec.md) joined it.
2. **Whose players.** Any player in the game, at any club or none.
3. **A mark on edited saves.** Yes. Once any edit is saved, the game is marked as edited and its save names end in " (edited)". Where the mark lives (a season part of the save or a field the save already has) is research for slice 1; it must survive a save and load.
4. **Upstream.** The editor stays in the fork for now. It keeps the rules an offer would need (off by default, same-save check passes), so a pull request stays possible later.
