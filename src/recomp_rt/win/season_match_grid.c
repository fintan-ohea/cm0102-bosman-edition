/* The match grid guard: MATCH_PLAYER::move_to_grid_cell 0x6f46b0, thiscall (x, y), ret 8. A player at a cell
 * (+0x102 x, +0x103 y, slot +0x28 >= 0) leaves it: the cell of the engine (+0x19e) at 0x215e + (x * 12 + y) * 90
 * counts the players of each side (+0x27) at +0x58. With a count of 0 the original shows match_pl.cpp 4885
 * (0x6f4731..0x6f4786) and returns without the move. Gate (b) met it once (2021 start, 14 October 2023, after
 * the July 2023 load); three replays of that save and 20 short games with five substitutes forced in every match
 * (about 81,000 matches, RECOMP_TEST_SUBS_USED below) gave no such cell. No 0.6.0 code moves a player on the
 * grid: the engine makes every substitution itself, and the season rules only set the counts, which init_match
 * caps at 9. In a season game the guard logs the match and moves the player as one in no cell (slot -1, the
 * original's own path at 0x6f4819), so the counts stay right and no modal box stops the game. */

#include "win.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void o_006f46b0(void);

static void name_at(uint32_t at, char *out)
{
    out[0] = 0;
    if (at >= 0x10000u && at < 0xfff00000u) memcpy(out, g_mem + at + 4, 51), out[51] = 0;
}

RT_WRAP(006f46b0, match_grid_guard)
{
    uint32_t pl = cpu.ecx;
    if ((int8_t)g_mem[pl + 0x28] > -1 && season_code_on() &&
        (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules())) {
        int x = (int8_t)g_mem[pl + 0x102], y = (int8_t)g_mem[pl + 0x103], side = (int8_t)g_mem[pl + 0x27];
        uint32_t engine = rd32(g_mem, pl + 0x19e);
        uint32_t cell = engine + 0x215eu + (uint32_t)((x * 12 + y) * 90);
        if (!g_mem[cell + (uint32_t)side + 0x58]) {
            uint32_t fixture = rd32(g_mem, engine + 0x4792);
            char comp[52], home[52], away[52];
            name_at(rd32(g_mem, fixture + 0x14), comp);
            name_at(rd32(g_mem, fixture + 0x1c), home);
            name_at(rd32(g_mem, fixture + 0x20), away);
            fprintf(stderr, "recomp: match grid: %s, %s v %s, word +0x4d %04x: a player of side %d (slot %d) leaves "
                            "cell %d,%d (to %d,%d) whose count is 0; player bytes", comp, home, away,
                    rd16(g_mem, fixture + 0x4d), side, (int8_t)g_mem[pl + 0x28], x, y, (int8_t)rt_arg(0),
                    (int8_t)rt_arg(1));
            for (uint32_t i = 0; i < 0x30; i += 4) fprintf(stderr, " %08x", rd32(g_mem, pl + i));
            fputc('\n', stderr);
            g_mem[pl + 0x28] = 0xff;           /* in no cell: the original adds him to the new cell */
        }
    }
    o_006f46b0();
}

/* Test switch RECOMP_TEST_SUBS_USED=n (1 to 9): CM3_MATCH_ENGINE::init_match 0x6b7f20, thiscall (fixture, ...),
 * sets the substitutes used of every match (the high nibble of fixture +0x4d, read and capped at 9 at
 * 0x6b8006..0x6b802a) before the original, for the grid test of match_pl.cpp 4885. With RECOMP_CLUB_LOG the
 * program prints the count of matches at exit. */
void o_006b7f20(void);

static int matches;

static void matches_at_exit(void) { fprintf(stderr, "recomp: match grid: %d matches started\n", matches); }

RT_WRAP(006b7f20, match_subs_test)
{
    static int used = -1;
    if (used < 0) {
        const char *v = getenv("RECOMP_TEST_SUBS_USED");
        used = v ? atoi(v) : 0;
        if (getenv("RECOMP_CLUB_LOG")) atexit(matches_at_exit);
    }
    uint32_t fixture = rt_arg(0);
    matches++;
    if (used > 0 && used <= 9 && fixture >= 0x10000u)
        wr16(g_mem, fixture + 0x4d, (uint16_t)((rd16(g_mem, fixture + 0x4d) & 0xff0fu) | (uint32_t)(used << 4)));
    o_006b7f20();
}
