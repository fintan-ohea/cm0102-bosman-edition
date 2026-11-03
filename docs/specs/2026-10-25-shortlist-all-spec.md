# Shortlist All (fork of fintan-ohea)

Status: built on `feature/save-editor`, 2026-10-25 (`src/recomp_rt/win/shortlist_all.c`). Research: `docs/research/2026-10-25-shortlist-all.md`.

## Problem

A player goes on the shortlist one at a time: open his profile, open the Action drop-down, click Add To Shortlist. After a search that finds the players a manager wants to follow, that is one profile per player.

## Solution

An option `shortlist_all`, off by default. With it on, Player & Staff Search has a Shortlist All button left of Filters, on every list but the Short List itself. A click adds every person the list shows, after its filters (all pages, not only the page on screen), to the manager's shortlist.

- A person goes on the shortlist only when the profile would offer Add To Shortlist for him: a player of the loaded leagues, not at a club the manager runs, not a board member and not retired. Players already on the shortlist are left out. So on Staff Search only the staff who are players go on it.
- After the click the button reads "Added n" (or "None to add") until the next click on the screen.
- The batch is one change of the search session: Cancel asks "Cancel all changes?" and takes it back, Ok keeps it.

## Tests

- `ctest -R options`.
- Same game: `smoke_run.py --same-save --save start.sav` with `shortlist-on` and `shortlist-off` (the holiday never opens the screen), and `--search` with `shortlist-on` and `shortlist-off`, which open the screen with the button and close it with Cancel. Both must give the save of the run without the option.
- `shortlist_all_run.py`: the first click adds every player shown, a second adds nobody, Staff Search adds only its players, Ok keeps the shortlist over a new opening of the screen; with the option off the screen has no button.
