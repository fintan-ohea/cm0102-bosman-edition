/* Domestic cups with a fixed number of entrants (0.6.0 gate fix O151). Many cup classes set their count (+0x3e)
 * to a constant and fill the list (+0xa7, 6 bytes a club) from the divisions of the original's data, for example
 * PolCup::collect_entrants 0x7c8550: 52 places (0x7c8569) from the Second Division clubs by their last place
 * (0x7c86d7..0x7c871a, 0x7c8759..0x7c87ae), Lower Division clubs up to 36 places (0x7c87b4..0x7c880a) and the top
 * division (0x7c8837..). With the divisions of another season (slice 15's club moves) a pool can be shorter, so a
 * place stays empty: in a 2003/04 game the Polish Cup drew a tie with away club 0xffff and the match engine
 * stopped at match_eng..cpp 652 (gate (a) run 6).
 *
 * Before Cup::map_fixture_tree 0x518790 maps the list into ties (its one wrap is in season_uefa_qual.c), each
 * empty place of a domestic cup gets a club of the cup's nation that is not in the list yet: from the divisions
 * at or below the lowest division of the clubs in the list (record +0x69, the level), by level, then by
 * reputation (+0x80). So the cup keeps its count and the class's round table. A domestic cup is one whose record
 * names a nation (+0x5d) and whose entrants are all of that nation; the UEFA cups, whose empty places take clubs
 * later (season_uefa_qual.c put_club), are not. It writes a log line for each cup it fills. The base game
 * (2001/02, changing rules off) and RECOMP_SEASON_CODE=off get none of it (Q152, Q342). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_cups.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>

#define CUP_TEAMS 0x3eu
#define CUP_TEAM_LIST 0xa7u
#define ENTRY 6u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_COUNT_LESS 0xae2350u      /* the clubs are [0xae235c] - 2 * [0xae2350] (as 0x7c85ef..0x7c8606) */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define RECORD_NATION 0x5du
#define RECORD_LEVEL 0x69u
#define MAX_LIST 512

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int nation_id(uint32_t club)
{
    uint32_t n = rd32(g_mem, club + CLUB_NATION);
    return readable(n) ? (int32_t)rd32(g_mem, n) : -1;
}

static int level_of(uint32_t club)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) ? (int16_t)rd16(g_mem, d + RECORD_LEVEL) : -1000;
}

static int better(uint32_t a, uint32_t b)
{
    int la = level_of(a), lb = level_of(b);
    if (la != lb) return la > lb;
    return (int16_t)rd16(g_mem, a + CLUB_REPUTATION) > (int16_t)rd16(g_mem, b + CLUB_REPUTATION);
}

void season_cup_fill_entrants(uint32_t cup)
{
    if (!readable(cup) || !season_game_changes()) return;
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST), rec = rd32(g_mem, cup + 4);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    if (!readable(list) || !readable(rec) || count < 2 || count > MAX_LIST) return;
    int nation = (int32_t)rd32(g_mem, rec + RECORD_NATION), empty = 0, lowest = 1000000;
    if (nation < 0) return;
    for (int i = 0; i < count; i++) {
        uint32_t club = rd32(g_mem, list + ENTRY * (uint32_t)i);
        if (!readable(club)) {
            empty++;
            continue;
        }
        if (nation_id(club) != nation) return;           /* not a cup of one nation */
        int l = level_of(club);
        if (l < lowest) lowest = l;
    }
    if (!empty || empty == count) return;
    uint32_t table = rd32(g_mem, CLUBS);
    int clubs = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, CLUB_COUNT_LESS), filled = 0;
    for (int i = 0; i < count && readable(table); i++) {
        uint32_t at = list + ENTRY * (uint32_t)i;
        if (readable(rd32(g_mem, at))) continue;
        uint32_t best = 0;
        for (int k = 0; k < clubs && k < 200000; k++) {
            uint32_t c = table + CLUB_SIZE * (uint32_t)k;
            if (nation_id(c) != nation || !readable(rd32(g_mem, c + CLUB_DIVISION)) || level_of(c) > lowest) continue;
            int in = 0;
            for (int j = 0; j < count && !in; j++) in = rd32(g_mem, list + ENTRY * (uint32_t)j) == c;
            if (!in && (!best || better(c, best))) best = c;
        }
        if (!best) break;
        wr32(g_mem, at, best);
        g_mem[at + 4] = 3;                                /* as the original's last pool (0x7c87cd..0x7c87f4) */
        g_mem[at + 5] = 0;
        filled++;
    }
    fprintf(stderr, "recomp: cup entrants: %s (%.40s) %d: %d of %d places empty, %d filled\n", season_change_file(cup),
            win_str(rec + 4), (int16_t)rd16(g_mem, cup + 0x40), empty, count, filled);
}
