"""A fix for an error box of the original's league load (league_stage.cpp 533; src/recomp_rt/win/league_load.c).

League_Stage::load_from_file 0x68b720 reads the result array of each table row: +0xa7 bytes (the matches per club,
(clubs - 1) x meetings when +0xa7 is -1, 0x68c827..0x68c83c) into the block at row +9 (0x68c859..0x68c86f), with
fread 0x945bfe (size +0xa7, count 1, 0x68c881..0x68c88d), and stops at the box when fread does not give 1
(cmp eax, 1 at 0x68c895, jne 0x68c8df at 0x68c898). A league or stage of one club has 0 matches per club, so the
read of 0 bytes gives 0 and the load stops, though the save wrote that row's 0 bytes (0x6897af..0x6897c4). The
original never makes a league of one club; slice 28's CONCACAF Nations League did (a 2022 save of a long game).
rt_league_row_read_failed gives 0 for a read of 0 bytes, so the load goes on; else 1, the original's jump.
"""

INSNS = {
    0x68C898: ("jne 0x68c8df", "if (!cc_e(F) && rt_league_row_read_failed(esi)) goto L_68c8df;"),
}
