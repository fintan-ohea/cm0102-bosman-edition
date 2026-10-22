/* The French CFA background league in a season game (fix of "virtual_staff..cpp 195", 2025/26).
 *
 * FraCfa::FraCfa 0x5b91c0 collects the clubs of its four groups of 18 in a stack array of 72 (0x48, 0x5b92b6,
 * 0x5b9300): every club whose division is the record of key [0x9cf5cc] or [0x9cf5d0] (0x5b92a0..0x5b92ba), and the
 * clubs of A Lower Division [0x9cf764] whose zone record (+0x60) is the CFA [0x9cf5d8], at most 34 of them
 * (0x5b92e8). It shuffles the array (0x4afb40 at 0x5b9329) and makes the groups (0x5b9fa0, 0x5b933b..0x5b9362)
 * without clearing it, so a short array leaves stack values in the last group: 38 + 34 = 72 in the data of
 * 2001/02. With both divisions at 18 (2024/25 on, slice 18's league sizes) 70 came in, the last group held 0xee and
 * 0x944e68 as clubs, and its first reserve fixture stopped at virtual_staff.cpp 195 (0x90d4c1). In a season game
 * the cap of the lower clubs is 72 less the clubs of the two divisions, so the array fills when the CFA zone holds
 * enough clubs. The base game keeps 34 (Q152). */
#include "win.h"
#include "season_names.h"

#include <stdio.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u
#define KEY_A 0x9cf5ccu
#define KEY_B 0x9cf5d0u
#define GROUPS_CLUBS 0x48

uint32_t rt_fra_cfa_lower_cap(uint32_t original)
{
    if (!season_game_changes()) return original;
    uint32_t base = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATION_COUNT);
    int32_t a = (int32_t)rd32(g_mem, KEY_A), b = (int32_t)rd32(g_mem, KEY_B);
    int upper = 0;
    for (int32_t i = 0; i < count; i++) {
        uint32_t d = rd32(g_mem, base + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION);
        if (!d) continue;
        int32_t id = (int32_t)rd32(g_mem, d);
        upper += id == a || id == b;
    }
    int cap = GROUPS_CLUBS - upper;
    if (cap < 0) cap = 0;
    static int said;
    if ((uint32_t)cap != original && !said++)
        fprintf(stderr, "recomp: french cfa: %d clubs of the two upper divisions, so up to %d lower clubs (the "
                        "original takes %u)\n", upper, cap, original);
    return (uint32_t)cap;
}
