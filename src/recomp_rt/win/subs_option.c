/* The options bench and subs_used (fork of fintan-ohea, options.h): the named substitutes and the substitutes used
 * of every match, in place of the counts of each competition.
 *
 * How the original holds the counts (docs/research/2026-10-17-season-rules-17.md, section 1): a fixture keeps them
 * in the word at +0x4d, the named substitutes (the bench) in bits 0 to 3 and the substitutes used in bits 4 to 7.
 * The competition packs them when it makes the fixture (Comp::set_fixture_substitutes 0x48f2d0 and the three
 * overrides that season_kind_subs.c wraps). The team screens read the bench as 11 plus the low nibble (0x459aa8,
 * 0x474e49, 0x8cf8cf), and CM3_MATCH_ENGINE::init_match 0x6b7f20 reads both and caps each at 9 (0x6b7feb,
 * 0x6b8006). So the option writes the two nibbles in three places:
 * - a new fixture, after the competition packs it (subs_option_fixture, from the wraps of season_kind_subs.c);
 * - the fixture of every match, before init_match reads it (season_match_grid.c), so a fixture made before the
 *   option was set, or held in a fixtures_<year>.tmp file of a later year, plays with the option's counts;
 * - the unplayed first-team fixtures of the human's club, once a game day and when the option changes
 *   (subs_option_turn, from the human turn of testrun.c), so the squad and tactics screens name the new bench
 *   before the match. FIXTURE_MANAGER::find_fixtures 0x5acbe0 finds them as holiday_next.c does; the search ends
 *   at the last year block that the store holds.
 * A value of off leaves the nibble as the competition made it. Same game (Q152): with both options off nothing is
 * read or written. Turning an option off again does not restore the fixtures it changed: they keep its count
 * until they are played, and new fixtures take the competition's count. */
#include "win.h"
#include "options.h"

#include <stdio.h>

#define WORD 0x4du                  /* fixture: u16, bench in bits 0..3, substitutes used in bits 4..7 */
#define RESULT 0x43u                /* fixture: s8, -1 or less while unplayed (0x454c0d) */
#define GAME_DAY 0xae2c90u          /* s16 day of the year from 0, s16 year */
#define FIXTURES 0xae2a58u          /* the fixture store; +0x1130 s16 year, +0x112c next year block */
#define FIXTURE_SEARCH 0x5acbe0u    /* thiscall FIXTURES, 9 args, returns the count; team flag 1 = first team */
#define LIST_FREE 0x68d040u         /* thiscall on the list: frees its nodes */
#define PLAYER_MANAGER 0xb74c26u    /* per player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* The counts of the options: 0 for off. */
static int bench(void)
{
    int i = options_enum(OPT_BENCH);
    return i > 0 ? i + 4 : 0;       /* off|5|6|7|8|9 */
}

static int used(void)
{
    int i = options_enum(OPT_SUBS_USED);
    return i > 0 ? i + 2 : 0;       /* off|3|4|5|6|7|8|9 */
}

int subs_option_on(void) { return bench() || used(); }

void subs_option_fixture(uint32_t fixture)
{
    int b = bench(), u = used();
    if ((!b && !u) || !readable(fixture)) return;
    uint32_t word = rd16(g_mem, fixture + WORD), new_word = word;
    if (b) new_word = (new_word & ~0xfu) | (uint32_t)b;
    if (u) new_word = (new_word & ~0xf0u) | ((uint32_t)u << 4);
    if (new_word != word) wr16(g_mem, fixture + WORD, (uint16_t)new_word);
}

/* The fixture of a match that starts (the init_match wrap, season_match_grid.c). Logs the first matches. */
void subs_option_match(uint32_t fixture)
{
    static int logged;
    if (!subs_option_on() || !readable(fixture)) return;
    subs_option_fixture(fixture);
    if (logged < 5) {
        logged++;
        fprintf(stderr, "recomp: subs option: match starts with word %04x\n", rd16(g_mem, fixture + WORD));
    }
}

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static int days_in(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 366 : 365; }

/* The unplayed first-team fixtures of a club, from today to the end of the last year block. */
static int club_fixtures(uint32_t club)
{
    int day = (int16_t)rd16(g_mem, GAME_DAY), year = (int16_t)rd16(g_mem, GAME_DAY + 2);
    uint32_t store = rd32(g_mem, FIXTURES);
    int last = -1;
    for (uint32_t b = store, n = 0; b && n < 100; b = rd32(g_mem, b + 0x112c), n++)
        if ((int16_t)rd16(g_mem, b + 0x1130) > last) last = (int16_t)rd16(g_mem, b + 0x1130);
    if (!store || last < year) return 0;
    uint32_t list = rt_alloc_zero(16);
    uint32_t args[9] = {(uint32_t)day, (uint32_t)year, (uint32_t)(days_in(last) - 1), (uint32_t)last,
                        rd32(g_mem, club), list, 0xffffffffu, 0xfffffffeu, 1};
    call_this(FIXTURE_SEARCH, store, 9, args);
    int count = 0;
    for (uint32_t node = rd32(g_mem, list + 2), n = 0; node && n < 2000; node = rd32(g_mem, node + 6), n++) {
        uint32_t f = rd32(g_mem, node);
        if (readable(f) && (int8_t)g_mem[f + RESULT] <= -1) {
            subs_option_fixture(f);
            count++;
        }
    }
    call_this(LIST_FREE, list, 0, NULL);
    rt_free(list);
    return count;
}

void subs_option_turn(void)
{
    static int done_day = -1, done_year, done_bench, done_used;
    static uint32_t done_club;
    int b = bench(), u = used();
    if (!b && !u) return;
    uint32_t manager = rd32(g_mem, PLAYER_MANAGER + (uint32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
    if (!readable(manager)) return;
    uint32_t club = rd32(g_mem, manager + 0x39);
    if (!readable(club)) return;
    int day = (int16_t)rd16(g_mem, GAME_DAY), year = (int16_t)rd16(g_mem, GAME_DAY + 2);
    if (day == done_day && year == done_year && club == done_club && b == done_bench && u == done_used) return;
    done_day = day, done_year = year, done_club = club, done_bench = b, done_used = u;
    int n = club_fixtures(club);
    fprintf(stderr, "recomp: subs option: bench %d, used %d: %d fixtures of club %d\n", b, u, n,
            (int32_t)rd32(g_mem, club));
}
