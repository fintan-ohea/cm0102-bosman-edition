# Club editor (fork of fintan-ohea)

Status: built on `feature/save-editor`, 2026-10-25 (`src/recomp_rt/win/club_editor.c`). Research: `docs/research/2026-10-25-club-editor.md`.

## Problem

The player editor (docs/specs/2026-10-24-player-editor-spec.md) holds a club's balance and transfer funds, so a club can be changed only from the profile of one of its players, and only its money.

## Solution

An option `club_editor`, off by default. With it on, the club screen has a sixth tab, Edit Club, after Finances & Info. The tab shows the club's values with buttons that change them at once:

- Money: Balance (1m and 10m steps), Transfer funds (1m and 10m, as the adjustment the player editor kept).
- Club: Reputation (100 and 1,000, 1 to 10,000), Training facilities (1 and 5, 1 to 20), Status (Professional, Semi-Professional, Amateur), Board confidence (100 and 1,000, 1 to 10,000).
- Stadium: Capacity, Seated capacity and Expansion limit (500 and 5,000; seats never above capacity), Under-soil heating, Retractable roof, and whether the club owns its ground.

Money steps are amounts as shown, so they follow the money scale. A change marks the game as edited, as the player editor does. National teams get no tab. The Balance and Funds row leaves the player editor.

## What each value does in the game

- Balance and funds: as in the player editor (the funds are an adjustment added to Finance::get_transfer_budget 0x598800).
- Reputation: the transfer and board code reads it, and the season-end stadium routine.
- Training facilities: the club's training ground; the info tab shows "Top facilities plus youth academy" above 15.
- Board confidence: the board's confidence in the club's manager; low values end in the sack.
- Stadium: capacity limits the gate; the season-end routine uses the expansion limit, heating and roof.

## Tests

- `ctest -R options`.
- Same game: `smoke_run.py --same-save` with `club_editor` on and off. Off equals normal. On, the tab bar's heap block holds 11 entries, so later heap addresses move: in a 14-day run the save differed from normal only in general.dat and staff history.tmp, in 202 bytes that are all heap addresses 0x930 higher, so the check allows those two parts for `club-on`.
- `club_editor_run.py`: the tab draws for Arsenal with six tabs on top; each button changes its value by its step and keeps it in range; a save and load keep the values; with the option off the club screen has five tabs; the player editor has no club row.
