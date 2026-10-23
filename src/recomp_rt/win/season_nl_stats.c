/* National team stats in the club records (0.6.0 slice 28 fix: club_records.cpp 794). Evidence below; research
 * doc docs/research/2026-10-18-nations-league.md, section "National team stats in the club records". */
#include "win.h"
#include "season_comps.h"
#include "season_nl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_NL_LOG");
        on = v ? atoi(v) : 0;
    }
    return on;
}

void o_007c3920(void);

/* PlayerStats::get_category_record, thiscall (Player *player, char category), ret 8, returns the player's 32-byte
 * stats record of a category or 0 (0x7c3920..0x7c39b5). Club_Records::update_records_for_match 0x444930 asks it
 * for each player of a national team with category 5 (0x445f7b..0x445f96) and stops at the box "club_records.cpp
 * 794" (0x44602d) when the player has none. A player has a category 5 record only when his national team called
 * him up. The 2022 Finalissima (Argentina, with no other match in that window) and other matches of slice 28's
 * competitions can field players with none: box in a 2021 start with Greece (October 2021), text "No player stats
 * for Argentina playing in 2022 Finalissima". The record is only read there (player_stats_get_record_value
 * 0x7c2150 at 0x44653f, 0x446690, 0x4467f7), so in a season game this one call gets an empty record of its own
 * (no matches, so the default rating) in place of 0. Every other call and the base game keep the original's
 * answer (Q152). */
RT_WRAP(007c3920, club_records_national_stats)
{
    uint32_t ret = rd32(g_mem, cpu.esp), player = rt_arg(0);
    int category = (int8_t)rd8(g_mem, cpu.esp + 8u);
    o_007c3920();
    if (rt_unwinding || cpu.eax || ret != 0x00445f9bu || category != 5 || !season_comps_on()) return;
    static uint32_t empty;
    if (!empty) empty = win_static_alloc(0x20u);
    if (!empty) return;
    memset(g_mem + empty, 0, 0x20u);
    cpu.eax = empty;
    if (logging()) {
        /* The staff row of the player (staff +0x61 is his player row), his nation and club, and whether the
         * national team of his nation holds him in its squad slots (+0xd7). */
        uint32_t staff = rd32(g_mem, 0xae23bcu), n = rd32(g_mem, 0xae2364u), sp = 0;
        for (uint32_t i = 0; staff >= 0x10000u && i < n && !sp; i++)
            if (rd32(g_mem, staff + i * 0x6eu + 0x61u) == player) sp = staff + i * 0x6eu;
        uint32_t nation = sp ? rd32(g_mem, sp + 0x1a) : 0, club = sp ? rd32(g_mem, sp + 0x39) : 0;
        fprintf(stderr, "recomp: season nl: a national team player with no stats record gets an empty one (staff %08x, "
                        "nation %.30s, club %.30s)\n", sp, nation >= 0x10000u ? (char *)(g_mem + nation + 4) : "",
                club >= 0x10000u ? (char *)(g_mem + club + 4) : "");
    }
}

/* Test log (RECOMP_SEASON_NL_LOG=1) for the cause of the 794 box: comp_active_key_nation_of_fixture 0x48dd70,
 * cdecl (record, fixture, fixture, 0x4f), gives the fixture type when a fixture goes into the store (call at
 * 0x5ab2e3). Only types 0 and 1 get the call-up two weeks before (NationalTeams::daily_update 0x76b207,
 * 0x76b221). The line names the type of each Gold Cup and Finalissima fixture. It only reads. */
int season_gold_cup_record(uint32_t rec);
void o_0048dd70(void);

RT_WRAP(0048dd70, nl_fixture_type_log)
{
    uint32_t rec = rt_arg(0), fixture = rt_arg(1);
    o_0048dd70();
    if (rt_unwinding || !logging() || !rec || !season_gold_cup_record(rec) || fixture < 0x10000u) return;
    uint32_t home = rd32(g_mem, fixture + 0x1c), away = rd32(g_mem, fixture + 0x20);
    fprintf(stderr, "recomp: season nl: fixture of %.50s, %.50s v %.50s, gets type %d\n", (char *)(g_mem + rec + 4),
            home >= 0x10000u ? (char *)(g_mem + home + 4) : "", away >= 0x10000u ? (char *)(g_mem + away + 4) : "",
            (int8_t)cpu.eax);
}

/* Test log (RECOMP_SEASON_NL_LOG=1) for O322: a player who gets his first national team stats record
 * (category 5) in a Finalissima match. PlayerStats::record_match 0x7c0d70, thiscall (fixture, ...), ret 8, finds
 * the competition at [0xadadfc][fixture +0] (0x7c0ee5..0x7c0eef) and calls create_stats 0x7c5d80, thiscall
 * (player, category), ret 8, for a player with no record of the category (0x7c10e6..0x7c10f9). Both only log. */
void o_007c0d70(void);
void o_007c5d80(void);
static int in_finalissima;

RT_WRAP(007c0d70, nl_record_match_log)
{
    uint32_t fixture = rt_arg(0), comp = 0;
    if (logging() && fixture >= 0x10000u) {
        int32_t id = (int32_t)rd32(g_mem, fixture);
        uint32_t table = rd32(g_mem, 0xadadfcu);
        if (id >= 0 && table >= 0x10000u) comp = rd32(g_mem, table + 4u * (uint32_t)id);
    }
    int was = in_finalissima;
    in_finalissima = comp >= 0x10000u && season_finalissima_object(comp);
    o_007c0d70();
    in_finalissima = was;
}

RT_WRAP(007c5d80, nl_create_stats_log)
{
    uint32_t player = rt_arg(0);
    int category = (int8_t)rd8(g_mem, cpu.esp + 8u);
    if (in_finalissima && category == 5) {
        uint32_t staff = rd32(g_mem, 0xae23bcu), n = rd32(g_mem, 0xae2364u), sp = 0;
        for (uint32_t i = 0; staff >= 0x10000u && i < n && !sp; i++)
            if (rd32(g_mem, staff + i * 0x6eu + 0x61u) == player) sp = staff + i * 0x6eu;
        uint32_t nation = sp ? rd32(g_mem, sp + 0x1a) : 0;
        fprintf(stderr, "recomp: season nl: finalissima: a player of %.30s gets his first national team stats record\n",
                nation >= 0x10000u ? (char *)(g_mem + nation + 4) : "?");
    }
    o_007c5d80();
}
