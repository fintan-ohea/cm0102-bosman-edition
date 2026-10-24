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
