# Player editor: where each attribute lives

Research for slice 1 of `docs/specs/2026-10-24-player-editor-spec.md`. Original 3.9.68, addresses of the original program. Read from the disassembly in the generated code (`build/recomp/gen`) and the bytes of the clean `cm0102.exe`.

## The two functions

- `0x5431e0` (staff_get_player_attribute) takes (staff, index 1..31). It calls `0x543540` with (staff, staff `+0x61`, index) (0x5431ff..0x543205). The callers that show a value cap it at 20 (`picker_shown_attribute`).
- `0x543540` (staff_compute_player_attribute) takes (staff, player data, index). For an index outside 1..31 or a null player data it shows an error and returns -1 (0x543a26). It jumps through the table at `0x543a94` on index - 1 (0x5435b7). On entry `edx` is the staff and `eax` the player data (0x543540, 0x54356b).

## The formula

A computed attribute takes a signed stored byte `b` and the current ability `ca` (player data `+0x05`, s16). Two forms (0x5435c8, helper 0x544380; goalkeeper form 0x5436db, helper 0x5442f0):

- normal: `x = (2 * b + ca) * [0x967888] + [0x9688b0]`
- weighted: `x = (2 * b + ca * [0x967880]) * [0x967888] + [0x9688b0]`

then `y = (x * x * [0x967880] + x) * [0x968a80] + [0x968968]`, held between `[0x967890]` and `[0x968ea8]` and rounded by `0x945640`.

The weighted form applies when the player data byte `+0x0f` is 20 for some attributes and below 15 for others (0x5436db, 0x5437ca). Inference: `+0x0f` is the goalkeeper position rating, so a keeper's outfield skills and an outfield player's keeper skills scale differently. The editor never copies the formula: it asks `0x543540` for the result of each candidate byte.

## The 31 attributes

"Stored" means the byte is the value. "Computed" means the byte goes through the formula. All offsets are in the player data (staff `+0x61`) except Determination.

| Index | Name | Case | Kind | Byte |
|---|---|---|---|---|
| 1 | Acceleration | 0x5435be | stored | +0x1b |
| 2 | Aggression | 0x54365b | stored | +0x1c |
| 3 | Agility | 0x543665 | stored | +0x1d |
| 4 | Anticipation | 0x54366f | computed | +0x1e |
| 5 | Balance | 0x5436c7 | stored | +0x1f |
| 6 | Bravery | 0x5436d1 | stored | +0x20 |
| 7 | Creativity | 0x5436db | computed, weighted when `+0x0f` = 20 | +0x43 |
| 8 | Crossing | 0x543722 | computed, weighted when `+0x0f` = 20 | +0x23 |
| 9 | Decisions | 0x5435c8 | computed | +0x24 |
| 10 | Determination | 0x54376c | stored, in the **staff** record | staff +0x58 |
| 11 | Dribbling | 0x543776 | computed, weighted when `+0x0f` = 20 | +0x26 |
| 12 | Finishing | 0x54396c | computed (0x5442f0 when `+0x0f` = 20, else 0x544380) | +0x27 |
| 13 | Flair | 0x5437c0 | stored | +0x28 |
| 14 | Handling | 0x5437ca | computed, weighted when `+0x0f` < 15 | +0x2a |
| 15 | Heading | 0x543814 | computed | +0x2b |
| 16 | Influence | 0x543833 | stored | +0x2f |
| 17 | Jumping | 0x54383d | stored | +0x2e |
| 18 | Long Shots | 0x5439d6 | computed (0x544380) | +0x31 |
| 19 | Marking | 0x543847 | computed, weighted when `+0x0f` = 20 | +0x32 |
| 20 | Off The Ball | 0x543891 | computed, weighted when `+0x0f` = 20 | +0x33 |
| 21 | Pace | 0x5438db | stored | +0x36 |
| 22 | Passing | 0x5438e5 | computed | +0x37 |
| 23 | Positioning | 0x543904 | computed | +0x39 |
| 24 | Reflexes | 0x543923 | computed, weighted when `+0x0f` < 15 (else 0x544380) | +0x3a |
| 25 | Set Pieces | 0x543962 | stored | +0x29 |
| 26 | Stamina | 0x54399a | stored | +0x3c |
| 27 | Strength | 0x5439a4 | stored | +0x3d |
| 28 | Tackling | 0x5439ae | computed (0x544380) | +0x3e |
| 29 | Teamwork | 0x5439c2 | stored | +0x3f |
| 30 | Technique | 0x5439cc | stored | +0x40 |
| 31 | Work Rate | 0x5439ea | stored | +0x44 |

Names: the long names of `0x5428a0` (jump table `0x543158`), read from the strings of each case.

## Current and potential ability

- Current ability: player data `+0x05`, s16, 1 to 200 (`picker.c`; the formula reads it at 0x5435cc).
- Potential ability: player data `+0x07`, s16. TRANSFER_OFFER::restructure_fee_into_clauses (0x8e8330) computes `+0x07` minus `+0x05` at 0x8e8345..0x8e834d, the room a player has to grow. 0x4c8461 compares it with 100. The order also fits the community's description of the 3.9.68 player record (current ability, then potential ability, then reputations, then position ratings from `+0x0f`). Inference: in a running game it holds 1 to 200. The database also allows negative values for a random potential; the editor shows whatever value is there and writes 1 to 200.
- Current ability above potential ability does not last. In a test game (start.sav of `tests/fixtures`, Ray Parlour, staff 6110) the editor set current ability 165 and potential ability 160; one game day later the saved game had current ability 160 (`tests/smoke/player_editor_run.py`, 2026-10-24). Inference: the daily update cuts current ability to potential ability. The code that does it is not read yet. The editor keeps current ability at or below potential ability.

## Condition, injury and morale (slice 3)

The injury object is `[0xb63d28]` (the `this` of the calls at 0x87ab0b and 0x87ab2f in the profile build 0x87a510).

- `[obj]` is a table of 31-byte rows, one per staff id (`id * 31`: `shl 5; sub` at 0x62c493..0x62c49c). Row `+0x00` and `+0x04` hold the date of its last update, `+0x08` s16 a fitness ceiling (0 to 10000), `+0x0a` s16 the condition (0 to 10000), `+0x0e` s32 the index of the player's injury record or -1, and `+0x1e` a byte that the update subtracts player data `+0x34` from (0x62cb39..0x62cb43).
- Injury::update_player_fitness (0x62c680) brings a row up to today: it reads the days since the row's date (0x54cc30 at 0x62c7a7), clears an index outside `[obj+8]` (0x62c7cf..0x62c7d8), lets the condition recover, keeps the ceiling between bounds from player data `+0x34`, writes `+0x08` and `+0x0a` (0x62cdd3, 0x62cdd7) and stamps the date (0x417c10 at 0x62cddb).
- Injury::get_condition (0x62c590), `this` the object, (staff, 1): calls the update, then `+0x0a * +0x08 / 10000` (0x62c5e0..0x62c5fb), at most 9000 while the status is 1 and at least 100 (0x62c603..0x62c61d); with argument 1 it gives the percentage, rounded and at most 100 (0x62c62a..0x62c64c). The profile shows it as "%d%%" (0x87ab45).
- Injury::get_player_injury_status (0x62c400), `this` the object, (staff): 0 with no record. Else the record is `[obj+4] + index * 48` (0x62c4a9..0x62c4b8). The days since the injury (0x54cc30 with the record's date `+0x20`, `+0x24`; result s16 days and u16 years, years * 365 + days at 0x62c4cd..0x62c4e8) below the s16 `+0x2a` give 2, "Injured" on the profile (0xa86304 at 0x87ab1f); below `+0x28` give 1; else 0 (0x62c4ea..0x62c502).
- The editor cures an injury by setting `+0x2a` and `+0x28` of the record to the days already passed: the record then reads as an injury that has run its course, which the daily update meets anyway when the days pass. Inference: the game does not keep the end of an injury anywhere else; the status and the condition read only these fields.

Morale is player data `+0x45`, 1 to 20: staff_adjust_morale (0x544710) holds it between 1 and 20 (0x5447b8..0x5447ca) before it writes it (0x544811). contract_manager_write_morale_text (0x4cc2e0) shows Very Low below 4, Low below 8, Ok below 12, Good below 15, Very Good below 18 and Superb from 18 (0x4cc2e7..0x4cc384).

## Contract (slice 4)

- The contract of staff id `id`: `ci = int32 [[0xae2d0c] + id * 79]`, -1 for none; the record is `[[0xae1050]] + ci * 80` (CONTRACT_MANAGER::get_wage 0x4d0c20, `this` = `[0xae1050]`: 0x4d0c34..0x4d0c50; `docs/research/2026-10-15-contract-clauses.md`).
- Wage: int32 at `+0x0c` (get_wage returns it at 0x4d0c59).
- Dates: two Date objects (s16 day of the year, s16 year, u32 leap flag, as the game date at 0xae2c90) at `+0x25` and `+0x2d`, copied by Date::operator= 0x417c10 in STAFF_CONTRACT::operator= (0x4cc86e, 0x4cc882).
- The end date is `+0x2d`: STAFF_CONTRACT::format_end_date_text (0x85e8f0), which the profile's "  Expires" line calls (0x87d281), writes it with 0x54a650 from `this + 0x2d` (0x85e954). Before that it tests the type, the low 6 bits of `+0x35`: 3 "Contract Expired" (0xa8370c), 4 "Free" (0xa836e4), 1 "Rolling" (0xa836dc) (0x85e8f0..0x85e942).
- At the start of a game STAFF_CONTRACT::generate_initial_contract (0x85e960) copies the end date from the staff record `+0x46` (0x85ec81..0x85ec8d). Inference: from then on the contract record is the end date the game uses; no other code reads staff `+0x46` as a date (no `lea reg, [reg + 0x46]` in the generated code). The editor changes only the contract record.
- The editor moves the end date with Date::set 0x54a130 (thiscall: day 1 to 31, month 0 to 11, year, weekday or -1; `season.c`), so the leap flag stays right.

## Club money (slice 5)

- FinanceTable::get_club_finance (0x5a0590), `this` the table `[0xae2a38]` (the `mov ecx, [0xae2a38]` before all 25 calls), (club): after 0x540a50 (inference: the parent club of a reserve side) the record is `[this] + club id * 0x167` (0x5a062c..0x5a063c). The editor calls it, so it gets the same record as the game.
- The bank balance is the signed 64-bit value at `+0` of the record (`tools/save/edit_money.py`: set by Finance::init_for_club 0x5961d0, changed by the income and cost functions from 0x5987c0).
- The transfer budget has no field: Finance::get_transfer_budget (0x598800) works it out. It starts from the balance (the 64-bit value of `this`, 0x598807 and 0x598818) and adds, for each of the 50 squad places (club `+0xd7`), the staff `+0x52` value of players that 0x5444a0 accepts (0x59884c..0x59888b); the rest of its 1,246 lines (reputation, wage bill, board amount: `edit_money.py`) is not read yet.
- The rest of the function projects the season: it takes off the wage bill (finance `+0x0c` and the contracts of pending deals, 0x4ca300, 0x8ccb80), running costs from the club record (`+0x69`, `+0x6d`, `+0x7f`, `+0x80`, `+0x73`) over a number of periods (16, less the board's `+0x57` figure above 15, +5 for one nation, 0x598b13..0x598b56), and at the end limits the result with a figure from the board (staff record of `+0x53`, byte `+0x85`; 0x599687..0x5997dc). It stores nothing: each call works the budget out again.
- The finance screen shows it rounded: Cash::get_display_amount (0x43f460, thiscall on the s64 with (6, 6), 0x463b2c) and cash_format_money (0x441c10, 0x463b3d).
- Test (2026-10-24, start.sav, Arsenal): the function gave 14,392,763, which the finance screen shows as £14,500,000. After the editor's balance +1m it gave 14,492,763 (still £14,500,000 on the screen): the balance moves the budget by about a tenth here.
- So the editor makes the funds editable as an adjustment per club: a wrap of 0x598800 adds the club's amount to the function's result (at least 0). After Funds +1m the function gave 15,492,763 and the finance screen showed £15,500,000. A purchase lowers the balance, which lowers the game's own figure, so the extra is spent as the club spends; where the board's limit holds the figure, it can last longer. The adjustments go into the save part `bosman_editor.dat`.
- A player's club is staff `+0x39` (`compare.c`, `print_pdf.c`), 0 for a player without a club.
