---
name: add-option
description: Adds a new Bosman Edition option end to end, as a line in settings.txt, a row in the options box, tests and same-save runs. Use for "add an option", "new setting", "make it a switch", "settings.txt line", "off by default", or any feature the spec gives an option name.
---

# Add an option

Every feature that a player may not want is an option. The settings file (`settings.txt` in the save folder) is the store. The options box writes to the same file.

## Steps

1. `src/recomp_rt/win/options.h`: add `OPT_<NAME>` before `OPT_COUNT`, with a comment that gives the kind, the default and the file that reads it.
2. `src/recomp_rt/win/options.c`: add the row to the table: name, kind (`KIND_SWITCH`, `KIND_ENUM` or `KIND_TEXT`), default, the list of values for an enum, and the comment line that `options_load` writes into the file. The comment says in plain words what `on` does, and names any limit (for example "widescreen only").
3. Read the value through the getter (`options_bool`, `options_enum`, `options_string`) where the feature acts. Read it at each use when it can apply at once. A value read once at start needs the note "Applies at the next start" on its row in the box.
4. `tests/options/test_options.c`: check the default and that the file gets the line with its comment. Run `ctest -R options` in `build/`.
5. The options box (`src/recomp_rt/win/options_screen.c`): add the row to the Screen or the Game tab, nine rows each (add a tab or ask Max when both are full). Label in plain words. Add a note when the option takes effect only at the next build of a screen, the next game day or the next start.
6. `tests/smoke/smoke_run.py`: add the switch to the `off` run of `SAME_SAVE_RUNS`, and add `<name>-on` and `<name>-off` to `SAME_SAVE_RUNS` and `MATCH_RUNS`. If the feature acts on a screen that the holiday steps never open, add a check that opens that screen in both runs. Use the same-save skill to read the results.
7. Site and changelog, in the release: one line in the site FAQ list with the default, and the option name in backticks in the changelog item.

## Rules

- A switch takes `on` or `off`. Use the default the spec gives.
- The option off must give the original's behaviour exactly.
- No option may show a hidden value (current or potential ability, or any value the original does not show). The fork's exceptions are `player_editor` and `hidden_tab` (docs/specs/2026-10-30-hidden-tab-spec.md); ask before adding another.
- `options_set` writes the file through a temporary file and a rename and keeps the player's lines and comments. Never write `settings.txt` another way.
