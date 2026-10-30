# Hidden tab on the player profile (fork of fintan-ohea)

Status: built on `feature/save-editor`, 2026-10-30 (`src/recomp_rt/win/hidden.c`).

## Problem

The game keeps values that no screen shows: current and potential ability, and attributes such as Consistency,
Important Matches, Injury Proneness, Ambition and Professionalism. A player who wants to see them needs an outside
tool.

## Solution

An option, `hidden_tab`, off by default. On: a player's profile has a sixth tab, Hidden, after History. It shows,
in three columns:
- Mental: Adaptability, Ambition, Loyalty, Pressure, Professionalism, Sportsmanship, Temperament.
- Playing: Consistency, Dirtiness, Important Matches, Injury Proneness, Natural Fitness, Versatility; under it,
  Ability: Current Ability and Potential Ability (1 to 200).
- Set pieces and feet: Corners, Penalties, Throw Ins, One On Ones, Left Foot, Right Foot.

Corners, Penalties, Throw Ins and One On Ones are rated with the current ability as the game rates its shown
technical attributes. With six tabs the game's tab bar draws two rows: Transfer, History and Hidden move below the
page. The tab only shows values; it changes nothing. Staff who are not players get no tab.

## Search by ability (2026-10-30)

With the same option, the Attributes page of the search Filters (Player Search, not Staff Search) has one more row
in the free place after Technique: Ability and Potential, each with - and +. They set the least current and least
potential ability a player must have, in steps of 10 up to 200; "-" means no minimum. Clear on that page clears
them. A saved search keeps them. The option is one switch for hidden values; a fourth tab for a separate option
would move the tabs of the Options box that the tests click.

Research: the "Ability" part of the comment in `src/recomp_rt/win/value_range.c` and
`docs/research/2026-10-30-value-range.md`.

## Decision

fintan-ohea, 2026-10-30: hidden values may show on this tab, behind the option. This changes the rule of the
glossary (Hidden value) and decision 1 of the player editor spec, which kept them to the editor box. With the option
on, the fork shows hidden values, so this feature is not one to offer upstream.

Research: `docs/research/2026-10-30-hidden-attributes.md`.

## Tests

- `tests/smoke/hidden_tab_run.py`: the tab opens on a player's profile and shows his values; the other tabs still
  work after it; with the option off the profile has five tabs.
- `tests/smoke/ability_filter_run.py`: Player Search lists fewer players with an ability minimum, fewer again with
  a potential minimum too, and all of them again with both at "-"; with the option off, all of them each time.
  Clear is not checked yet.
- Same game: the same-save runs `hidden-on` and `hidden-off` (no profile opened) equal normal.
