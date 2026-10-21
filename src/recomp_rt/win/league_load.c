/* A fix for the error box "league_stage.cpp 533" of a league load (tools/recomp/league_load.py).
 *
 * League_Stage::load_from_file 0x68b720 reads the result array of each table row, +0xa7 bytes (the matches per club,
 * 0x68c827..0x68c83c), with fread 0x945bfe at 0x68c88d, and stops at the box when fread does not give 1 (0x68c895,
 * 0x68c898). A league of one club has 0 matches per club: the save writes 0 bytes for its row (0x6897af..0x6897c4),
 * and the read of 0 bytes gives 0. So for a read of 0 bytes the load goes on. The original never makes such a
 * league, so every other load takes the original's jump. The test switch RECOMP_BOX_FIXES=off turns the fix off
 * (as call_up_news.c and euro_finals.c). */
#include "win.h"
#include "../rt.h"

#include <stdlib.h>
#include <string.h>

uint32_t rt_league_row_read_failed(uint32_t league)
{
    const char *off = getenv("RECOMP_BOX_FIXES");
    if (off && !strcmp(off, "off")) return 1;
    return (int16_t)rd16(g_mem, league + 0xa7u) != 0;
}
