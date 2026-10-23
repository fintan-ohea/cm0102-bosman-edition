/* The MLS with clubs that the original does not know (0.6.0 slice 18).
 *
 * The UsaMls class plays three groups of 4 clubs (UsaMls::create_table 0x908560 sets 4 at 0x908572 and takes the
 * clubs of group 0x413, UsaMls::create_second_division_stage 0x908fc0 takes 0x47b, 0x9091e0 takes 0x414). The
 * group of a club comes from usa_mls_get_club_division 0x909ec0, which knows the 12 MLS clubs of the original by
 * their key ids (0x9d037c..0x9d03a8, compares at 0x909ee5..0x909f6b) and stops at usa_mls.cpp 1819 (push 0x71b
 * at 0x909f96, box 0x5e8290 at 0x909fbe) for any other club whose division is the American Major League (key id
 * [0x9cf590], compare at 0x909edd). usa_mls_get_club_all_star_conference 0x90a000 does the same for the two
 * All-Star conferences (0x413 and 0x414, stop at usa_mls.cpp 1859, push 0x743 at 0x90a0d6).
 *
 * The club moves of a season game (season_clubs.c) put the real MLS clubs of the season into the division, up to
 * the 12 that the class takes. So the runtime gives each club that the original does not know a free place:
 * - a club of another division, or a known club, goes to the original (the same result in every game);
 * - an unknown club of the MLS takes the group with the fewest clubs that has fewer than 4 (ties: 0x413, 0x47b,
 *   0x414), counting the clubs of the division in the order of the club table, the known clubs first with their
 *   own group. So every group holds 4 clubs when the division holds 12. A club after the 12th gets 0, the value
 *   of a club that is not in the MLS: no group takes it;
 * - its All-Star conference: 0x413 for group 0x413, 0x414 for group 0x414, and for group 0x47b the conference
 *   with fewer clubs (the original splits group 0x47b 2 and 2).
 * The base game holds only the 12 known clubs, so it never reaches the new path. */
#include "win.h"
#include "season.h"

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u         /* club_comp pointer, its +0 is the record id */
#define KEY_MLS 0x9cf590u           /* key id of the American Major League (0x909ed7) */

#define GROUP_A 0x413u
#define GROUP_B 0x47bu
#define GROUP_C 0x414u

/* The key ids of the 12 clubs and their group (0x909ee5..0x909f6b) and conference (0x90a025..0x90a0ab). */
static const struct { uint32_t key; uint16_t group, conference; } KNOWN[12] = {
    {0x9d0390u, GROUP_A, GROUP_A}, {0x9d0394u, GROUP_A, GROUP_A}, {0x9d0384u, GROUP_A, GROUP_A},
    {0x9d0380u, GROUP_A, GROUP_A}, {0x9d03a4u, GROUP_B, GROUP_A}, {0x9d0388u, GROUP_B, GROUP_A},
    {0x9d03a0u, GROUP_B, GROUP_C}, {0x9d037cu, GROUP_B, GROUP_C}, {0x9d03a8u, GROUP_C, GROUP_C},
    {0x9d039cu, GROUP_C, GROUP_C}, {0x9d0398u, GROUP_C, GROUP_C}, {0x9d038cu, GROUP_C, GROUP_C},
};

static int in_mls(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + CLUB_DIVISION);
    return div && rd32(g_mem, div) == rd32(g_mem, KEY_MLS);
}

static int known(uint32_t club)
{
    uint32_t id = rd32(g_mem, club);
    for (int k = 0; k < 12; k++)
        if (rd32(g_mem, KNOWN[k].key) == id) return k;
    return -1;
}

/* The group and conference of an unknown club of the MLS: 0 when the three groups are full. */
static void place(uint32_t want, uint16_t *group, uint16_t *conference)
{
    static const uint16_t order[3] = {GROUP_A, GROUP_B, GROUP_C};
    int in_group[3] = {0, 0, 0}, east = 0, west = 0;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    *group = *conference = 0;
    for (int pass = 0; pass < 2; pass++)            /* the known clubs first, then the others in table order */
        for (int i = 0; i < count; i++) {
            uint32_t c = table + (uint32_t)i * CLUB_SIZE;
            if (!in_mls(c)) continue;
            int k = known(c);
            if ((pass == 0) != (k >= 0)) continue;
            uint16_t g = 0, conf = 0;
            if (k >= 0) {
                g = KNOWN[k].group;
                conf = KNOWN[k].conference;
            } else {
                int best = -1;
                for (int j = 0; j < 3; j++)
                    if (in_group[j] < 4 && (best < 0 || in_group[j] < in_group[best])) best = j;
                if (best >= 0) {
                    g = order[best];
                    conf = g == GROUP_A ? GROUP_A : g == GROUP_C ? GROUP_C : east <= west ? GROUP_A : GROUP_C;
                }
            }
            for (int j = 0; j < 3; j++) in_group[j] += g == order[j];
            east += conf == GROUP_A;
            west += conf == GROUP_C;
            if (c == want) {
                *group = g;
                *conference = conf;
                return;
            }
        }
}

/* 1 when the runtime answers for this club: an unknown club of the MLS, with the season code on. */
static int ours(uint32_t club)
{
    return club && season_code_on() && in_mls(club) && known(club) < 0;
}

void o_00909ec0(void);
void o_0090a000(void);

/* usa_mls_get_club_division 0x909ec0, cdecl (club), returns the group in ax. */
RT_WRAP(00909ec0, mls_club_group)
{
    uint32_t club = rt_arg(0);
    if (!ours(club)) {
        o_00909ec0();
        return;
    }
    uint16_t group, conference;
    place(club, &group, &conference);
    rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | group);
}

/* usa_mls_get_club_all_star_conference 0x90a000, cdecl (club), returns the conference in ax. */
RT_WRAP(0090a000, mls_club_conference)
{
    uint32_t club = rt_arg(0);
    if (!ours(club)) {
        o_0090a000();
        return;
    }
    uint16_t group, conference;
    place(club, &group, &conference);
    rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | conference);
}

/* The MLS play-off rounds in a season game (rule O284, row arg-usa-218). UsaMls::create_schedule 0x9072d0 writes
 * the play-off and final round records with flag 0x20 in the match flags (+0x0d 0x230 at 0x907fe7 and 0x908068,
 * +0x0f 0x30 at 0x908003 and 0x90808b, +0x11 0x20 at 0x908009 and 0x908091, the final +0x0d 0x220 at 0x9081c2;
 * cup round record doc, section 2). With 0x20, CM3_MATCH_ENGINE::period_completed 0x6bd930 starts the run-in
 * shootout (state 1 at 0x6bdb1c) in place of penalties (flag 0x1, state 4 at 0x6bdaf7). In it both sides can miss
 * every kick: Kansas City Wizards v Washington DC United (2021 start, October 2021 database, day 287 of 2023) was
 * still 0-0 after 127 kicks a side, the signed kick counter (+0x8e9f) wrapped, kick % 11 read before the taker
 * order (0x6be931..0x6be96a) and the taker became an object that is no player (match_pl.cpp 4885 at 0x6f4731). A
 * run with three substitutes forced met the same 0-0 run-in (San Jose Earthquakes v Atlanta United FC, 40 kicks).
 * MLS dropped the shootout for the 2000 season; from 2002/03 its play-offs go to extra time and then penalties
 * (row arg-usa-218). In a season game the flag becomes 0x3; the base game keeps 0x20 (Q152). The seasons.py sites
 * call this; the init_match wrap (season_match_grid.c) maps a fixture made before (an older save's table). */
uint32_t rt_mls_round_flags(uint32_t flags)
{
    if (!(flags & 0x20u) || !season_code_on() || (season_start_year() == SEASON_FIRST_YEAR && !season_changing_rules()))
        return flags;
    return (flags & ~0x20u) | 0x3u;
}
