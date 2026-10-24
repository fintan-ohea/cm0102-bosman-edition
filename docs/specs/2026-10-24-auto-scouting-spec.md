# Automatic scouting (fork of fintan-ohea)

Status: Slices 1 to 4 built on `feature/save-editor`, 2026-10-24 (`src/recomp_rt/win/scouting.c`, `savepart.c`). Decisions taken (end of this page). Research: `docs/research/2026-10-24-scouting.md`.

## Problem

In CM 01/02 a scout works only when the player gives him a duty: a nation, a competition, a club, a player or a match. The player must then read the game's short scout reports and compare each player with his own squad by hand. A player who manages for many seasons gives the same duties again and again.

## Solution

An option `auto_scouting`, off by default. With it on, the player's scouts work on their own: each week the assistant gives every scout of the player's club a duty that the game already supports, chosen from the squad's weakest positions, and the game's own scouting does the work. From what the scouts find, the assistant writes detailed reports: each attribute as a range whose width depends on the scout's Judging Player Ability and Judging Player Potential, a recommendation, and a comparison with the best player of the squad in that position. A Scout reports box in the squad's Assistant drop-down lists them, and a weekly news item says how many are new.

## Slices

1. **Automatic duties.** Each Monday (the weekly human turn), for each scout of the player's club without a duty the player set, the assistant picks a duty: a player at a position of need, a competition or nation where such players play, or a match of a target. Duties the player sets by hand stay; the assistant only fills scouts that are free. Built, with changes from the plan: each human turn (not only Monday) every free scout gets a duty through the game's own start_scout_assignment (0x7ae9c0), in turn the next opposition (the game scouts their matches and reports on them), youth players and the 14 regions. The game does not end a duty within 14 days (a run), so a duty the assistant gave is cleared after three weeks (clear_scout_assignment 0x7aee80) and the scout gets the next one. The duties cannot aim at positions of need: a search's filters (`+0x1fa`, positions GK 0x8, D 0x10, M 0x20, A 0x40 of the high word, set_option_flag 0x806630) act only when the game lists the results (get_result_list 0x7af40d is the one caller of player_matches_filter 0x807450), not on what a scout looks at. So the need acts where the reports are chosen (slice 2).
2. **Reports.** When a scout's work raises the player's knowledge of a target, the assistant writes a report: the player, the scout, the date, each attribute as a range (the true value is always inside), ability and potential as star ranges, the position, age, value and wage, a recommendation (Sign, Consider, Not needed) and the comparison with the squad's best player in that position. The reports go into the save as a part, as the editor's mark does. Built: up to 5 reports a game day from the results of every scout search, highest stars first, for players of other clubs, ranked by stars plus 3 when the player's best role is one of the two roles whose best squad player scores lowest (the roles the squad needs); a player gets a new report only from a better judge or after 60 days. Ability and potential are the game's own stars from the results, which already hold the scout's judgement (report_on_player 0x7b62a0); 0 shows as "-". The player is scored in his best role from the middle of his ranges as the Assistant scores (key attributes x fit / 20) and compared with the best two of the squad in that role. Sign: more than 0.5 above the best, or potential 4 or more at 21 or younger and within 2 of the second best. Consider: at least the second best, or of that potential, or potential 3 at 23 or younger. Each report keeps the player's value (staff `+0x52`, the value the search screens sort by, 0x819fa6) and his weekly wage (the contract, as the editor reads it) at its date, and whether its role is one the squad needs. Save part format 2 (reports of 108 bytes); format 1 (100 bytes, no value or wage) still loads, with value and wage shown as "-".
3. **Scout reports box.** From the squad's Assistant drop-down: the reports, best first, with a page per report. Built: 14 a page, Sign first, with the value, and the role in yellow when the squad needs it; a click on a name opens the report with the 31 ranges (the role's key attributes in yellow), the scout and his judging, the date, the stars, the comparison, the value and the wage.
4. **Weekly news.** A news item each Monday: "Your scouts filed N new reports", when N > 0. Built: type 0x1400 in the range of the game's scout news, made as season_change.c makes its news; the body counts the Sign and Consider advice and says where the box is. A report counts as new until the box closes.

## Accuracy

The width of a range comes from the scout's Judging Player Ability (attributes and current ability) and Judging Player Potential (potential): a scout with 20 gives the true value or one point either side, a scout with 1 gives up to five points either side. The range is fixed for each scout and player (a hash of the two ids, not the game's random numbers, so the game's results do not change), and a second report by a better scout narrows it. Built: a range is the true value and SPAN = 1 + (20 - Judging Player Ability) * 4 / 19 more numbers (1 for 20, 5 for 1), placed by the hash; the potential is the game's stars, not a range of the fork's own.

## The data (3.9.68), read so far

- What a manager can see of a player: FOG_OF_WAR, one map per manager (FOG_OF_WAR::get_manager_map 0x5afde0 on `[0xae2a60]`). FOG_OF_WAR_MAP::get_knowledge_level (0x5b02f0) gives 0 to 3; staff_is_player_attribute_visible (0x543280) shows every attribute at 3 and hides a set at 2 and 1 that depends on the player's id, when attribute masking is on (`[0x9cc270]`). FOG_OF_WAR_MAP::raise_knowledge (0x5b09d0) and daily_update (0x5b0260) change it.
- A scout's judging: non-player attribute 5 "Judging Player Ability" is coaching data (staff `+0x69`) `+0x17`, attribute 6 "Judging Player Potential" `+0x18` (staff_compute_non_player_attribute 0x543fd0, names 0x543b10 with 0x9b7c28 and 0x9b7c08).
- Scouting: SCOUT_MANAGER (0x7f6760..0x7fc310: daily_update 0x7f8750, add_scout_request 0x7f9290, record_scout_report 0x7f9520, send_scout_reports 0x7f9890) and PLAYER_SEARCH's scout duties (set_scout 0x7ae8a0, start_scout_assignment 0x7ae9c0, run_scout_day 0x7b3e20, scout_match 0x7b5be0). Not read yet.

## Tests

- Same game: `smoke_run.py --same-save` with `auto_scouting` on and off. On changes the game on purpose (scouts work), so the runs check that off equals normal and that on equals itself when run again.
- A run with the option on: after two weeks the scouts have duties, reports exist, the box lists them, and a save and load keeps them.
- Accuracy: for every report the true attribute is inside its range, and a better judge gives narrower ranges. A run with `RECOMP_SCOUT_JUDGING=1` (test only: the reports are written as by scouts of judging 1) checks that every range is then 6 numbers wide and still holds the true value.

## Decisions

fintan-ohea, 2026-10-24.

1. The scouts work through the game's own duties (slice 1), not a model of the fork's own.
2. The reports are in a box in the squad's Assistant drop-down, with a weekly news item.
3. Accuracy is shown as ranges whose width follows the scout's Judging ratings.
