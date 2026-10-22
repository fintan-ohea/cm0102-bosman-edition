# 0.6.0 load sweep (2026-10-21, O162)

Scope: every 0.6.0 season module (`src/recomp_rt/win/season_*.c` and the `tools/recomp` patches) on main f958309, read for faults that show only after a saved game is loaded. Five Opus readers took one area each with one brief: core and save, domestic leagues, cups, UEFA, international. Fixes are on `claude/fix-load-sweep`. One Opus review checked the fixes.

The known cases were O145 (Euro 2028 qualifying slot 9, fix 97b3d92), O160 (12-group World Cup qualifying, fix f85cd6d on its own branch) and O161 (match_eng 612, open, another session).

## Fixed

1. **Stage slots after +0x30 hold old heap bytes after a load** (the O145 class). League::load 0x687b10 allocates the stage array with malloc (0x688e3a..0x688e5b) and fills only slots 0 to +0x30 (0x688ea1..0x689017). Cup::load_from_disk 0x51fc00 does the same (0x520c8a). The season code reads later slots: `stage_of` in `season_intl_qual.c` (CONCACAF stage 6, OFC stage 1) and `season_wcq26.c` (Asia 11, Africa 0 to 8, OFC 1), and the 32-club Club World Cup slot 7. Fix: `season_load.c` wraps League::load, and the Cup::load wrap calls the same helper. After a load that succeeds, in a season game, the slots after +0x30 are set to 0, up to the block size (or +0x2c). The original itself never reads them before it builds them, and its save writes only slots 0 to +0x30 (0x68991a).
2. **Euro qualifying +0x2c 9** (`season_intl_euroq.c`). `euroq_advance` sets +0x2c to 9 when no stage follows the groups, and start_new_season 0x57eb20 keeps it. After a load League::load gives 9 slots, and the next ten-group campaign writes slot 9 past the block. Fix: at the draw of group 0 of a ten-group campaign, a count below 10 gets a new cleared array of 10 slots and +0x2c 10. A game that is not loaded also changes: that campaign can reach its play-off step again.
3. **Nations League places overwritten at a load** (`season_intl_euroq.c`, `season_nl.c`). The ctor epilogue 0x57d03b calls `rt_euroq_rules` while a Nations League object still has the class vtable, so its loaded +0xbe and +0xbf took the Euro qualifying values. Fix: `nl_make` marks its constructor call, and `rt_euroq_rules` then leaves +0xbe and +0xbf alone.
4. **World Cup play-off ties** (`season_intl_qual.c`). `built[]` was not saved and not reset, so a second load kept the first game's teams. Fix: reset in `season_change_reset`. With an empty list, the ties are read from their built stages (Asia stage 12, Oceania stage 2, only when +0x30 has reached them).
5. **New super cup club list** (`season_new_cups.c`). Neither ItaSuper 0x65f670 nor Cup::Cup 0x518640 sets +0xa7. A picker that finds no clubs kept the old value, and `put_list` freed it. Fix: +0xa7 and +0x3e are set to 0 before the constructor.
6. **Primera Federación and Liga 3** (`season_divisions.c`). After a load the array has 1 slot, and the July builder calls wrote slots 1 and 2. Fix: no write past the block. A season of the class's form after a load grows the array to the class size (SpaSecondB 9 slots, PorSecondB 2) before the original builder runs.
7. **3. Liga stage slot** (`season_divisions.c`). The save keeps +0x2c 0, so the load makes no array, and GerFirst's season start reads `[[+0xc]]` with no test (0x5dbf7f). Fix: after League::load, a GerRegional object holding the 3. Liga gets its cleared 1-slot array again.
8. **League phase pending list** (`season_uefa_league.c`). `pending_stage` and `pending_list` stayed set after an unwind. Fix: reset on the unwind and in `season_change_reset`.
9. **Names at a load** (`season_names.c`). A part with no renames took the names of the season of the date, so a competition took its new name before its update day and lost its "is now called" news. Fix: a part with news fields (season point above 0) keeps the names of the data. A version 1 part keeps the old fallback.

## Refuted

- Uefa_Cup +0xb2 "not saved": the class's load slot 0x903300 reads it (0x903315) for save versions above 0x3e003, and the save writes it (0x903340). No change.

## Leads for O161 (not changed here)

- `season_records_fill` (`season_records.c:1137-1140`) gives fill players a contract to 30 June of the year after the start, so the squads of new club records lose them on 30 June 2023 in a 2022 start.
- The 1 September European registration pass (0x9058e0 calling 0x85a9b0 with 2) registers only the clubs of each competition's list. 0.6.0 changes the Europa League's list (`season_uefa_qual.c:616-626`).

## Retests

| Test | Scenario | Result |
|---|---|---|
| T1 | 2005 start, September 2005 Helvete, England, rules on. Save 21 October 2005, load, play to June 2007 | PASS. The tie and the World Cup 2006 draw are made after the load, with no box |
| T2 | 2018 start, October 2018, England, rules on. Save 16 November 2018 (Nations League groups), load, play to June 2020 | PASS. The Nations League ends its groups and makes the finals |
| T3 | 2008 start, September 2008, England and Germany with the regional division, rules on. Save 19 September 2008, load, play to June 2010 | PASS. The 3. Liga makes its July 2009 change (3 down, 3 new clubs), no box |
| T4 | 2021 start, October 2021, Spain and Portugal extra divisions, rules on. Save 15 October 2021, load, play to June 2023 | PASS. spa_second_b and por_second_b make their July 2022 change after the load, no box |

T1 to T4 ran on the first fix commit. After the review round (fix 8 dropped, the +0x30 test of fix 4, the array growth of fix 6), T1 ran again on the final build: PASS. Fix 6's growth path needs a season row that leaves the new form, and no such row exists yet, so it has no game test.

ctest 45 of 45 on both builds. Same-save start.sav set (17 runs, the run against main-f958309 included) PASS on both builds.
