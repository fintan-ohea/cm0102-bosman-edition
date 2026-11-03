# Value range in the search Filters (fork of fintan-ohea)

Status: built on `feature/save-editor`, 2026-10-30 (`src/recomp_rt/win/value_range.c`).

## Problem

The Value row of the search Filters screen has Any and Range. Range gives one band: "0 - £X" for seven amounts,
"Over £X", or the players the club can afford. A player cannot search for values from one amount to another, for
example from £1M to £5M.

## Solution

An option, `value_range`, off by default. On: the Value row on Range gets two more drop-downs, From and To, after
the original's drop-down. From lists 0, £100K, £250K, £500K, £1M, £2.5M, £5M, £10M, £25M, £50M, £100M and £200M;
To lists the same amounts from £100K, and No limit. The row's text shows the range, as "£1M - £5M" or "£10M +".
A search then lists the players whose value is at least From and at most To. The amounts are the shown amounts,
so with a money scale they are today's money.

- Picking From above To sets To to No limit; picking To below From sets From to 0.
- The original's drop-down still works: picking one of its bands replaces the range.
- The range is kept in the band byte of the filter block, at values the original never writes (8 and up), so a
  saved search keeps it and the game with the option off makes no value test for it.

Research: `docs/research/2026-10-30-value-range.md`.

## Tests

- `tests/smoke/value_range_run.py`: the list of Player Search with no range, with £1M to £5M, and with £5M and up;
  the two ranges list fewer players than no range and the sum fits.
- Same game: the same-save runs `range-on` and `range-off` (the Filters never opened) equal normal.
