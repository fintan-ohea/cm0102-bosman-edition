# Training assistant (fork of fintan-ohea)

Status: Slices 1 to 4 built on `feature/save-editor`, 2026-10-24 (`src/recomp_rt/win/training.c`), as three tabs of one box: Schedules (slices 1 and 4), Explain (slice 2), Coaches (slice 3).

## Problem

Training in CM 01/02 works through schedules: each human manager has ten, each schedule sets five categories (Fitness, Tactics, Shooting, Skills, Goalkeeping) to None, Light, Medium or Intensive, and each player follows one schedule. Coaches are assigned to categories. The game shows none of the numbers behind it, so players set it up by forum lore.

The forum topic "How Training Really Works" (alex91, champman0102.co.uk t=11012, 2026-10-11) describes the numbers. It says it comes from the machine code, but it was written with ChatGPT and gives no addresses. In the same topic Shephard 4.2 describes a weekly cycle (5 days Intensive, 2 days Medium) that works but needs a change of every schedule twice a week; other posters say it is too much work.

## Solution

A training assistant behind the option `training_assistant`, off by default. It opens from the Assistant drop-down of the player's own squad screen (`assistant.c`), as Team views does, and works on the manager's schedules directly. It does not open from the Training screen, because that screen edits a copy (TRAINING_EDIT_SESSION 0x8b0a00) and its Ok writes the copy back (apply_changes 0x8b0b40).

Every number the assistant shows or uses is read from the code first, with its address in `docs/research/2026-10-24-training.md`. A claim of the forum topic that the code does not confirm is not used.

## Slices

1. **One-click schedules.** The box shows two schedules from the forum topic, Outfield (Fitness Medium, Tactics Intensive, Shooting Medium, Skills Intensive, Goalkeeping None) and Keepers (Fitness Medium, Tactics Medium, Shooting None, Skills Medium, Goalkeeping Intensive), with their workload (94 and 81 by the game's own formula, not the 95 and 82.5 of the forum topic: research note). Apply writes them into the manager's two "Unused" schedule slots and puts every player of the first team on one of them by position. A slot that is not unused is not written; the box says so.
2. **Training explainer.** The box lists the manager's schedules with their levels and workload, and for each category the attributes it trains (after that mapping is read from the code).
3. **Coach advisor.** The box shows each coach's rating for each category and the effect of one coach on several categories, and suggests one specialist per category. Apply sets the assignments. Only after the rating formulas are read from the code. Built: the ratings and factors use the formulas of the research note, and the box checks itself against the game (the factors it works out equal get_coaching_factor 0x8b41d0). The suggestion tries every assignment with up to two categories per coach and keeps the one that gives the most first team player-categories a training quality above 100 (where a player can improve), then at least 110, then the highest factors weighted by players, then the fewest assignments. Maximising the factors alone left Shooting at 50 in a test, where Medium Shooting has quality 100 and trains nothing. The box refreshes the coach table with the game's own update_coach_assignments (0x8b2880) when the tab opens.
4. **Automatic 5/2 cycle.** A second switch in the box: the Outfield schedule trains Intensive in four categories for five days and Medium for two, every week, without the player's clicks. Keepers stay as they are. It changes how the player's team trains, so it is off until the player turns it on, and the box says what it does. Built: the option `training_cycle` (also a row of the Tools tab); each human turn (0x5cde40, `testrun.c`) sets Outfield to Intensive from Sunday to Thursday and Medium on Friday and Saturday (Shephard: Medium on the two days before the weekend match), for every human manager whose slot 8 holds the assistant's Outfield. Turning it off puts Outfield back to its plan. Like a change on the Training screen it does not bring players up to date first (research note), so it does what the manual method does. The check that a save equals a run with the same changes by hand is not built.

## The data (3.9.68), read so far

- The training manager is `[0xde1f44]` (the `this` of the calls at 0x5cf367, 0x5d3786, 0x5d6b48).
- TRAINING_SCHEDULE, 0x9b bytes (TRAINING_SCHEDULE::TRAINING_SCHEDULE 0x8b4e10): `+0x04`..`+0x08` the levels of Fitness, Tactics, Shooting, Skills and Goalkeeping (get_category_level 0x8b4f50, set_category_level 0x8b4f30; the names at 0x8b5520); `+0x09` the name, 11 characters and the end (0x8b51f4..0x8b520b); `+0x15` the type; `+0x16` position training flags (0x8b5090, 0x8b50c0); `+0x93` set to 0 by every change.
- The levels are 0 None, 10 Light, 25 Medium, 50 Intensive (set_type 0x8b4e50 writes 0, 0xa, 0x19, 0x32; the names at 0x8b5690).
- Types (set_default_name 0x8b50f0): 0 No Training, 1 General, 2 Fitness, 3 Shooting, 4 to 7 the other defaults, 8 a schedule of the manager's own (no default name).
- Workload (TRAINING_SCHEDULE::get_training_load 0x8b4f60): 1.3 x Fitness + 0.5 x each other category ([0x96f878] = -1.3, [0x969540] = -0.5, [0x968968] = 0.5, all subtracted), at most 100 (0x8b4fe7). The forum topic's formula is right.
- A human manager's schedules: get_manager_schedule (0x8b3930), (manager, index 0 to 9): `this + (h * 10 + index + 7) * 0x9b`, h = the manager's place among the 16 human staff at the end of the staff table (`[0xae2364] - id - 1`).
- A player's schedule: a 6-byte entry per player at `[this + 0x651d] + player id * 6`; byte 0 is the schedule index (set_player_schedule 0x8b2250, which first brings the player's training up to date with 0x8b1750); bytes 3 to 5 hold signed 4-bit progress values per category (remove_or_restore_training_progress 0x8b3640, data map). get_player_schedule (0x8b39f0) uses the manager's schedule only when the club's manager is human.

## Tests

- Same game: `smoke_run.py --same-save` with `training_assistant` on and off; with it off, and with it on but unused, the saves equal normal.
- A run opens the box, applies the schedules, saves and loads: the slots hold the levels and every first team player follows Outfield or Keepers.
- The 5/2 cycle: a run of 14 days logs the level changes on the right days; the save equals a run where the same changes are made by hand.

## Open

- A player who joins the squad gets slot 0 (research note), not Outfield or Keepers: Apply again, or a later slice puts new players on the right schedule.

- Where "first team" ends: players on loan to the club (Shephard reports that they do not train) and reserves.
- What the Training screen shows for a slot that the assistant writes while the screen is closed.
