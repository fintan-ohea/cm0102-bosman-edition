# New divisions (0.6.0 slice 20)

Date 2026-10-20. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q232, Q247, Q290, Q291, Q331, Q332, Q363). Code: `src/recomp_rt/win/season_divisions.c` and `.h`, `tools/recomp/new_divisions.py`, small blocks in `season_clubs.c`, `season_sizes.c`, `tools/seasons/make_clubs.py`, `tests/seasons/rules_report_check.py`.

## Parts

The research of 2026-10-19 and 2026-10-20 read the code of each part (one research file per part, outside the repo). Each part is medium to large:

| Part | Design | Size |
|---|---|---|
| 20a Germany | the GerRegional object holds the 3. Liga from 2008/09 | medium to large: 8 wraps and 2 sites |
| 20b Italy | groups A, B, C in the C1a, C1b and C2a objects; a new start and chain code for Serie A | large: built in a session of its own (branch claude/slice-20b) |
| 20c Spain, Portugal | Primera Federación and Liga 3 in the Segunda B objects, fewer groups | medium to large |
| 20d Greece | a second GreSecond object with a new record (slice 26's module), a cup wrap | medium to large |
| 20e Brazil | 15 state championships with a picker by state, 12 new records | large: built in a session of its own (branch claude/slice-20e) |

Only a season game (`season_game_changes`) gets any of it. The base game and `RECOMP_SEASON_CODE=off` keep the original (Q152, Q342). `RECOMP_NEW_DIVISIONS=off` is the test switch in a season game, `RECOMP_DIVISION_LOG=1` prints a line for each count and date template.

## 20a Germany: the 3. Liga

### The original

- `create_german_comps` 0x8343f0 makes 5 objects when bit 0x04 of nation +0x11c is set (the extra division flag, `test byte [edx+0x11c], 4` at 0x8344d5), else 4. The fifth is `GerRegional` (constructor 0x5dcbd0, record key [0x9cf564] "German Regional", made at 0x83467c). So Q363 holds by itself: the 3. Liga exists only with the flag, as the Regionalliga.
- One object, two groups. The object's table is the North group: `GerRegional::create_table` 0x5dd7f0 takes the clubs whose division (+0x57) is German Regional and whose group tag (+0x60) is North [0x9cf55c] (0x5dd8d5..0x5dd8f1), 18 of them (store at 0x5dd809; 19 when +0x40 is 2000, 0x5dd801). A club after the count gets the box ger_regional.cpp 332 (0x14c at 0x5dd998). The South group is a child stage made by `create_south_stage` 0x5ddc50 (exactly 18 clubs, box 528 with fewer at 0x5dde2a), called by the constructor at 0x5dcdcd and by `start_new_season` 0x5dda90 at 0x5ddc24.
- The stage is read with no test by `GerFirst::process_promotion_relegation_chain` 0x5db6c0 (0x5db73f, 0x5db78a: 0x689c80 with the stage at 0x5db79c), the exchange 0x5db980 (second pass, 0x5dba5d), `get_club_array` 0x5ddff0 (36 slots, 0x5de005..0x5de018; 0x5de0d5) and `set_predicted_positions` 0x5de130 (0x5de202). The destructor (0x5dce80) and `start_new_season` (0x5ddacc) free the stages up to +0x30 only.
- `GerFirst::start_new_season` 0x5db4a0: the chain (vtable +0xb0), then the exchange 0x5db980 when the regional object exists (else `ger_first_exchange_regional_clubs_unplayed` 0x5db7b0), then the split 0x5dbbd0 (36 clubs by latitude into 18 North and 18 South; box ger_first.cpp 1049 at 0x5dbcd4 with fewer), then +0x40 + 1 (0x5db52a).
- The exchange takes new clubs from a pool of up to 200 German clubs (nation [0x9cf314]) with no division or in A Lower Division [0x9cf764], whose senior side (0x540a50) is not in German Regional or A Lower Division (0x5db9bc..0x5dba17). Relegated rows go to A Lower Division through 0x6831a0 (0x5dba9e) and lose their tag (0x5dbaad). The pool is sorted by 0x4afbd0 (0x5dbb6f) and the first clubs come in through 0x6830b0 (0x5dbbae).
- `GerRegional::create_schedule` 0x5dcef0: D = 34 (0x5dcf12); the writes of 0x68a160 fill dates 0 to 33 (the last at 0x5dd7a6, date index 0x21).
- `GerRegional::set_up_rules` 0x5dd9f0: meetings 2 (0x5dda00), +0xbe 2 (0x5dda2f), +0xc1 4 (0x5dda4c).
- `GerSecond::set_up_rules` sets its lower division +0x20 to the regional record only with the flag (0x5df441..0x5df457).

### What the game does

In a season whose rules table has one `ger_regional` row (the 3. Liga, 2008/09 on; after 2027/28 the last row stays, Q248):

1. **Rules table (Q520).** The 3. Liga rows of 2008/09 on take comp_file `ger_regional`, and the Regionalliga rows of those seasons `n/a` (below the 3. Liga: Q331). The names table then gives the GerRegional object the name "3. Liga" (`season_change_rename` of the "German Regional" record).
2. **Count.** `rt_ger_regional_clubs` at 0x5dd801 and 0x5dd809 (`tools/recomp/new_divisions.py`): the clubs of the division, from 8 to 24. A new game puts the 20 clubs of the list into it.
3. **Dates.** A wrap of 0x5dcef0 makes a template of 2 (n - 1) dates: the 34 of the class, and each extra round on the Wednesday after a Saturday whose next date is at least a week later, spread over the season. With fewer than 18 clubs (only when the pool runs short: the log says "new clubs needed") the class's 34 dates stay and the last ones are not used.
4. **Places.** A wrap of 0x5dd9f0: +0xbe the row's up and the play-off place, +0xc1 the row's down. The play-off of the 3rd against the 16th of the 2. Bundesliga has no code (slice 19 plays a play-off inside one league), so its place goes up and down directly (Q521). The 2. Bundesliga with the 3. Liga below gets the same count down (`season_divisions_rules`, called after slice 18's places), so the pairs of 0x689c80 stay even.
5. **No South group.** A wrap of 0x5ddc50 makes no stage: the slot is cleared, +0x30 stays -1 and the stage count +0x2c goes to 0 (the constructor sets 1 at 0x5dcc3d), so save, load and destructor see none. With +0x2c at 1, `League::update_for_date` 0x6847c0 took -1 < 0 as a stage to come (0x6849d1..0x6849da), called the empty advance_stage and skipped the season's end: the history (0x442e80 at 0x6849fa), the honours (vtable +0x30 at 0x684a8f) and the complete flag +0x4c (0x684a9e). Found in review 1; `tests/seasons/new_divisions_check.py` checks the flag at the season change. Wraps of 0x5ddff0 and 0x5de130 give the one table (positions 1, 2, 3, ...). A wrap of the chain 0x5db6c0 makes the same calls less the stage.
6. **Season change.** A wrap of the exchange 0x5db980 (when the season that starts has the 3. Liga): the relegated rows leave to A Lower Division as in the original (Q522: the German Cup takes its lower clubs from there, 0x5d94c6, and not clubs with no division, 0x5d94ad). At the change from two groups (2007/08 to 2008/09) the clubs below the row's count leave by table position, North and South in turn; the clubs that came down from the 2. Bundesliga stay. Then the pool fills the division to the row's count, with the original's pool and sort. A wrap of the split 0x5dbbd0 gives every club of the division the North tag.
7. **Club moves at a new game.** The 3. Liga lists go to "German Regional Division North" (`make_clubs.py`). With the flag, South takes 0 clubs: they go to no division (Q290). Without the flag, the 3. Liga list does not apply and North and South keep 18 each, because the unplayed exchange 0x5db7b0 and the split need 36 (`season_divisions_class_size`, `season_divisions_list_on`).

With changing rules off the start season stays (Q232): a 2008 start plays the 3. Liga in every season, a 2007 start the Regionalliga in every season.

### The box ger_first.cpp 1049

The research of 2026-10-19 found that with the flag off 0x5db7b0 moves a fixed 4 clubs up (0x5db8d8, 0x5db8e8), so a 2. Bundesliga with 2 relegation places would leave the pool at 34 and stop the split. Slice 18 already keeps the 2. Bundesliga's places when its lower division is not a size-table division (`open_neighbour`, commit 6587a7c), so it keeps 4 down. The flag-off test run below checks it.


## 20c Spain and Portugal: Primera Federación and Liga 3

### The original

- `create_spanish_comps` 0x837560 makes one `SpaSecondB` object (constructor 0x855f40, record [0x9cf7b8] "Spanish Second Division B") only with the extra division flag (`test byte [edx+0x11c], 4` at 0x837647). Its own table is group 1 (B1 [0x9cf62c], picked by club +0x60, `load_group_clubs` 0x856cd0, 20 at 0x856ce2). Groups 2 to 4 are League_Stage objects built by 0x857210 for slots 0, 1, 2 (constructor 0x85614c, `start_new_season` 0x85712a), 20 clubs each. `SpaSecondB::advance_stage` 0x857150 builds the promotion play-off when +0x30 reaches 3 (0x8571d0).
- `SpaFirst::start_new_season` 0x84feb0: the chain 0x850700 (Segunda with group 1 and stage slots 0, 1, 2 at 0x8507eb, 0x850818, 0x850846, 0x850874, read with no test), new clubs 0x850300 (pool: Spanish clubs of A Lower Division, 0x850325..0x85036f; relegated rows to A Lower Division at 0x8503e4; a reserve side whose senior side is in Segunda B is passed over, 0x8504c9..0x8504ec) and the group deal 0x850530 (needs exactly 80 clubs, 0x8505da; sorts by 0x53fd40, push at 0x85067f), all before +0x40 + 1 (0x84ff3a).
- `create_portuguese_comps` 0x8366e0 makes `PorSecondB` (constructor 0x7d1920, record [0x9cf7c0]) only with the flag (0x83694f). Its own table is the North zone (`create_table` 0x7d2400, 20 at 0x7d2412), the Central and South zones are stages built by 0x7d2870 for slots 0 and 1 (exactly 20: 0x7d2965, 0x7d29ff; League_Stage of 20 at 0x7d2ab2). `PorPrm::start_new_season` 0x7cf9e0 runs the chain 0x7cfbf0 (stage slots read at 0x7cfcdd, 0x7cfd0a), new clubs 0x7cff70 (pool: Portuguese clubs of the Third Division [0x9cf620] or A Lower Division, 0x7d0031..0x7d005b; relegated rows to the Third Division at 0x7d010f) and the zone deal 0x7d0210 (sort 0x53fd40 pushed at 0x7d02c5).
- `League::update_for_date` 0x6847c0 calls `advance_stage` (vtable +0x28) while +0x30 is below +0x2c - 1 (0x6849d1..0x6849e3).

### What the game does

In a season whose row has the new form (spa_second_b 40 clubs from 2021/22, por_second_b 24 or 20 from 2021/22), with the flag (Q363; no flag, no object, as in the original):

1. **Two groups.** The wraps of the group builders 0x857210 and 0x7d2870 build slot 0 only, clear the other slots (`start_new_season` deletes the stages and keeps the pointers, 0x856fd6, 0x7d26f6) and set the stage count +0x2c to 1, so no advance_stage runs: no play-off stage (Spain's 0x857570 reads four groups).
2. **Sizes.** Spain keeps 20 a group. Portugal takes the row's half (12, then 10) at 0x7d2412, 0x7d2965, 0x7d29ff and 0x7d2ab2 (`rt_por_group_clubs`, `rt_por_stage_clubs`); the class never resets +0xa7, so a new count resets it.
3. **Places** (wraps of `set_up_rules` 0x856ef0, 0x7d2610, and the stage after it is built). Spain: the group winner and the play-off place (Q521) up, 2 a group, 5 down a group. Portugal: the winners of the promotion series (Q521), 1 a series, the row's down shared. The division above (Segunda, Liga Portugal 2) relegates as many as the tier sends up (`season_divisions_rules`).
4. **Season change.** Wraps of the chains (the calls less the cleared slots), the new clubs (the shared exchange of 20a: relegated rows to the original's division, at the change from more groups the clubs below the row's count leave by table position, then the pool fills to the row's count) and the deals (sort by 0x53fd40; the first half, the southern clubs, to the second group, the rest to the first: Spain B2 and B1, Portugal Central and North).
5. **Club moves at a new game.** Primera Federación Group 1 and 2 go to B1 and B2, Liga 3 Série A and B to North and Central (`make_clubs.py`). The groups that end (B3, B4, South) keep their database clubs, unplayed (Q527): wraps of the first-season merges (0x856cd0 at 0x856d0e..0x856d3a, 0x7d2400 at 0x7d243e..0x7d2462) leave them out of the tier. The cups need them: `PorCup::collect_entrants` 0x7ce220 counts the three zones (0x7ce2d6..0x7ce2f0), and a 2021/22 start whose South zone had no clubs stopped at por_cup.cpp 74 (box at 0x7cdb19). Without the flag the lists do not apply.
6. **The Portuguese Cup.** Only club rows count: the last 2 x nations rows of the club table are national teams, which the original's null-division jump kept out (review 1).  `PorCup::collect_entrants` 0x7ce220 wants 164 entrants (0x7ce234): zone and tier clubs (pass 1), then the Third Division and Portuguese clubs of A Lower Division (pass 2, a club with no division is passed over at 0x7ce37d). A count before the cup of a 2021/22 start: without the flag 60 zone, 13 Third Division and 62 A Lower Division clubs; with Liga 3 24 tier, 20 South, 13 and 55, as the club moves leave 23 more clubs with no division. So the cup stopped at por_cup.cpp 74, also with the slice's code off (the database has too few lower clubs for one tier of 24). The cause (O260, row A24 of the guard review): the club moves left the clubs of Liga 3's group divisions that no list keeps with no division, while the season change sends the clubs that leave the tier to the original's division of relegated clubs (Portugal's Third Division [0x9cf620], 0x7d010f; Spain's A Lower Division, 0x8503e4). The Third Division is a data division (no object reads [0x9cf620]: only the key setup 0x602149, the cup 0x7ce381 and the tier's exchange 0x7d0031..0x7d0118) and the pool the tier takes clubs from. So the club moves now send those clubs there too (`season_divisions_leaver_division`, a club with a nation only), and the cup's second pass takes them. `rt_por_cup_none` at 0x7ce37d (Q528) stays as a logged safety net (the line "Portuguese Cup ... takes ... with no division (safety net)"), and the test switch `RECOMP_BOX_FIXES=off` turns it off. `tests/seasons/new_divisions_check.py --only por-cup` runs a 2021/22 start with the net off.

Spain 2020/21 (5 groups split into 10 subgroups, 102 clubs) keeps the database's 4 groups of 20 (Q524). The rules report keeps a `clubs` line for these group classes (the object's table holds one group).

## 20d Greece: the second group of the second tier

### The original

- `create_greek_comps` 0x8347c0 makes GrePrm, GreSecond, GreCup and GreSuper with no extra division flag (0x834965..0x834b02). Greece is entry 18 of the league table (the function is stored at 0xb64278).
- `GreSecond::create_table` 0x5ebcb0 takes the clubs of the key id [0x9cf870] (0x5ebd54..0x5ebd64), 14 (0x5ebcc2, table of 14 rows at 0x5ebcbb). The constructor 0x5eb2b0 calls it directly (0x5eb35e).
- `GrePrm::start_new_season` 0x5eaf50: the chain 0x689c20 (GrePrm with [0x9cf870] at 0x689c60), then 0x5eb120 (2 clubs of the Lower Division [0x9cf874] up by 0x4afbd0 and 0x6830b0, the relegated rows down by 0x6831a0).
- `GreCup::collect_entrants` 0x5e9a80 needs 44: 16 Lower Division clubs, the National B and the National A Division. The group keeps its own record, so the cup stays as it is.
- 0x689c80 marks both rows of a moved pair with 0xff (0x689df4, 0x689dff), so a second call against a second lower division moves no club twice.

### What the game does

1. **Object.** A def of slice 26's module (`season_div_gre_group`): comp "league group 2", entry 18, a new record "Greek Second Division Group 2" like the National B Division (its level too: a def level of -1 copies it, a small block in `season_comps.c`), and a GreSecond object (0xee bytes, constructor 0x5eb2b0). Its rows take comp_file gre_second (Q520), so it is the second gre_second object of the report and the names table; the group names are "Football League North/South" and "Super League 2 North/South" (Q525).
2. **Clubs.** A wrap of 0x5ebcb0 gives the key [0x9cf870] the group's record id while the original runs for the group's object. The class holds 14, so the group holds at most 14 (17 in 2021/22 and 15 in 2022/23 are capped, Q526). The club moves fill the new record from the lists (`season_clubs.c` takes a record with no database club when the module adds it). A group that starts later in a game takes its clubs from the Lower Division (the original's pool) before its constructor runs.
3. **Places.** In a season with the group both groups promote their own row's up count, at least 1 (the group winner: the joint play-offs have no code, Q521), so North and South take one rule (review 2). The National B Division keeps its 2 relegations (slice 18 floor: its exchange 0x5eb120 brings 2 back, 0x5eb219); the group takes its row's down. The National A Division relegates the promotions of both groups.
4. **Season change.** A league with a division above is started by its top league (GrePrm starts the National B Division at 0x5eb0ec..0x5eb0f7), so the group starts in the same wrap, after its clubs are in place. The new clubs come first, while the group's relegated clubs are still in it, so neither the group's pool nor the original's pool for the National B Division (0x5eb1d7..0x5eb20f) takes a club that has just gone down (review 1). A wrap of 0x5eb120: the group's exchange with GrePrm (a second 0x689c80 call), its relegated rows to the Lower Division, then its clubs for the next season from the Lower Division, or all to the Lower Division when the next season has no group. Then the original for the National B Division.
5. **Differences.** The Greek Cup takes no group 2 club: `GreCup::collect_entrants` 0x5e9a80 reads only [0x9cf870] (0x5e9b9d..0x5e9bd5). It still gets its 44 entrants (16 + 14 + 14), so nothing breaks; the real cup takes both groups (a cup matter for slice 21).
6. **Gaps.** At the change into a season with no group, the wrap of 0x5eb120 does not start the group (review 2): it keeps its finished season and waits with the year before its next edition. The rule kind "divisions" keeps the group idle (`season_change_skip`) in the seasons with no group, and at its return sets +0x40 to the year before, so `start_new_season` makes the new dates (the third case of `season_comps.h`).

## Decisions

- **Q520:** a new division that lives in the object of an old one takes that object's comp_file in the rules table from its first season; the old division's rows of those seasons become `n/a` with a note. Done for the 3. Liga (as Q291 does for Spain and Portugal).
- **Q521:** a play-off against the division above, which slice 19's code cannot play, becomes a direct place up and down (slice 19's nearest form). 3. Liga: 3 up, 2. Bundesliga 3 down. Spain: the 2 play-off places go to the two runners-up (2 a group). Portugal is the exception (review 2, lead): its 2021/22 and 2022/23 play-off is the two series runners-up against each other, then the winner against the 16th of Liga Portugal 2. That is one place for the better of two tables. The places of the class are per table (+0xbe of the object and of its stage, set before the season), and no code compares two tables at the season's end, so one direct place cannot go to one runner-up. A place a series would send up 4 against the real 3. So Liga 3 sends up the two series winners (1 a series) and Liga Portugal 2 relegates 2.
- **Q523:** the pool of new clubs of a new tier 3 takes the nation's clubs with no division too (Q331: promoted clubs come from that pool), besides the original's pool divisions.
- **Q524:** Spain 2020/21 keeps the database's 4 groups of 20; the code holds no 10 subgroups.
- **Q525:** the two Greek groups are called North and South in their names ("Super League 2 North"), so the two divisions do not show the same name.
- **Q526:** the second Greek group holds at most 14 clubs (the GreSecond table); the real 17 and 15 stay `clubs` lines of the report.
- **Q527:** at a new game the Spanish and Portuguese groups that end keep their database clubs, unplayed, as the original keeps the tier 3 divisions of a nation without the flag; the cups take their entrants from them (a deviation from Q290, which would leave the Portuguese Cup short).
- **Q528:** in a game with Liga 3 the Portuguese Cup also takes Portuguese clubs with no division as lower entrants (a small cup site that slice 21 may replace). Since O260 a safety net only: the club moves send the clubs that leave Liga 3 to the Third Division.
- **Q522:** clubs relegated from the 3. Liga go to A Lower Division, as the original's Regionalliga does, so the German Cup still takes them. The clubs of the ended South group at a new game go to no division (Q290).

## Tests

Runs of 2026-10-20 (`G=/Users/max/Projects/cm0102/clean-3968`, `D=/Users/max/Projects/cm0102/test-fixtures/databases`; each is `python3 tests/seasons/rules_report_check.py --run build/cm0102_recomp $G` with the arguments listed). Each passes against its expected file; the lines of the new divisions that stay are Q521 (direct places in place of play-offs), Q526 (the Greek group at 14) and the group classes' table count (one group in the report's `clubs`).

| Arguments | Result |
|---|---|
| `--year 2008 --database "$D/September 2008" --nations germany --extra regional --start-only` | 3. Liga, 20 clubs, 38 dates; club list check passes |
| `--year 2008 --last 2009 ...` (the same, two seasons) | 3 down, 3 new from the pool, 20 clubs each season |
| `--year 2008 --last 2009 --database "$D/September 2008" --nations germany` | no flag: Regionalliga pool of 36, no box (ger_first.cpp 1049 gone) |
| `--year 2007 --last 2008 --database "$D/November 2007" --nations germany --extra regional --changing-rules on` | Regionalliga 2007/08, then the 3. Liga of 20 (8 down, 8 cut); before the stage fix: league.cpp 3214 on 27 July 2007, also on main |
| `--year 2013 --last 2014 --database "$D/Autumn 2013" --nations greece` | the second group (14) plays both seasons |
| `--year 2014 --last 2016 --database "$D/October 2014" --nations greece --changing-rules on` | the group ends in 2015 (13 to the Lower Division) and waits for 2020 |
| `--year 2021 --last 2022 --database "$D/October 2021" --nations greece` | the group (Super League 2 South) plays 2021/22 and 2022/23 |
| `--year 2021 --database "$D/October 2021" --nations all,-<the 15 rows but Greece> --extra spain,portugal --start-only` | Primera División RFEF 2 x 20, Liga 3 2 x 12; club lists pass (one Portuguese stand-in) |
| the same with `--last 2022` | Spain 10 down and 10 new (40), Portugal 4 and 4 (24) |
| `--year 2020 --last 2021 --database "$D/November 2020" --nations all,-<the 16 rows> --extra spain,portugal --changing-rules on` | July 2021: Portugal 60 to 24 (12 down, 24 cut), Spain 80 to 40 (17 down, 23 cut) |

Same-save (Q342), `python3 tests/smoke/smoke_run.py build/cm0102_recomp $G --same-save --runs normal,again,new-divisions-off,season-code-off --jobs 2` with `--save start.sav`, `--save prematch.sav`, a new game and `--save yeovil.sav --days 7`: PASS (each run equal to normal). Repeatability: `python3 tests/seasons/season_core_run.py build/cm0102_recomp $G --year 2008 --database "$D/September 2008" --only season` and the same for 2021 (October 2021): PASS (season-again equal to season). `ctest`: 34 of 34.

Not slice 20's, found here: a 2020 start with Greece and changing rules on (November 2020) stops at the June 2021 change with "free of 0x705a85c0, which is not a heap block (from 0x945330)", also with `RECOMP_NEW_DIVISIONS=off`.
