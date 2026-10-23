/* The news item of a national squad call-up (type 29) with a player who is in the squad more than once.
 * Research: docs/research/2026-10-18-staff-reserve-boxes.md.
 *
 * NationalTeams::select_squad 0x76c120 sends one item for each club run of the new squad (0x76e0b0, called
 * at 0x76d3dc) before national_teams_remove_duplicate_squad_entries 0x76c060 takes the duplicates out
 * (0x76c02f, 0x76c03d). The item holds the count of players in slot 4 and four slots for each player from
 * slot 10, the staff id first (0x76e2ce..0x76e324). The text writer 0x76e3b0 reads the groups from slot 10
 * and skips a group whose staff id it already has, without a step of its counter (0x76e4c5..0x76e4e8). So
 * with a player listed three times it reads past the groups that the item holds, the empty slots all hold
 * 0, and it reads on until NewsItem::get_slot 0x785830 gets slot 50 and shows the box "news..cpp 7797".
 *
 * The wrap below runs the reader's loop on the slots first. Only when that loop would reach slot 50 does
 * the reader get, for this call, the count of different players among the groups of the item. Every other
 * item reads as in the original. The item itself does not change, so the saved game holds the same bytes. The
 * test switch RECOMP_BOX_FIXES=off turns the wrap off (tests/seasons/season_boxes_run.py).
 *
 * The cause. select_squad takes the squad from match_man_assign_players_to_positions 0x6e2000 (called at
 * 0x76d047) and puts each pick into the first free slot of the squad (club +0xd7, 0x76d09d..0x76d0b3) with no
 * test for a player already there: with a short pool the pick gives one player for several places. The original
 * takes the repeats out with national_teams_remove_duplicate_squad_entries 0x76c060 after select_squad (0x76c02f,
 * 0x76c03d in select_squads_for_fixture 0x76bab0), but select_squad sends the news before (0x76d3dc). In a season
 * game the wrap of the news 0x76e0b0 below runs 0x76c060 first, for the call of select_squad, so the item never
 * holds a repeat; the reader wrap above stays as a logged net. The base game keeps the original's order (Q152). */
#include "win.h"
#include "season.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT(item, k) ((item) + 5u + 4u * (uint32_t)(k))   /* NewsItem::get_slot 0x785845 */
#define SLOTS 50                                           /* 0x78583e */
#define COUNT_SLOT 4                                        /* 0x76e24a, read back at 0x76e400 */
#define FIRST_GROUP 10                                      /* 0x76e2c4, 0x76e422 */
#define GROUP 4                                             /* four slots for each player */
#define MAX_COUNT 3                                         /* the groups are read only for 1 to 3 (0x76e418) */

void o_0076e3b0(void);

/* 1 when the reader of 0x76e3b0 would read a slot from 50 on before it has count different players. */
static int reader_overrun(uint32_t item, int count)
{
    uint32_t seen[SLOTS];
    int got = 0;
    for (int at = FIRST_GROUP; got < count; at += GROUP) {
        if (at + GROUP > SLOTS) return 1;
        uint32_t id = rd32(g_mem, SLOT(item, at));
        int dup = 0;
        for (int k = 0; k < got; k++) dup |= seen[k] == id;
        if (!dup) seen[got++] = id;
    }
    return 0;
}

RT_WRAP(0076e3b0, write_call_up_news_text)
{
    uint32_t item = rt_arg(0), count_at = SLOT(item, COUNT_SLOT), count_word = rd32(g_mem, count_at);
    int count = (int8_t)count_word;
    const char *off = getenv("RECOMP_BOX_FIXES");
    if (count < 1 || count > MAX_COUNT || (off && !strcmp(off, "off")) || !reader_overrun(item, count)) {
        o_0076e3b0();
        return;
    }
    uint32_t ids[MAX_COUNT];
    int distinct = 0;
    for (int g = 0; g < count; g++) {
        uint32_t id = rd32(g_mem, SLOT(item, FIRST_GROUP + GROUP * g)), dup = 0;
        for (int k = 0; k < distinct; k++) dup |= ids[k] == id;
        if (!dup) ids[distinct++] = id;
    }
    if (getenv("RECOMP_SCREEN_LOG"))
        fprintf(stderr, "recomp: call-up news lists a player more than once: %d groups, %d players\n", count,
                distinct);
    wr32(g_mem, count_at, (count_word & ~0xffu) | (uint32_t)distinct);
    o_0076e3b0();
    /* When the call unwinds (rt_unwinding, rt.h: a longjmp of the game ran an older frame), the item may be
     * gone, so the old count is not written back. The item then keeps the count of different players, which
     * names each player once. */
    if (!rt_unwinding) wr32(g_mem, count_at, count_word);
}

#define REMOVE_REPEATS 0x0076c060u     /* thiscall (nation), ret 4: the squads of club +0xd7 of the nation */
#define NEWS_FROM_SELECT 0x0076d3e1u   /* the return address of the news call in select_squad (0x76d3dc) */
#define CLUB_NATION 0x53u

void o_0076e0b0(void);
void o_006e2000(void);

#define SQUAD 0xd7u                     /* 50 player pointers of a national side (0x76d09d) */
#define PICK_FROM_SELECT 0x0076d04cu    /* the return address of the pick call in select_squad (0x76d047) */

/* The repeats in the squad of a national side: the slots whose player is in an earlier slot too. */
static int squad_repeats(uint32_t side)
{
    int n = 0;
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t p = rd32(g_mem, side + SQUAD + 4 * i);
        for (uint32_t k = 0; p && k < i; k++)
            if (rd32(g_mem, side + SQUAD + 4 * k) == p) { n++; break; }
    }
    return n;
}

/* Test switch RECOMP_TEST_REPEAT_PICK=1 (O260 A15 retest): match_man_assign_players_to_positions 0x6e2000, cdecl
 * (players, count, position masks, places, subs, out, ...), called from select_squad, gives a pick of the human
 * manager's club for two more places, the repeat that a short pool makes. Without the switch it is the original. */
RT_WRAP(006e2000, pick_players)
{
    static int on = -1;
    if (on < 0) { const char *v = getenv("RECOMP_TEST_REPEAT_PICK"); on = v && *v == '1'; }
    uint32_t from = rd32(g_mem, cpu.esp), out = rt_arg(5), places = rt_arg(3) & 0xffu;
    o_006e2000();
    if (rt_unwinding || !on || from != PICK_FROM_SELECT || places < 3) return;
    /* A player of the human manager's club (the manager of the screen's player slot, 0xb74c26 + slot * 0x300 with
     * the slot at 0xb77c7a; club +0x39, else +0x24 as holiday_next.c), so that the item reaches the text writer. */
    uint32_t manager = rd32(g_mem, 0xb74c26u + (uint32_t)(int16_t)rd16(g_mem, 0xb77c7au) * 0x300u);
    if (!manager) return;
    uint32_t club = rd32(g_mem, manager + 0x39u) ? rd32(g_mem, manager + 0x39u) : rd32(g_mem, manager + 0x24u);
    for (uint32_t i = 0; club && i < places; i++) {
        uint32_t p = rd32(g_mem, out + 4 * i);
        if (!p || rd32(g_mem, p + 0x39u) != club) continue;
        for (uint32_t k = 0, put = 0; k < places && put < 2; k++)
            if (k != i) { wr32(g_mem, out + 4 * k, p); put++; }
        fprintf(stderr, "recomp: test: the squad pick repeats a player of the human club three times\n");
        return;
    }
}

/* national_teams_post_call_up_news 0x76e0b0, stdcall (national side, b squad, fixture, date, finals, opponents,
 * count): in a season game, from select_squad, the repeats of the nation's squads go out before the news. */
RT_WRAP(0076e0b0, post_call_up_news)
{
    uint32_t side = rt_arg(0);
    int season = season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
    if (season && rd32(g_mem, cpu.esp) == NEWS_FROM_SELECT && side && rd32(g_mem, side + CLUB_NATION)) {
        int before = squad_repeats(side);
        Cpu saved = cpu;
        uint32_t a[1] = {rd32(g_mem, side + CLUB_NATION)};
        rt_callback(REMOVE_REPEATS, 1, a);
        if (rt_unwinding) return;
        cpu = saved;
        if (before && getenv("RECOMP_SCREEN_LOG"))
            fprintf(stderr, "recomp: call-up squad: %d repeats taken out before the news, %d left\n", before,
                    squad_repeats(side));
    }
    o_0076e0b0();
}
