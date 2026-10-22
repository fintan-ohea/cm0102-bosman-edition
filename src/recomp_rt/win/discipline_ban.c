/* A fix for the error box "discipline..cpp 1149" of a ban release (tools/recomp/discipline_ban.py).
 *
 * Discipline::release_ban 0x556d00 removes a player's international ban from the nation index (this+0xc, count
 * this+0x12, entries of 8 bytes: staff id, nation key) with bsearch under his first nation (+0x1a) and then his
 * second nation (+0x1e). The entry holds his first nation at the time of the ban (0x556c10..0x556c43), so when his
 * first nation changed in between, neither search finds it and the original stops at the box (0x556dcf). Here the
 * entry is found by the staff id alone, and the release removes it as for a found entry (0x556e34). A release
 * that finds the entry under his nations never comes here, so it takes the original's path. The test switch
 * RECOMP_BOX_FIXES=off turns the fix off (as league_load.c). */
#include "win.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAFF_TABLE 0xae23bcu   /* g_staff_table, 0x6e bytes a row */
#define STAFF_ROW 0x6eu

static int32_t nation_key(uint32_t row, uint32_t field)
{
    uint32_t nation = rd32(g_mem, row + field);
    return nation >= 0x10000u && nation < 0xfff00000u ? (int32_t)rd32(g_mem, nation) : -1;
}

uint32_t rt_discipline_ban_entry(uint32_t discipline, uint32_t staff)
{
    const char *off = getenv("RECOMP_BOX_FIXES");
    if (off && !strcmp(off, "off")) return 1;
    uint32_t index = rd32(g_mem, discipline + 0xcu), row = rd32(g_mem, STAFF_TABLE) + staff * STAFF_ROW;
    int count = (int16_t)rd16(g_mem, discipline + 0x12u);
    for (int i = 0; i < count; i++) {
        uint32_t e = index + 8u * (uint32_t)i;
        if (rd32(g_mem, e) != staff) continue;
        fprintf(stderr, "recomp: discipline: the international ban of staff %u was under nation %d, his nations are "
                        "%d and %d: removed\n", staff, (int32_t)rd32(g_mem, e + 4u), nation_key(row, 0x1au),
                nation_key(row, 0x1eu));
        return e;
    }
    fprintf(stderr, "recomp: discipline: the international ban of staff %u has no nation index entry (nations %d "
                    "and %d)\n", staff, nation_key(row, 0x1au), nation_key(row, 0x1eu));
    return 0;
}
