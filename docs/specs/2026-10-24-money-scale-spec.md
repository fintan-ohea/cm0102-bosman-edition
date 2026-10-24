# Money scale (fork of fintan-ohea)

Status: built on `feature/save-editor`, 2026-10-24 (`src/recomp_rt/win/money.c`). Decisions taken (end of this page). Research: `docs/research/2026-10-24-money-scale.md`.

## Problem

The game's money is that of 2001: the best players are worth about £25M and the top wages are near £80K a week. Today the best players are worth £150M to £200M, and the money of clubs, wages, fees and prize money has grown about as much. The 2026 data updates change the players but not the money: in the 2026 preview save the most valuable player is worth £25.9M.

## Solution

An option `money_scale`: `off` (default), `5`, `7` or `10`. With a factor, every amount of money the game shows is that factor times larger, and every amount the player types or picks is divided by it. Values, wages, transfer fees, bids, budgets, balances, prize money, ticket income and the rest all change together, so the books stay right.

The game keeps its own amounts: only what the player sees and enters changes. Five functions of the game's logic compare or step amounts as they are shown; they run with the game's own rates. So the AI's decisions, a club's finances and every random draw are those of the original (same game, Q152), and a save has no trace of the option: it can be loaded with any factor.

The way: the original converts every amount it shows into the player's currency (pounds, euros, dollars and more) with one exchange rate, and every amount the player enters back with the same rate. The option multiplies that rate by the factor, after the game sets it.

## Limits

- Everything scales by one factor. In real football wages and TV money grew more than ticket prices; the game shows them all with the factor.
- The game's preset amounts (the seven amounts of 100,000 to 10,000,000 that some menus offer) show the factor too, so a preset is still the same amount for the game.

## Tests

- `ctest -R options`: the default, the values, a bad value.
- Same game: `smoke_run.py --same-save` with `money_scale = 7` and `off`: the save is that of normal (only the display changes).
- `money_scale_run.py`: with 7 the game's rates are 7 times its own, and the Contract and Transfer tabs of a profile and the player editor show other amounts than with `off` in their rows of money and nowhere else (£9,000 a week becomes £65,000, a value of £3,900,000 becomes £27,000,000).
- Five functions of the game's logic compare or step amounts as shown (research note). They run with the game's own rates, so the decisions stay those of the original. A 45-day holiday from start.sav with 7 and with `off` drew the same random numbers on every day (24.8 million on the last), and its saves differ only in 253 bytes that are all heap addresses the game writes as they are (the longer money texts of the news move the heap); the 14-day same-save runs are equal byte for byte. `RECOMP_MONEY_LOG` (test only) writes each call of them.

## Decisions

fintan-ohea, 2026-10-24.

1. A money scale of the display and the input, not a change of the stored amounts.
2. The factor is 7.
