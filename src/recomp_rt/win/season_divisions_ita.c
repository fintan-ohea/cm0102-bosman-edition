/* Italy's single Serie C of three groups, 2014/15 on (season_divisions_ita.h, 0.6.0 slice 20b). Evidence:
 * docs/research/2026-10-20-ita-serie-c.md.
 *
 * The objects. create_italian_comps 0x835320 always makes Serie C1/A (ItaSerC1a, key id [0x9cf578]) and C1/B
 * ([0x9cf57c]), and C2/A, C2/B and C2/C ([0x9cf580], [0x9cf584], [0x9cf588]) only with the extra division flag
 * (bit 4 of nation +0x11c, 0x835407). In the Serie C era the groups A, B and C are C1/A, C1/B and C2/A (Q620).
 * Each class picks its clubs by the key id of its own record (C1a 0x64b71d, C1b 0x64f8cd, C2a 0x653b3d).
 *
 * The start. ItaSerA::start_new_season 0x641700 runs the chain (vtable +0xb0 = 0x642410, at 0x641713), then with
 * a C2/A object exchange_serie_d_clubs 0x6421c0 and regroup_serie_c2_clubs 0x642790, else
 * exchange_serie_c2_clubs_unplayed 0x641fc0, then regroup_serie_c1_clubs 0x642630 (0x641725..0x641743). It reads
 * C2/B and C2/C whenever C2/A exists (the chain 0x64250f, 0x642527; 0x6421e8..0x6421f5; the tier calls
 * 0x6418e0..0x641939; the starts 0x64198d..0x6419b7). In the Serie C era the wraps below play the season of the
 * three groups, and the two patched loads of the C2/A object (0x6418d5, 0x641986) keep C2/B and C2/C out. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_rules.h"
#include "season_names.h"
#include "season_divisions_ita.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_OBJECTS 0xadadfcu          /* g_comp_object_table */
#define COMP_RECORDS 0xae23d0u          /* club_comp records, 0x6b bytes each (as 0x6422ed..0x642306) */
#define COMP_RECORD_SIZE 0x6bu
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define NATION_COUNT 0xae2350u          /* the national teams: 2 rows each at the end of the club table */
#define KEY_A 0x9cf570u
#define KEY_B 0x9cf574u
#define KEY_C1A 0x9cf578u
#define KEY_C1B 0x9cf57cu
#define KEY_C2A 0x9cf580u
#define KEY_C2B 0x9cf584u
#define KEY_C2C 0x9cf588u
#define KEY_SERIE_D 0x9cf58cu
#define KEY_ITALY 0x9cf360u             /* the nation record of Italy (0x6422af) */
#define HUMANS 0xb7431cu                /* the manager list: 0x7875e0 (club) tells a human manager (0x6426ec) */

#define F_RELEGATE_TO 0x006831a0u       /* thiscall (club, record, 1) ret 0xc: the club's division becomes record */
#define F_PROMOTE_TO 0x006830b0u        /* thiscall (club, record, 1): as 0x64230c */
#define F_PROMOTE_RELEGATE 0x00689c80u  /* league_promote_and_relegate, thiscall (upper, lower, 1, pairwise, up, down) */
#define F_TIER_LIMIT 0x0068a980u        /* League::limit_club_division_tier, thiscall (1, 3, -3, 1) (0x6418e0) */
#define F_SHUFFLE 0x004afbd0u           /* cdecl (array, count, k), as 0x6422de */
#define F_QSORT 0x00945333u
#define F_LATITUDE 0x0053fd40u          /* database_compare_clubs_by_latitude (qsort of club pointers, 0x642826) */
#define F_LEAGUE_ROW 0x006826d0u        /* League::get_table_row, thiscall (index) */
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_IS_HUMAN 0x007875e0u          /* thiscall on [HUMANS] (club), 0x6426ec */
#define F_NEWS_NEW 0x0077bbe0u          /* news item constructor, thiscall (0x13, 0), 0x642700 */
#define F_NEWS_ARG 0x00785790u          /* thiscall (index, id), 0x64271e and 0x64272f */
#define F_NEWS_SEND 0x007861e0u         /* thiscall on [HUMANS] (news, club), 0x642745 */
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time, year, 0) */
#define F_GROUP_SLOT 0x0068a1c0u        /* league_round_set_match_slot, cdecl (table, round, slot, -1, -1, -1, 0) */

#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define STATUS_DOWN 3
#define GROUP_ROUND_SIZE 0x41u
#define ORIGINAL_CLUBS 18
#define MAX_CLUBS 20
#define GROUPS 3

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t object_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (id < 0 || !readable(table)) return 0;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(obj) ? obj : 0;
}

static uint32_t record_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t recs = rd32(g_mem, COMP_RECORDS);
    return id >= 0 && readable(recs) ? recs + COMP_RECORD_SIZE * (uint32_t)id : 0;
}

static const uint32_t GROUP_KEY[GROUPS] = {KEY_C1A, KEY_C1B, KEY_C2A};

/* ---- The rows ---- */

/* The row of group g in season s, or NULL when not held. Q520: from 2014/15 the rows "Serie C group A..C" take the
 * comp_file of the object they play in (ita_ser_c1a, ita_ser_c1b, ita_ser_c2a), and the C1 and C2 rows of those
 * seasons are n/a; so sizes, names and the rules report read them as the rows of these objects. After the last
 * row of the file (2027/28) that row holds (Q248, as slice 18's row_of_season): else the era would end and
 * the original's chain would read C2/B and C2/C (0x642506..0x642602), which a game without the flag lacks. */
static const SeasonRule *group_row(int s, int g)
{
    static const char *const comp[GROUPS] = {"Serie C group A", "Serie C group B", "Serie C group C"};
    static const char *const file[GROUPS] = {"ita_ser_c1a", "ita_ser_c1b", "ita_ser_c2a"};
    const SeasonRule *last = NULL;
    int later = 0;                      /* the file has a row in season s or after it */
    for (int i = 0, n = season_rules_count(); i < n; i++) {
        const SeasonRule *r = season_rules_row(i);
        if (strcmp(r->comp_file, file[g])) continue;
        if (r->season >= s) later = 1;
        if (strcmp(r->comp, comp[g])) continue;
        if (r->season == s) return r->clubs >= 2 && r->clubs <= MAX_CLUBS ? r : NULL;
        if (r->season < s && (!last || r->season > last->season)) last = r;
    }
    if (!later && last) return last->clubs >= 2 && last->clubs <= MAX_CLUBS ? last : NULL;
    return NULL;
}

/* The rules season of a season year: the year itself with changing rules on, the start year with them off (Q232);
 * 0 in the base game and with the season code off. */
static int rules_season(int year)
{
    if (!season_game_changes()) return 0;
    return season_changing_rules() ? year : season_start_year();
}

static int era_of_year(int year)
{
    int s = rules_season(year);
    return s && group_row(s, 0) && group_row(s, 1) && group_row(s, 2);
}

/* ---- The two-group Serie C2, 2011/12 to 2013/14 (Q529) ----
 *
 * With the extra division flag the original plays C2 in three groups of 18 (C2/A, C2/B, C2/C, 0x835678..0x835721).
 * From 2011/12 to 2013/14 the rows hold two groups (ita-176, ita-177, ita-193, ita-194, ita-210, ita-211) and C2/C is
 * not held (ita-178, ita-195, ita-212). A season is a two-group season when its rules season has the C2/A and C2/B
 * rows and no C2/C row, the flag is set and it is not in the Serie C era. Then C2/C ends as the era's ended groups do
 * (year 2999, Q623), and the chain, the Serie D exchange, the regroup and the starts of ItaSerA::start_new_season
 * 0x641700 take two groups. C2/A and C2/B hold the clubs of their rows, at most 20 (the 21 of C2/B in 2011/12 is 20,
 * as the era's groups). Without the flag C2 has no objects and the original's code runs. */
static const SeasonRule *c2_row(int s, int g)
{
    static const char *const comp[GROUPS] = {"Serie C2 group A", "Serie C2 group B", "Serie C2 group C"};
    static const char *const file[GROUPS] = {"ita_ser_c2a", "ita_ser_c2b", "ita_ser_c2c"};
    for (int i = 0, n = season_rules_count(); i < n; i++) {
        const SeasonRule *r = season_rules_row(i);
        if (r->season == s && !strcmp(r->comp_file, file[g]) && !strcmp(r->comp, comp[g]))
            return r->clubs >= 2 ? r : NULL;
    }
    return NULL;
}

static int extra_division_flag(void);

static int two_c2_of_year(int year)
{
    int s = rules_season(year);
    return s && extra_division_flag() && !era_of_year(year) && c2_row(s, 0) && c2_row(s, 1) && !c2_row(s, 2);
}

/* The clubs of C2 group g (0 or 1) in a two-group season s: its row, at most 20. */
static int c2_clubs(int s, int g)
{
    const SeasonRule *r = c2_row(s, g);
    int n = r ? r->clubs : 0;
    return n > MAX_CLUBS ? MAX_CLUBS : n;
}

static const uint32_t C2_KEY[GROUPS] = {KEY_C2A, KEY_C2B, KEY_C2C};

/* The C2 group (0 A, 1 B, 2 C) of a league object by its record (+4), or -1. */
static int c2_group(uint32_t obj)
{
    uint32_t rec = readable(obj) ? rd32(g_mem, obj + 4) : 0;
    for (int g = 0; g < GROUPS; g++)
        if (rec && rec == record_of(C2_KEY[g])) return g;
    return -1;
}

/* 1 when obj is C2/A or C2/B in a two-group season of its year. */
static int two_c2(uint32_t obj)
{
    int g = c2_group(obj);
    return g >= 0 && g < 2 && two_c2_of_year((int16_t)rd16(g_mem, obj + 0x40));
}

int season_ita_group(uint32_t obj)
{
    if (!readable(obj)) return -1;
    /* By the object's record (+4): while the constructor runs, the object is not yet in [0xadadfc] (the module and
     * build_comp_object_table 0x8386f0 fill it later). */
    uint32_t rec = rd32(g_mem, obj + 4);
    for (int g = 0; g < GROUPS; g++)
        if (rec && rec == record_of(GROUP_KEY[g])) return g;
    return -1;
}

int season_ita_serie_c(uint32_t obj)
{
    return season_ita_group(obj) >= 0 && era_of_year((int16_t)rd16(g_mem, obj + 0x40));
}

/* The clubs whose division is the record of a key id. */
static int clubs_of(uint32_t key)
{
    uint32_t rec = record_of(key), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    for (int i = 0; rec && readable(table) && i < count; i++) n += rd32(g_mem, table + CLUB_SIZE * (uint32_t)i + 0x57) == rec;
    return n;
}

int season_ita_division_size(const char *name, int season)
{
    static const char *const div[5] = {"Italian Serie C1/A", "Italian Serie C1/B", "Italian Serie C2/A",
                                       "Italian Serie C2/B", "Italian Serie C2/C"};
    if (name && two_c2_of_year(season)) {
        for (int k = 0; k < GROUPS; k++)
            if (!strcmp(name, div[GROUPS - 1 + k])) return k < 2 ? c2_clubs(rules_season(season), k) : 1;
        return -1;
    }
    if (!name || !era_of_year(season)) return -1;
    for (int k = 0; k < 5; k++)
        if (!strcmp(name, div[k])) {
            const SeasonRule *r = k < GROUPS ? group_row(rules_season(season), k) : NULL;
            if (k >= GROUPS) return extra_division_flag() ? 1 : 0;     /* one club with the flag (see below) */
            return r ? r->clubs : 0;
        }
    return -1;
}

/* ---- Sizes ---- */

int season_ita_cap(uint32_t obj, int cap)
{
    int g = season_ita_group(obj);
    return g >= 0 && g < 2 && season_ita_serie_c(obj) && cap < MAX_CLUBS ? MAX_CLUBS : cap;
}

uint32_t rt_ita_table_bytes(uint32_t obj, uint32_t original)
{
    return (season_ita_serie_c(obj) || two_c2(obj)) && original < MAX_CLUBS * ROW_SIZE ? MAX_CLUBS * ROW_SIZE : original;
}

/* The count store of C2/A's create_table (0x653aa2), and of C2/B's (0x657f62, through rt_ita_c2_end_clubs): the
 * clubs of its division, at most 20, in the Serie C era (C2/A) and in a two-group C2 season. */
static uint32_t own_clubs(uint32_t obj, uint32_t key, uint32_t original)
{
    int have = clubs_of(key), n = have > MAX_CLUBS ? MAX_CLUBS : have;
    if (n < 2) return original;
    /* check_complete 0x689ad0 counts the matches again: a group of 20 that goes back to 18 (C2 from 2012/13) kept
     * the 38 of its last season, was never complete, and Serie A never started 2013/14. */
    if ((uint32_t)n != original || (int16_t)rd16(g_mem, obj + 0x3e) != n) wr16(g_mem, obj + 0xa7, 0xffffu);
    return (uint32_t)n;
}

uint32_t rt_ita_c2a_clubs(uint32_t obj, uint32_t original)
{
    return season_ita_serie_c(obj) || two_c2(obj) ? own_clubs(obj, KEY_C2A, original) : original;
}

/* The match days of a group of 20 (38): the original's 34 of C1/A (create_schedule 0x64ad80, 0x64ae06..0x64b3a0:
 * Sundays from 5 September to 7 May, Wednesday 6 January and Tuesday 5 April), and 22 and 29 August and the
 * Wednesdays 27 October and 9 March (Q621), weekday 3 as the original's Wednesday 6 January. Month 0 to 11; year 1
 * the second calendar year. */
typedef struct { int8_t day, month, year, weekday, time; } Day;
static const Day DAYS_38[38] = {
    {22, 7, 0, 6, 1}, {29, 7, 0, 6, 1}, {5, 8, 0, 6, 1}, {12, 8, 0, 6, 1}, {19, 8, 0, 6, 1}, {26, 8, 0, 6, 1},
    {3, 9, 0, 6, 1}, {10, 9, 0, 6, 1}, {17, 9, 0, 6, 1}, {24, 9, 0, 6, 1}, {27, 9, 0, 3, 2}, {31, 9, 0, 6, 1},
    {7, 10, 0, 6, 1}, {14, 10, 0, 6, 1}, {21, 10, 0, 6, 1}, {28, 10, 0, 6, 1}, {5, 11, 0, 6, 1}, {12, 11, 0, 6, 1},
    {19, 11, 0, 6, 1}, {6, 0, 1, 3, 2}, {16, 0, 1, 6, 1}, {23, 0, 1, 6, 1}, {30, 0, 1, 6, 1}, {13, 1, 1, 6, 1},
    {20, 1, 1, 6, 1}, {27, 1, 1, 6, 1}, {5, 2, 1, 6, 1}, {9, 2, 1, 3, 2}, {12, 2, 1, 6, 1}, {19, 2, 1, 6, 1},
    {26, 2, 1, 6, 1}, {2, 3, 1, 6, 1}, {5, 3, 1, 2, 2}, {9, 3, 1, 6, 1}, {16, 3, 1, 6, 1}, {23, 3, 1, 6, 1},
    {30, 3, 1, 6, 1}, {7, 4, 1, 6, 1}};

/* create_schedule of C1/A, C1/B and C2/A, thiscall (stage, &rounds, &name, &flag) ret 0x10. Stage -1 is the league
 * (0x64ad91); the original makes 34 match days (D 0x22 at 0x64adbd, block 0x8a2 at 0x64adb8). A group of more than
 * 18 clubs in the Serie C era gets 38. */
static int schedule(uint32_t key)
{
    uint32_t obj = cpu.ecx;
    if ((int8_t)rt_arg(0) != -1 || !(season_ita_serie_c(obj) || two_c2(obj))) return 0;
    int n = clubs_of(key);
    if (n > MAX_CLUBS) n = MAX_CLUBS;
    if (n <= ORIGINAL_CLUBS) return 0;
    int rounds = 2 * (n + (n & 1) - 1);
    if (rounds > 38) rounds = 38;
    uint32_t sz[1] = {(uint32_t)rounds * GROUP_ROUND_SIZE};
    uint32_t t = call_cdecl(F_MALLOC, 1, sz);
    if (rt_unwinding) return 1;
    if (!readable(t)) return 0;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + 0x40);
    for (int r = 0; r < rounds && !rt_unwinding; r++) {
        const Day *d = &DAYS_38[r + 38 - rounds];
        uint32_t a[9] = {t, (uint32_t)r, (uint32_t)d->day, (uint32_t)d->month, (uint32_t)d->year, (uint32_t)d->weekday,
                         (uint32_t)d->time, year, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
        uint32_t s[7] = {t, (uint32_t)r, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        if (!rt_unwinding) call_cdecl(F_GROUP_SLOT, 7, s);
    }
    if (rt_unwinding) return 1;
    uint32_t out_rounds = rt_arg(1), out_name = rt_arg(2), out_flag = rt_arg(3);
    if (readable(out_flag)) wr32(g_mem, out_flag, 1);      /* as 0x64ada4 */
    if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint16_t)rounds);
    if (readable(out_name)) wr16(g_mem, out_name, 0);
    fprintf(stderr, "recomp: serie c: %.50s %u: %d clubs, %d match days\n", win_str(rd32(g_mem, obj + 4) + 4), year, n,
            rounds);
    rt_ret_stdcall(t, 4);
    return 1;
}

void o_0064ad80(void);
void o_0064ef30(void);
void o_006531a0(void);
void o_00657660(void);
RT_WRAP(0064ad80, c1a_schedule) { if (!schedule(KEY_C1A)) o_0064ad80(); }
RT_WRAP(0064ef30, c1b_schedule) { if (!schedule(KEY_C1B)) o_0064ef30(); }
RT_WRAP(006531a0, c2a_schedule) { if (!schedule(KEY_C2A)) o_006531a0(); }
/* ItaSerC2b::create_schedule 0x657660 (D 0x22 at 0x65769d, block 0x8a2 at 0x657698): C2/B of 20 in 2011/12. */
RT_WRAP(00657660, c2b_schedule) { if (!schedule(KEY_C2B)) o_00657660(); }

/* ---- The season change ---- */

/* The era of the season that ItaSerA starts: its year + 1 (start_new_season adds 1 at 0x641789, after the chain). */
static int next_era(uint32_t serie_a) { return era_of_year((int16_t)rd16(g_mem, serie_a + 0x40) + 1); }
static int next_season(uint32_t serie_a) { return rules_season((int16_t)rd16(g_mem, serie_a + 0x40) + 1); }

static uint32_t row_club(uint32_t league, int i, int *status)
{
    uint32_t a[1] = {(uint32_t)i};
    uint32_t row = call_this(F_LEAGUE_ROW, league, 1, a);
    if (rt_unwinding || !readable(row)) return 0;
    if (status) *status = g_mem[row + ROW_STATUS];
    return rd32(g_mem, row);
}

/* The clubs that the chain of this season change sends to the Serie D: the regroup does not take them back up
 * (the original takes its Serie D clubs up before it sends C2 clubs down, 0x6422de..0x64230c, then 0x642367). */
static uint32_t just_down[256];
static int just_down_n;

static int went_down(uint32_t club)
{
    for (int i = 0; i < just_down_n; i++)
        if (just_down[i] == club) return 1;
    return 0;
}

static void send_down(uint32_t serie_a, uint32_t club, uint32_t serie_d)
{
    uint32_t r[3] = {club, serie_d, 1};
    call_this(F_RELEGATE_TO, serie_a, 3, r);
    if (just_down_n < 256) just_down[just_down_n++] = club;
}

/* The Italian clubs whose division is record (0 for none: no division), less this season's relegated clubs. The
 * last 2 x [0xae2350] rows of the club table are national teams with no division (0x53b900), so a pool with no
 * division stops before them, as the German Cup does (0x5d9569..0x5d957c). */
static int pool(uint32_t record, uint32_t *out, int max)
{
    uint32_t table = rd32(g_mem, CLUBS), italy = rd32(g_mem, KEY_ITALY);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    if (record == 0) count -= 2 * (int)rd32(g_mem, NATION_COUNT);
    for (int i = 0; readable(table) && i < count && n < max; i++) {
        uint32_t c = table + CLUB_SIZE * (uint32_t)i, nat = rd32(g_mem, c + 0x53);
        if (rd32(g_mem, c + 0x57) != record || !readable(nat) || rd32(g_mem, nat) != italy) continue;
        if (record == 0 && (int16_t)rd16(g_mem, c + 0x80) <= 1) continue;       /* no extinct or empty club */
        if (went_down(c)) continue;
        out[n++] = c;
    }
    return n;
}

static void shuffle(uint32_t *clubs, int n, int k)
{
    if (n <= 1) return;
    uint32_t sz[1] = {4u * (uint32_t)n};
    uint32_t a = call_cdecl(F_MALLOC, 1, sz);
    if (rt_unwinding || !readable(a)) return;
    for (int i = 0; i < n; i++) wr32(g_mem, a + 4u * (uint32_t)i, clubs[i]);
    uint32_t s[3] = {a, (uint32_t)n, (uint32_t)k};
    call_cdecl(F_SHUFFLE, 3, s);
    if (rt_unwinding) return;
    for (int i = 0; i < n; i++) clubs[i] = rd32(g_mem, a + 4u * (uint32_t)i);
    uint32_t f[1] = {a};
    call_cdecl(F_FREE, 1, f);
}

static int by_reputation(const void *a, const void *b)
{
    int x = (int16_t)rd16(g_mem, *(const uint32_t *)a + 0x80), y = (int16_t)rd16(g_mem, *(const uint32_t *)b + 0x80);
    return (y > x) - (y < x);
}

#define IDLE_YEAR 2999                  /* Q395: a competition that never returns waits for year 2999 with no fixtures */

/* An ended league leaves the links of the leagues: +0x1c the record of the league above, +0x20 the one below (-1
 * for none). The season change pass starts a nation's leagues only through its top league (vtable +0xc, 0x48ce10:
 * false while +0x1c is not -1), and only when its table and each league below it by +0x20 is complete (vtable
 * +0x10, check_complete 0x689ad0; 0x48ce30..0x48ce54). An ended league is never complete again: with the extra
 * division the Serie A of a 2014 game never started 2015/16. So the league above takes its link below, and its own
 * +0x1c stays, so that the pass never takes it for a top league. ItaSerC2a::set_up_rules writes C2/A's links again
 * each season and at a load (+0x1c [0x9cf57c], +0x20 [0x9cf584], 0x653bf5..0x653c0c), so unlink_ended runs after
 * each start of C2/A, at a new game and after a load. */
static void ita_unlink(uint32_t obj)
{
    uint32_t table = rd32(g_mem, COMP_OBJECTS), slots = rd32(g_mem, 0xadadf8u);
    int32_t id = (int32_t)rd32(g_mem, rd32(g_mem, obj + 4)), below = (int32_t)rd32(g_mem, obj + 0x20);
    for (uint32_t i = 0; readable(table) && i < slots && i < 4096; i++) {
        uint32_t o = rd32(g_mem, table + 4u * i);
        if (readable(o) && o != obj && (int32_t)rd32(g_mem, o + 0x20) == id) wr32(g_mem, o + 0x20, (uint32_t)below);
        if (readable(o) && o != obj && below >= 0 && i == (uint32_t)below)
            wr32(g_mem, o + 0x1c, rd32(g_mem, obj + 0x1c));
    }
    wr32(g_mem, obj + 0x20, 0xffffffffu);
}

static void unlink_ended(void)
{
    const uint32_t keys[2] = {KEY_C2B, KEY_C2C};
    for (int k = 0; k < 2; k++) {
        uint32_t obj = object_of(keys[k]);
        if (obj && (int16_t)rd16(g_mem, obj + 0x40) == IDLE_YEAR) ita_unlink(obj);
    }
}

/* C2/B or C2/C ends: the year 2999, so it makes no fixtures and the chain never starts it again (0x641986). Its
 * table keeps its last season (the rules report shows that edition with year 2999). It leaves the links.
 *
 * A table of one club (a new game of the era with the flag) holds no result blocks. Each row's block at +9 holds
 * +0xa7 bytes, the matches of a club, which is 0 with one club. League::save_to_temp_file writes the blocks when
 * row 0 has one (0x689762..0x6897e3), and League::load_from_temp_file reads each with fread of +0xa7 bytes and wants
 * a count of 1 (0x688c59..0x688c98): fread of 0 bytes gives 0, so the load of a 2014 game with the extra division
 * stopped at its box (line 0x12e4, 0x688cea). With no block in row 0 the save writes 0 and the load gives each row
 * none (0x688d1f..0x688d3d), a state that the load itself makes. */
static void drop_results(uint32_t obj)
{
    uint32_t rows = rd32(g_mem, obj + 0xb1);
    int n = (int16_t)rd16(g_mem, obj + 0x3e);
    if (!readable(rows) || n > 1) return;
    for (int k = 0; k < n && !rt_unwinding; k++) {
        uint32_t at = rows + ROW_SIZE * (uint32_t)k + 9u, block = rd32(g_mem, at);
        if (!block) continue;
        uint32_t f[1] = {block};
        call_cdecl(F_FREE, 1, f);
        wr32(g_mem, at, 0);
    }
}

static void idle(uint32_t obj)
{
    if (!obj) return;
    wr16(g_mem, obj + 0x40, IDLE_YEAR);
    ita_unlink(obj);
    drop_results(obj);
    fprintf(stderr, "recomp: serie c: %.50s ends (year %d)\n", win_str(rd32(g_mem, obj + 4) + 4), IDLE_YEAR);
}

/* The change into the era (the season before has C1 and C2, changing rules on): the clubs of C2/B and C2/C (with the
 * extra division flag) that their table does not relegate (status 3, the 9 of each group in 2013/14, ita-210, ita-211)
 * join group C's record for the regroup; the relegated go to the Serie D. Then both objects end. */
static void change_into_era(uint32_t serie_a)
{
    uint32_t serie_d = record_of(KEY_SERIE_D), group_c = record_of(KEY_C2A);
    const uint32_t keys[2] = {KEY_C2B, KEY_C2C};
    for (int k = 0; k < 2 && !rt_unwinding; k++) {
        uint32_t obj = object_of(keys[k]);
        if (!obj || (int16_t)rd16(g_mem, obj + 0x40) == IDLE_YEAR) continue;     /* C2/C ended in 2011 (Q529) */
        int n = (int16_t)rd16(g_mem, obj + 0x3e), joined = 0, down = 0;
        for (int i = 0; i < n && n <= 60 && !rt_unwinding; i++) {
            int st = 0;
            uint32_t club = row_club(obj, i, &st);
            if (!readable(club)) continue;
            if (st == STATUS_DOWN) {
                send_down(serie_a, club, serie_d);
                down++;
            } else {
                wr32(g_mem, club + 0x57, group_c);              /* a move within the tier, as the regroup 0x642852 */
                joined++;
            }
        }
        if (rt_unwinding) return;
        fprintf(stderr, "recomp: serie c: %.50s: %d clubs to the Serie C, %d to the Serie D\n",
                win_str(rd32(g_mem, obj + 4) + 4), joined, down);
        idle(obj);
    }
}

/* The last season of each group club: its division (+0x5b) and final place (+0x5f, row i is place i + 1), from the
 * group's table while it still holds that season (its first row has played: a table of a new season has 0, row
 * +0x11, as check_complete 0x689b51 reads it). ItaCup::select_entrants
 * 0x63faf0 takes its lower clubs by them (0x63fc69..0x63fc93, 0x63fce6..0x63fd08) and its sort 0x640590 reads the
 * season of that division: in a 2014 game with the extra division no group club had them (the cup's loop never
 * ended, then comp_stats.CPP 1664). The cup can start before or after Serie A in the pass, so both mark them. */
static void mark_last_season(void)
{
    for (int i = 0; i < GROUPS; i++) {
        uint32_t obj = object_of(GROUP_KEY[i]), rec = record_of(GROUP_KEY[i]);
        uint32_t rows = obj ? rd32(g_mem, obj + 0xb1) : 0;
        if (!obj || !readable(rows) || !g_mem[rows + 0x11]) continue;
        int n = (int16_t)rd16(g_mem, obj + 0x3e);
        for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
            uint32_t club = row_club(obj, k, NULL);
            if (!readable(club)) continue;
            wr32(g_mem, club + 0x5b, rec);
            g_mem[club + 0x5f] = (uint8_t)(k + 1);
        }
    }
}

/* The points of row k of league (the column that sort_table reads: league_compare_table_rows 0x683210 takes
 * row + [league + 0xad] x 14 + 0x19, 0x683264..0x683275). */
static int row_points(uint32_t league, int k)
{
    uint32_t a[1] = {(uint32_t)k};
    uint32_t row = call_this(F_LEAGUE_ROW, league, 1, a);
    if (rt_unwinding || !readable(row)) return -1;
    return (int16_t)rd16(g_mem, row + 14u * rd32(g_mem, league + 0xad) + 0x19);
}

static void set_status(uint32_t league, int k, uint8_t st)
{
    uint32_t a[1] = {(uint32_t)k};
    uint32_t row = call_this(F_LEAGUE_ROW, league, 1, a);
    if (!rt_unwinding && readable(row)) g_mem[row + ROW_STATUS] = st;
}

/* The clubs that go up to Serie B (Q627). Each group's class keeps its own places and play-off (set_up_rules
 * 0x653bdd: +0xbe 1, +0xbf 4; the play-off of 0x653e90 reads +0xbf at 0x653f8f), so each group ends with two rows
 * that league_promote_and_relegate 0x689c80 moves up: status 0 (the winner) and 5 (0x689d47..0x689d4e). The real
 * format sends 4 up: the three group winners and the winner of a national play-off. Serie B sends down its rows of
 * status 3, and in pairs a row moves only with a partner (0x689d26..0x689d5d), so the two sides must match. So the
 * rows that go up are, to the number of Serie B's relegated rows: the three winners (status 0), then the play-off
 * winners (status 5) by points, then (when Serie B relegates more) the next rows by points. The other rows of status
 * 0 or 5 take 0xff, the status of a row that does not move (0x689df4). */
static void choose_promoted(uint32_t b, const uint32_t *g)
{
    int nb = (int16_t)rd16(g_mem, b + 0x3e), want = 0;
    for (int k = 0; k < nb && nb <= 60; k++) {
        int st = 0;
        row_club(b, k, &st);
        want += st == STATUS_DOWN;
    }
    typedef struct { int g, k, rank, points; } Cand;
    Cand c[GROUPS * MAX_CLUBS];
    int n = 0;
    for (int i = 0; i < GROUPS && !rt_unwinding; i++) {
        int rows = g[i] ? (int16_t)rd16(g_mem, g[i] + 0x3e) : 0;
        for (int k = 0; k < rows && k < MAX_CLUBS && n < GROUPS * MAX_CLUBS; k++) {
            int st = 0;
            if (!readable(row_club(g[i], k, &st)) || st == STATUS_DOWN) continue;
            c[n++] = (Cand){i, k, st == 0 ? 0 : st == 5 ? 1 : 2, row_points(g[i], k)};
        }
    }
    if (rt_unwinding) return;
    /* By rank (winner, play-off winner, other), then points, then place, then group order. */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            Cand x = c[j - 1], y = c[j];
            int later = x.rank != y.rank ? x.rank > y.rank : x.points != y.points ? x.points < y.points
                      : x.k != y.k ? x.k > y.k : x.g > y.g;
            if (!later) break;
            c[j - 1] = y, c[j] = x;
        }
    int up[GROUPS] = {0, 0, 0};
    for (int i = 0; i < n && !rt_unwinding; i++) {
        if (i < want) {
            if (c[i].rank == 2) set_status(g[c[i].g], c[i].k, 5);
            up[c[i].g]++;
        } else if (c[i].rank < 2) set_status(g[c[i].g], c[i].k, 0xff);
    }
    fprintf(stderr, "recomp: serie c: %d up to Serie B (groups A %d, B %d, C %d), %d down from Serie B\n",
            up[0] + up[1] + up[2], up[0], up[1], up[2], want);
}

/* The chain of the Serie C era, in place of ItaSerA::process_promotion_relegation_chain 0x642410. */
static void chain(uint32_t serie_a)
{
    just_down_n = 0;
    uint32_t a = object_of(KEY_A), b = object_of(KEY_B), g[GROUPS];
    for (int i = 0; i < GROUPS; i++) g[i] = object_of(GROUP_KEY[i]);
    uint32_t list[4] = {a, b, g[0], g[1]};
    /* The final places of each table first (vtable +0xa4, League::record_final_positions 0x68aa30, as
     * 0x642423..0x642465). */
    for (int i = 0; i < 4 && !rt_unwinding; i++)
        if (list[i]) call_this(rd32(g_mem, rd32(g_mem, list[i]) + 0xa4), list[i], 0, NULL);
    if (g[2] && !rt_unwinding) call_this(rd32(g_mem, rd32(g_mem, g[2]) + 0xa4), g[2], 0, NULL);
    if (rt_unwinding || !a || !b) return;
    mark_last_season();
    if (rt_unwinding) return;
    /* Serie A and B, 4 and 4 in pairs (0x64248e); B and each group in pairs (as 0x6424b6, 0x6424de), after the
     * choice of the clubs that go up (Q627). */
    uint32_t ab[6] = {a, b, 1, 1, 4, 4};
    call_this(F_PROMOTE_RELEGATE, serie_a, 6, ab);
    /* The change into the era with the extra division flag: C2/A played the season before as a C2 group, two tiers
     * below Serie B (the C2 rows of 2013/14 send no club up: all join the single Serie C, ita-210, ita-211), so its
     * rows of status 0 and 5 do not go up to Serie B: they take 0xff (0x689df4). Q529. */
    if (!rt_unwinding && g[2] && extra_division_flag() && !era_of_year((int16_t)rd16(g_mem, serie_a + 0x40))) {
        int n = (int16_t)rd16(g_mem, g[2] + 0x3e);
        for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
            int st = 0;
            if (readable(row_club(g[2], k, &st)) && (st == 0 || st == 5)) set_status(g[2], k, 0xff);
        }
    }
    if (!rt_unwinding) choose_promoted(b, g);
    for (int i = 0; i < GROUPS && !rt_unwinding; i++) {
        if (!g[i]) continue;
        uint32_t bg[6] = {b, g[i], 1, 1, 0xffffffffu, 0xffffffffu};
        call_this(F_PROMOTE_RELEGATE, serie_a, 6, bg);
    }
    if (rt_unwinding) return;
    /* The relegated rows of each group (status 3) go to the Serie D data division, as exchange_serie_d_clubs does
     * for C2 (0x642317..0x6423e6), and as many clubs of the Serie D come up (0x6422a3..0x64230c), into group C
     * (the regroup then puts each club in its group). Q620, Q622. */
    uint32_t serie_d = record_of(KEY_SERIE_D);
    int down = 0;
    for (int i = 0; i < GROUPS && !rt_unwinding; i++) {
        if (!g[i]) continue;
        int n = (int16_t)rd16(g_mem, g[i] + 0x3e);
        for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
            int st = 0;
            uint32_t club = row_club(g[i], k, &st);
            if (!readable(club) || st != STATUS_DOWN) continue;
            send_down(serie_a, club, serie_d);
            down++;
        }
    }
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: serie c: %d clubs to the Serie D\n", down);
    if (!era_of_year((int16_t)rd16(g_mem, serie_a + 0x40))) change_into_era(serie_a);
    /* The clubs that come up are taken in the regroup: it fills each group to its count. */
}

/* The regroup of the Serie C era, in place of regroup_serie_c2_clubs 0x642790 (and regroup_serie_c1_clubs 0x642630,
 * which does nothing then): the clubs of the three groups, filled from the Serie D data division (shuffled, as
 * 0x6422de), then from the Italian clubs with no division by reputation (Q622), or cut by reputation to the Serie D,
 * to the counts of the season's rows, sorted by latitude (0x53fd40) and split into C (the first), B and A. A human
 * manager whose club changes group gets the news of 0x6426e5..0x642745. */
/* The counts of the regroup: from[] the records whose clubs it takes, want[] the clubs of each (0 for a record that
 * keeps none); the clubs sorted by latitude fill the last record with a count first (the south). It returns the
 * club count and fills the numbers of the log line. */
typedef struct { int from_d, from_none, cut, news; } RegroupLog;

static int regroup_into(uint32_t serie_a, const uint32_t *rec, const int *want, int groups, RegroupLog *log)
{
    int total = 0;
    for (int g = 0; g < groups; g++) {
        if (!rec[g]) return 0;
        total += want[g];
    }
    uint32_t serie_d = record_of(KEY_SERIE_D);
    static uint32_t clubs[4096], extra[4096];
    int n = 0;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; readable(table) && i < count && n < 4096; i++) {
        uint32_t c = table + CLUB_SIZE * (uint32_t)i, d = rd32(g_mem, c + 0x57);
        for (int g = 0; g < groups; g++)
            if (d == rec[g]) {
                clubs[n++] = c;
                break;
            }
    }
    int from_d = 0, from_none = 0, cut = 0;
    if (n < total) {
        int m = pool(serie_d, extra, 4096);
        shuffle(extra, m, m);
        if (rt_unwinding) return 0;
        for (int i = 0; i < m && n < total; i++) clubs[n++] = extra[i], from_d++;
    }
    if (n < total) {
        int m = pool(0, extra, 4096);
        qsort(extra, (size_t)m, sizeof *extra, by_reputation);
        for (int i = 0; i < m && n < total; i++) clubs[n++] = extra[i], from_none++;
    }
    if (n > total) {
        /* Too many clubs: the lowest reputations go to the Serie D. */
        uint32_t idx[4096];
        for (int i = 0; i < n; i++) idx[i] = clubs[i];
        qsort(idx, (size_t)n, sizeof *idx, by_reputation);
        for (int i = total; i < n && !rt_unwinding; i++) {
            uint32_t r[3] = {idx[i], serie_d, 1};
            call_this(F_RELEGATE_TO, serie_a, 3, r);
            cut++;
        }
        n = 0;
        for (int i = 0; i < total; i++) clubs[n++] = idx[i];
    }
    if (rt_unwinding) return 0;
    /* Sort by latitude with the game's qsort on a game array. */
    uint32_t sz[1] = {4u * (uint32_t)n};
    uint32_t arr = call_cdecl(F_MALLOC, 1, sz);
    if (rt_unwinding || !readable(arr)) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, arr + 4u * (uint32_t)i, clubs[i]);
    uint32_t q[4] = {arr, (uint32_t)n, 4, F_LATITUDE};
    call_cdecl(F_QSORT, 4, q);
    if (rt_unwinding) return 0;
    int news = 0;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        uint32_t club = rd32(g_mem, arr + 4u * (uint32_t)i);
        int g = groups - 1, upto = want[g];
        while (g > 0 && i >= upto) upto += want[--g];
        int was = 0;
        uint32_t old = rd32(g_mem, club + 0x57);
        for (int k = 0; k < groups; k++) was |= old == rec[k];
        if (old != rec[g] && was) {
            uint32_t h[1] = {club};
            uint32_t human = call_this(F_IS_HUMAN, rd32(g_mem, HUMANS), 1, h);
            if (rt_unwinding) return 0;
            if (human) {
                uint32_t saved_esp = cpu.esp;
                cpu.esp -= 0x200;
                uint32_t item = cpu.esp + 0x10;
                uint32_t c[2] = {0x13, 0};
                call_this(F_NEWS_NEW, item, 2, c);
                uint32_t a0[2] = {0, rd32(g_mem, club)}, a1[2] = {1, rd32(g_mem, rec[g])};
                if (!rt_unwinding) call_this(F_NEWS_ARG, item, 2, a0);
                if (!rt_unwinding) call_this(F_NEWS_ARG, item, 2, a1);
                uint32_t sn[2] = {item, club};
                if (!rt_unwinding) call_this(F_NEWS_SEND, rd32(g_mem, HUMANS), 2, sn);
                if (rt_unwinding) return 0;
                cpu.esp = saved_esp;
                news++;
            }
        }
        if (old != rec[g]) {
            if (was) wr32(g_mem, club + 0x57, rec[g]);         /* as 0x642852 */
            else {
                uint32_t p[3] = {club, rec[g], 1};
                call_this(F_PROMOTE_TO, serie_a, 3, p);
            }
        }
    }
    uint32_t f[1] = {arr};
    if (!rt_unwinding) call_cdecl(F_FREE, 1, f);
    *log = (RegroupLog){from_d, from_none, cut, news};
    return n;
}

static void regroup(uint32_t serie_a)
{
    int s = next_season(serie_a), want[GROUPS], total = 0;
    uint32_t rec[GROUPS];
    for (int g = 0; g < GROUPS; g++) {
        const SeasonRule *r = group_row(s, g);
        want[g] = r ? r->clubs : 0;
        total += want[g];
        rec[g] = record_of(GROUP_KEY[g]);
    }
    if (total < 2 * GROUPS) return;
    RegroupLog l = {0, 0, 0, 0};
    int n = regroup_into(serie_a, rec, want, GROUPS, &l);
    if (rt_unwinding || !n) return;
    int y = (int16_t)rd16(g_mem, serie_a + 0x40) + 1;      /* the season that starts (the rules of s) */
    fprintf(stderr, "recomp: serie c %d/%02d: %d clubs in groups of %d, %d and %d (%d from the Serie D, %d with no "
                    "division, %d to the Serie D, %d news)\n", y, (y + 1) % 100, n, want[0], want[1], want[2], l.from_d,
            l.from_none, l.cut, l.news);
}

/* ---- The season change of the two-group Serie C2 (Q529) ---- */

static int c2c_ended(void)
{
    uint32_t c = object_of(KEY_C2C);
    return c && (int16_t)rd16(g_mem, c + 0x40) == IDLE_YEAR;
}

/* The season that ItaSerA starts is a two-group season, or C2/C has ended. */
static int c2_code(uint32_t serie_a)
{
    return two_c2_of_year((int16_t)rd16(g_mem, serie_a + 0x40) + 1) || c2c_ended();
}

static int rows_of_status(uint32_t league, int up)
{
    int n = league ? (int16_t)rd16(g_mem, league + 0x3e) : 0, got = 0;
    for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
        int st = 0;
        if (!readable(row_club(league, k, &st))) continue;
        got += up ? st == 0 || st == 5 : st == STATUS_DOWN;
    }
    return got;
}

/* The chain with two C2 groups, in place of 0x642410 once C2/C has ended (the change into two groups runs the
 * original's chain, as C2/C still plays that season): the final places of A, B, C1/A, C1/B, C2/A and C2/B
 * (0x642423..0x642465, 0x6424fb, 0x642511), then the original's calls of league_promote_and_relegate 0x689c80 with
 * their arguments, less the two of C2/C (0x6425f0, 0x642618): A and B (1, 1, 4, 4; 0x64248e), B and C1/A, B and
 * C1/B (1, 1, -1, -1; 0x6424b6, 0x6424de), C1/A and C2/A (1, 1; 0x642550), C1/B and C2/A (0, 1; 0x642578), C1/A
 * and C2/B (1, 1; 0x6425a0), C1/B and C2/B (0, 1; 0x6425c8). */
static void c2_chain(uint32_t serie_a)
{
    uint32_t a = object_of(KEY_A), b = object_of(KEY_B), c1a = object_of(KEY_C1A), c1b = object_of(KEY_C1B);
    uint32_t c2a = object_of(KEY_C2A), c2b = object_of(KEY_C2B);
    uint32_t list[6] = {a, b, c1a, c1b, c2a, c2b};
    for (int i = 0; i < 6 && !rt_unwinding; i++)
        if (list[i]) call_this(rd32(g_mem, rd32(g_mem, list[i]) + 0xa4), list[i], 0, NULL);
    if (rt_unwinding || !a || !b || !c1a || !c1b || !c2a || !c2b) return;
    const uint32_t pairs[3][6] = {{a, b, 1, 1, 4, 4}, {b, c1a, 1, 1, 0xffffffffu, 0xffffffffu},
                                  {b, c1b, 1, 1, 0xffffffffu, 0xffffffffu}};
    for (int i = 0; i < 3 && !rt_unwinding; i++) call_this(F_PROMOTE_RELEGATE, serie_a, 6, pairs[i]);
    if (rt_unwinding) return;
    int down_a = rows_of_status(c1a, 0), down_b = rows_of_status(c1b, 0);
    int up_a = rows_of_status(c2a, 1), up_b = rows_of_status(c2b, 1);
    /* The class's play-off takes places 2 to 5 whatever the direct places are (a test with 3 direct places gave
     * the statuses 5 0xfe 0 0xfe 0xfe 1 1: the champion, the play-off's winner and losers, and two play-off places
     * that never played). So each group sends 2 up (status 5 the champion and 0 the play-off winner, the rows that
     * 0x689c80 moves, 0x689d47..0x689d4e), 4 in all, while C1 sends 6 down (1 direct and 2 play-out losers a
     * group) and C1's regroup takes a fixed 36 (0x64269c). The rows give each group 2 direct places and 1 by the
     * play-off. So, while C2 sends up fewer than C1 sends down, the best row by place of each group (A, then B)
     * that does not go up and is not relegated takes status 0 (Q529). */
    int by_place = up_a + up_b;
    for (int turn = 0; up_a + up_b < down_a + down_b && turn < 2 * MAX_CLUBS && !rt_unwinding; turn++) {
        uint32_t c2 = turn % 2 ? c2b : c2a;
        int n = (int16_t)rd16(g_mem, c2 + 0x3e);
        for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
            int st = 0;
            if (!readable(row_club(c2, k, &st)) || st == 0 || st == 5 || st == STATUS_DOWN) continue;
            set_status(c2, k, 0);
            if (turn % 2) up_b++;
            else up_a++;
            break;
        }
    }
    const uint32_t lower[4][6] = {{c1a, c2a, 1, 1, 0xffffffffu, 0xffffffffu}, {c1b, c2a, 0, 1, 0xffffffffu, 0xffffffffu},
                                  {c1a, c2b, 1, 1, 0xffffffffu, 0xffffffffu}, {c1b, c2b, 0, 1, 0xffffffffu, 0xffffffffu}};
    for (int i = 0; i < 4 && !rt_unwinding; i++) call_this(F_PROMOTE_RELEGATE, serie_a, 6, lower[i]);
    if (rt_unwinding) return;
    by_place = up_a + up_b - by_place;
    fprintf(stderr, "recomp: serie c2: %d down from Serie C1 (A %d, B %d), %d up from Serie C2 (A %d, B %d; %d by "
                    "place)\n", down_a + down_b, down_a, down_b, up_a + up_b, up_a, up_b, by_place);
}

/* The Serie D exchange with two C2 groups, in place of exchange_serie_d_clubs 0x6421c0: the relegated rows (status
 * 3) of each C2 group that still plays go to the Serie D (as 0x642317..0x6423e6, each group to its own count). The
 * clubs that come up are taken in the regroup (it fills each group to its row, as the era's regroup), so the 9 of
 * 0x6422da are not taken here. */
static void c2_exchange(uint32_t serie_a)
{
    just_down_n = 0;
    uint32_t serie_d = record_of(KEY_SERIE_D);
    int down = 0;
    for (int g = 0; g < GROUPS && !rt_unwinding; g++) {
        uint32_t obj = object_of(C2_KEY[g]);
        if (!obj || (int16_t)rd16(g_mem, obj + 0x40) == IDLE_YEAR) continue;
        int n = (int16_t)rd16(g_mem, obj + 0x3e);
        for (int k = 0; k < n && n <= 60 && !rt_unwinding; k++) {
            int st = 0;
            uint32_t club = row_club(obj, k, &st);
            if (!readable(club) || st != STATUS_DOWN) continue;
            send_down(serie_a, club, serie_d);
            down++;
        }
    }
    if (!rt_unwinding) fprintf(stderr, "recomp: serie c2: %d clubs to the Serie D\n", down);
}

/* The regroup with two C2 groups, in place of regroup_serie_c2_clubs 0x642790 (54 clubs in groups of 18, the first
 * by latitude to C2/C, 0x64284a..0x642864): the clubs of C2/A, C2/B and C2/C (at the change into two groups C2/C
 * still has its clubs), to the counts of the rows (filled from the Serie D, then the clubs with no division; cut by
 * reputation to the Serie D), the first by latitude to C2/B (the southern group), the rest to C2/A. Then C2/C ends
 * (Q623). */
static void c2_regroup(uint32_t serie_a)
{
    int s = next_season(serie_a), want[GROUPS] = {c2_clubs(s, 0), c2_clubs(s, 1), 0};
    uint32_t rec[GROUPS] = {record_of(KEY_C2A), record_of(KEY_C2B), record_of(KEY_C2C)};
    if (want[0] < 2 || want[1] < 2) return;
    RegroupLog l = {0, 0, 0, 0};
    int n = regroup_into(serie_a, rec, want, GROUPS, &l);
    if (rt_unwinding) return;
    uint32_t c = object_of(KEY_C2C);
    if (c && (int16_t)rd16(g_mem, c + 0x40) != IDLE_YEAR) idle(c);
    int y = (int16_t)rd16(g_mem, serie_a + 0x40) + 1;
    fprintf(stderr, "recomp: serie c2 %d/%02d: %d clubs in groups of %d and %d (%d from the Serie D, %d with no "
                    "division, %d to the Serie D, %d news)\n", y, (y + 1) % 100, n, want[0], want[1], l.from_d,
            l.from_none, l.cut, l.news);
}

/* The era's code for the season that ItaSerA starts: in the era, and also when the C2/A object exists without
 * C2/B (the module's group C of a game without the flag): the original's chain, exchange and regroup read C2/B and
 * C2/C whenever C2/A exists (0x642506..0x642602, 0x6421dc, 0x6427aa). */
static int era_code(uint32_t serie_a)
{
    return next_era(serie_a) || (object_of(KEY_C2A) && !object_of(KEY_C2B));
}

void o_00642410(void);
void o_006421c0(void);
void o_00642790(void);
void o_00641fc0(void);
void o_00642630(void);

/* ItaSerA::process_promotion_relegation_chain, thiscall (1) ret 4. */
RT_WRAP(00642410, ita_chain)
{
    uint32_t serie_a = cpu.ecx;
    if (!era_code(serie_a) && !c2c_ended()) {
        o_00642410();
        return;
    }
    if (era_code(serie_a)) chain(serie_a);
    else c2_chain(serie_a);
    if (!rt_unwinding) rt_ret_stdcall(cpu.eax, 1);
}

/* exchange_serie_d_clubs, exchange_serie_c2_clubs_unplayed and regroup_serie_c1_clubs, thiscall () ret: in the Serie
 * C era the chain and the regroup do their work. */
RT_WRAP(006421c0, ita_exchange_d)
{
    uint32_t serie_a = cpu.ecx;
    if (era_code(serie_a)) rt_ret_cdecl(cpu.eax);
    else if (!c2_code(serie_a)) o_006421c0();
    else {
        c2_exchange(serie_a);
        if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
    }
}
RT_WRAP(00641fc0, ita_exchange_c2)
{
    if (!era_code(cpu.ecx)) {
        o_00641fc0();
        return;
    }
    regroup(cpu.ecx);           /* no C2/A object: the regroup still makes the three groups it can */
    if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
}
RT_WRAP(00642630, ita_regroup_c1)
{
    if (!era_code(cpu.ecx)) o_00642630();
    else rt_ret_cdecl(cpu.eax);
}
RT_WRAP(00642790, ita_regroup_c2)
{
    if (!era_code(cpu.ecx) && !c2_code(cpu.ecx)) {
        o_00642790();
        return;
    }
    if (era_code(cpu.ecx)) regroup(cpu.ecx);
    else c2_regroup(cpu.ecx);
    if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
}

/* 0x6418d5, the load of the C2/A object that picks the tier block (C2/A, B, C or C1/A, B, 0x6418dc): in the Serie
 * C era the tier call goes to C2/A here, and 0 sends the block to C1/A and C1/B (0x641915). The four arguments are
 * pushed around this load (0x6418d1..0x6418dc). */
uint32_t rt_ita_tier_c2(uint32_t c2a)
{
    uint32_t serie_a = object_of(KEY_A);
    if (!c2a || !serie_a) return c2a;
    int y = (int16_t)rd16(g_mem, serie_a + 0x40), two = two_c2_of_year(y);
    if (!era_of_year(y) && !two) return c2a;
    uint32_t a[4] = {1, 3, 0xfffffffdu, 1};
    call_this(F_TIER_LIMIT, c2a, 4, a);
    /* Two C2 groups (Q529): C2/B too (0x6418fb), not the ended C2/C (0x641939); the 0 then gives the call to C1/A
     * and C1/B as well (0x641915), as in the era. */
    uint32_t c2b = object_of(KEY_C2B), b[4] = {1, 3, 0xfffffffdu, 1};
    if (two && c2b && !rt_unwinding) call_this(F_TIER_LIMIT, c2b, 4, b);
    return 0;
}

#define F_FREE_TABLE 0x00687970u        /* League::free_table, thiscall (0: its own table) ret 4 */
#define F_C2A_TABLE 0x00653a90u         /* ItaSerC2a::create_table, thiscall () */
#define F_CREATE_FIXTURES 0x006835c0u   /* League::create_fixtures, thiscall () */
#define F_SORT_TABLE 0x006827d0u        /* League::sort_table, thiscall (0) ret 4 */
#define F_EXPECTATIONS 0x0068aa80u      /* League::set_club_season_expectations, thiscall () */

/* The group C object that the module makes in a season change pass, which waits for its fixtures (below). */
static uint32_t c2a_pending, c2a_making;

/* 0x641986, the load of the C2/A object that starts the C2 groups (0x64198b): in the Serie C era C2/A starts here
 * (its vtable +8, as 0x64198f) and 0 skips C2/B and C2/C. ItaSerA's year is already the new season (0x641789). */
uint32_t rt_ita_start_c2(uint32_t c2a)
{
    uint32_t serie_a = object_of(KEY_A);
    if (!c2a || !serie_a) return c2a;
    int y = (int16_t)rd16(g_mem, serie_a + 0x40), cy = (int16_t)rd16(g_mem, c2a + 0x40);
    if (two_c2_of_year(y)) {
        /* Two C2 groups (Q529): C2/A and C2/B start (0x64198f, 0x6419a3), the ended C2/C does not (0x6419b7). */
        uint32_t c2b = object_of(KEY_C2B);
        if (cy < y) call_this(rd32(g_mem, rd32(g_mem, c2a) + 8), c2a, 0, NULL);
        if (c2b && !rt_unwinding && (int16_t)rd16(g_mem, c2b + 0x40) < y)
            call_this(rd32(g_mem, rd32(g_mem, c2b) + 8), c2b, 0, NULL);
        if (!rt_unwinding) unlink_ended();
        return 0;
    }
    if (!era_of_year(y)) return c2a;
    if (c2a == c2a_pending) {
        /* Made by the module in this pass (it holds the new season) with no fixtures: now, after the regroup, the
         * group gets its rules, match days, table and fixtures once, in the order of ItaSerC2a::start_new_season
         * 0x653c40 less its archive and its year step: +0x4c 0 (0x653c4d), League::free_table 0x687970 (0x653c50),
         * free of the match days at +0xba (0x653c55..0x653c68), set_up_rules (vtable +0x8c, 0x653cad: the match
         * days of the group's count, create_schedule at 0x653c27), create_table (0x653cec), create_fixtures
         * (0x653d27), sort_table (0x653d63), vtable +0x5c (0x653dd5) and 0x68aa80 (0x653dda). A start would
         * archive a season that was never played (0x653c77) and make the fixtures again. */
        c2a_pending = 0;
        uint32_t zero[1] = {0};
        wr32(g_mem, c2a + 0x4c, 0);
        call_this(F_FREE_TABLE, c2a, 1, zero);
        uint32_t days = rd32(g_mem, c2a + 0xba);
        if (!rt_unwinding && days) {
            uint32_t f[1] = {days};
            call_cdecl(F_FREE, 1, f);
            wr32(g_mem, c2a + 0xba, 0);
        }
        if (!rt_unwinding) call_this(rd32(g_mem, rd32(g_mem, c2a) + 0x8c), c2a, 0, NULL);
        if (!rt_unwinding) call_this(F_C2A_TABLE, c2a, 0, NULL);
        if (!rt_unwinding) call_this(F_CREATE_FIXTURES, c2a, 0, NULL);
        if (!rt_unwinding) call_this(F_SORT_TABLE, c2a, 1, zero);
        if (!rt_unwinding) call_this(rd32(g_mem, rd32(g_mem, c2a) + 0x5c), c2a, 0, NULL);
        if (!rt_unwinding) call_this(F_EXPECTATIONS, c2a, 0, NULL);
        if (rt_unwinding) return 0;
        fprintf(stderr, "recomp: serie c: group C %d: table and fixtures of %d clubs\n", y,
                (int16_t)rd16(g_mem, c2a + 0x3e));
    } else if (cy < y) {
        call_this(rd32(g_mem, rd32(g_mem, c2a) + 8), c2a, 0, NULL);
    }
    if (!rt_unwinding) unlink_ended();
    return 0;
}

/* ---- Group C without the extra division flag ----
 *
 * create_italian_comps 0x835320 makes C2/A (constructor 0x652e90, thiscall (year, record) ret 8, operator new 0xee
 * as 0x835696..0x8356ce) only with bit 4 of nation +0x11c (0x835407). The module of slice 26 (season_comps.h) makes
 * it in the Italian entry 21 (the nation of entry 21 is Italy: key_nation_build_table 0x667150 stores [0x9cf360]'s
 * nation at 0xb64348 = 0xb63d60 + 21 x 0x48) in the first season of the table's group C, and again at a load from
 * its saved file. With the flag the original's object exists, so make returns 0 (no second object, also at a load:
 * the creators make the original's object then). The def's file is the class's own file, so the report and the
 * rule kinds read it as the C2/A object it is. */
#define F_NEW 0x00944cf1u
#define F_C2A_NEW 0x00652e90u
#define NATIONS 0xae23a8u
#define NATION_SIZE 0x122u
#define NATION_FLAGS 0x11cu
#define FLAG_EXTRA 4

static int extra_division_flag(void)
{
    int32_t italy = (int32_t)rd32(g_mem, KEY_ITALY);
    uint32_t table = rd32(g_mem, NATIONS);
    return italy >= 0 && readable(table) && (g_mem[table + NATION_SIZE * (uint32_t)italy + NATION_FLAGS] & FLAG_EXTRA);
}

/* In a season change pass (Serie A still holds the season before) the module makes group C before ItaSerA's
 * start, so before the regroup: its constructor then makes no fixtures (season_ita_no_fixtures), and
 * rt_ita_start_c2 makes its table and fixtures after the regroup. At a new game and at a load it is made whole. */
static uint32_t group_c_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    (void)d;
    if (extra_division_flag() || object_of(KEY_C2A)) return 0;
    uint32_t serie_a = object_of(KEY_A);
    int pass = !loading && serie_a && (int16_t)rd16(g_mem, serie_a + 0x40) == year - 1;
    uint32_t sz[1] = {0xee};
    uint32_t obj = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(obj)) return 0;
    uint32_t a[2] = {(uint32_t)year, rec};
    c2a_making = pass ? obj : 0;
    call_this(F_C2A_NEW, obj, 2, a);
    c2a_making = 0;
    if (rt_unwinding) return 0;
    if (pass) c2a_pending = obj;
    fprintf(stderr, "recomp: serie c: group C object made on the C2/A record for %d%s\n", year,
            loading ? " (load)" : "");
    return obj;
}

int season_ita_no_fixtures(uint32_t league)
{
    return c2a_making && league == c2a_making;
}

const SeasonCompDef season_ita_group_c = {"Serie C group C", "ita_ser_c2a", "Italian Serie C2/A", 21, group_c_make,
                                          NULL, NULL, 0};

/* ---- News ----
 *
 * The names of the groups ("Lega Pro girone A" to 2016/17, "Serie C girone A" from 2017/18) come from the names table
 * (slice 15): Q520 gives their rows the files of the three objects. This kind says the change of the format. */
static void kind_season(int s)
{
    if (!era_of_year(s) || era_of_year(s - 1)) return;
    const SeasonRule *a = group_row(s, 0), *b = group_row(s, 1), *c = group_row(s, 2);
    season_change_news("Serie C1 and Serie C2 end: the Serie C is now one division of three groups of %d, %d and %d "
                       "clubs.", a->clubs, b->clubs, c->clubs);
}

/* A new game of the era: C2/B and C2/C (made with the extra division flag, one club each) end now. */
static void kind_new_game(int y)
{
    if (two_c2_of_year(y)) {
        uint32_t c = object_of(KEY_C2C);
        if (c && (int16_t)rd16(g_mem, c + 0x3e) <= 1) idle(c);       /* a new game of two C2 groups (Q529) */
        return;
    }
    if (!era_of_year(y)) return;
    uint32_t b = object_of(KEY_C2B), c = object_of(KEY_C2C);
    if (b && (int16_t)rd16(g_mem, b + 0x3e) <= 1) idle(b);
    if (c && (int16_t)rd16(g_mem, c + 0x3e) <= 1) idle(c);
}

/* A load: the constructors and set_up_rules give C2/A, C2/B and C2/C their links again (0x653bf5..0x653c0c). */
static void kind_loaded(int y)
{
    (void)y;
    unlink_ended();
}

const SeasonRuleKind season_kind_ita_serie_c = {"ita_serie_c", kind_new_game, kind_loaded, kind_season, NULL, NULL,
                                                NULL};

/* ---- C2/B and C2/C at a new game of the era ----
 *
 * With the extra division flag the original makes C2/B and C2/C (0x8356ce, 0x835721). League::create_fixtures
 * 0x6835c0 cannot make a league of 0 clubs (its first block, n + n mod 2 pointers, at 0x683615..0x683638: box
 * league.cpp 1020 in a 2014 game), and create_table wants its whole count (0x64b755..0x64b763 in C1/A). So in the
 * era each keeps one club (season_ita_division_size gives 1 with the flag): its count store (0x657f62, 0x65c0f2)
 * gives 1, the round robin of one club and a rest place has no match (0x683ac9 on), check_complete counts 0
 * matches ((1 - 1) x 2, 0x689b1d..0x689b36), and the new game hook ends the object (year 2999). */
uint32_t rt_ita_c2_end_clubs(uint32_t obj, uint32_t original)
{
    if (c2_group(obj) == 1 && two_c2(obj)) return own_clubs(obj, KEY_C2B, original);
    if (c2_group(obj) == 2 && two_c2_of_year((int16_t)rd16(g_mem, obj + 0x40)))
        return clubs_of(KEY_C2C) == 1 ? 1u : original;
    if (!era_of_year((int16_t)rd16(g_mem, obj + 0x40))) return original;
    int32_t id = (int32_t)rd32(g_mem, rd32(g_mem, obj + 4));
    uint32_t key = id == (int32_t)rd32(g_mem, KEY_C2B) ? KEY_C2B : id == (int32_t)rd32(g_mem, KEY_C2C) ? KEY_C2C : 0;
    return key && clubs_of(key) == 1 ? 1u : original;
}

/* The fixed fixture lists of C2/B and C2/C (0x658750, 0x65c8e0, called by the constructors before create_fixtures,
 * for example 0x65b5d6): each allocates (clubs / 2) x match days entries of 16 bytes (0x65c900..0x65c91e) into +0xea
 * and writes 306 of the 2001 season. With one club the block holds none, so the writes run past it (a later block of
 * create_fixtures then failed: league.cpp 1020). create_fixtures uses the list only when its year is the league's
 * (0x68382e..0x68383d), never in the era; so in the era the builders make no list. */
void o_00658750(void);
void o_0065c8e0(void);

static int no_fixed_list(void)
{
    uint32_t obj = cpu.ecx;
    int y = (int16_t)rd16(g_mem, obj + 0x40);
    if (!era_of_year(y) && !two_c2_of_year(y)) return 0;
    wr32(g_mem, obj + 0xea, 0);
    rt_ret_cdecl(cpu.eax);
    return 1;
}

RT_WRAP(00658750, c2b_fixed_list) { if (!no_fixed_list()) o_00658750(); }
RT_WRAP(0065c8e0, c2c_fixed_list) { if (!no_fixed_list()) o_0065c8e0(); }

/* ---- The Coppa Italia Serie C in the era (Q625) ----
 *
 * ItaCCup::select_entrants 0x63dfe0 sets 90 entrants (+0xf2 0x5a at 0x63dff4, a block of 0x168 bytes at +0xee) and
 * takes the Italian clubs whose division is C1/A, C1/B, C2/A, C2/B or C2/C (0x63e094..0x63e107) up to 90; the code
 * after it reads all 90 slots (0x63e109..). The original's C1 and C2 hold 90; the era's three groups hold 60 or fewer,
 * and the constructor (0x63ccce) stopped at "Database..cpp 16398". Until slice 21 gives the cup its real 60 entrants
 * (ita-235), the clubs that are short come from the Serie D by reputation: for the call they count as C2/B clubs,
 * after it their division is the Serie D again. The two C2 groups of 2011/12 to 2013/14 hold 36 to 40 clubs, so the
 * C1 and C2 clubs are 72 to 76 then, and the same fill makes 90 (Q529). */
void o_0063dfe0(void);

RT_WRAP(0063dfe0, ita_ccup_entrants)
{
    uint32_t cup = cpu.ecx;
    int need = 90 - clubs_of(KEY_C1A) - clubs_of(KEY_C1B) - clubs_of(KEY_C2A) - clubs_of(KEY_C2B) - clubs_of(KEY_C2C);
    uint32_t serie_d = record_of(KEY_SERIE_D), c2b = record_of(KEY_C2B);
    int y = (int16_t)rd16(g_mem, cup + 0x40);
    if (need <= 0 || !(era_of_year(y) || two_c2_of_year(y)) || !serie_d || !c2b) {
        o_0063dfe0();
        return;
    }
    static uint32_t extra[4096];
    int m = pool(serie_d, extra, 4096);
    qsort(extra, (size_t)m, sizeof *extra, by_reputation);
    if (m > need) m = need;
    for (int i = 0; i < m; i++) wr32(g_mem, extra[i] + 0x57, c2b);
    o_0063dfe0();
    uint32_t eax = cpu.eax;
    for (int i = 0; i < m; i++) wr32(g_mem, extra[i] + 0x57, serie_d);
    cpu.eax = eax;
    fprintf(stderr, "recomp: serie c: Coppa Italia Serie C takes %d clubs of the Serie D\n", m);
}

/* ItaCup::select_entrants 0x63faf0, the two compares with the C1/B id (0x63fc79, 0x63fcf9): in the Serie C era the id
 * of C2/A (group C) gives the C1/B id, so the compare is "equal" and group C's clubs take the cup's lower places as the
 * clubs of groups A and B do. Its second loop goes up through the last positions until it has 32 clubs and has no end
 * (0x63fd22..0x63fd30): with group C left out, a 2014 game with the extra division looped there for ever. */
uint32_t rt_ita_cup_c1b(uint32_t id, uint32_t c1b)
{
    uint32_t serie_a = object_of(KEY_A);
    int y = serie_a ? (int16_t)rd16(g_mem, serie_a + 0x40) : 0;      /* the cup can start before Serie A: y + 1 */
    if (id == rd32(g_mem, KEY_C2A) && serie_a && (era_of_year(y) || era_of_year(y + 1))) return id;
    return c1b;
}

/* ItaCup::select_entrants 0x63fd28 (cmp si, 0x20 after the next position, 0x63fd22..0x63fd30): the list of lower clubs
 * is on the stack at esp + 0x1c (4 bytes a club, 0x63fd12), count clubs so far. In the Serie C era, when the position
 * passes 40 with fewer than 32 clubs, the Italian clubs whose last division (+0x5b) is a group and that are not in
 * the list yet join it by reputation, so the loop ends with 32. Else the count is as it is. Q626. Only the last
 * division counts: the cup's sort 0x640590 reads the stats of that division (a club with none stopped at
 * comp_stats.CPP 1664), and the upper places take clubs whose last division is Serie A or B (0x63fdc0..0x63fef0), so
 * no club comes twice. */
uint32_t rt_ita_cup_fill(uint32_t esp, uint32_t count, uint32_t position)
{
    uint32_t serie_a = object_of(KEY_A);
    int y = serie_a ? (int16_t)rd16(g_mem, serie_a + 0x40) : 0;
    if (count >= 0x20 || position <= 40 || !serie_a || !(era_of_year(y) || era_of_year(y + 1))) return count;
    uint32_t list = esp + 0x1c;
    int nulls = 0;
    for (uint32_t j = 0; j < count; j++) nulls += !readable(rd32(g_mem, list + 4u * j));
    fprintf(stderr, "recomp: serie c: Coppa Italia: %u lower clubs at position %u (%d empty)\n", count, position, nulls);
    static uint32_t cand[4096];
    int n = 0;
    uint32_t table = rd32(g_mem, CLUBS), italy = rd32(g_mem, KEY_ITALY);
    uint32_t rec[GROUPS] = {record_of(KEY_C1A), record_of(KEY_C1B), record_of(KEY_C2A)};
    int clubs = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    for (int i = 0; readable(table) && i < clubs && n < 4096; i++) {
        uint32_t c = table + CLUB_SIZE * (uint32_t)i, nat = rd32(g_mem, c + 0x53), last = rd32(g_mem, c + 0x5b);
        if (!readable(nat) || rd32(g_mem, nat) != italy || !last) continue;
        if (last == rec[0] || last == rec[1] || last == rec[2]) cand[n++] = c;
    }
    qsort(cand, (size_t)n, sizeof *cand, by_reputation);
    for (int i = 0; i < n && count < 0x20; i++) {
        int have = 0;
        for (uint32_t j = 0; j < count && !have; j++) have = rd32(g_mem, list + 4u * j) == cand[i];
        if (!have) wr32(g_mem, list + 4u * count++, cand[i]);
    }
    fprintf(stderr, "recomp: serie c: Coppa Italia: the lower places filled to %u from the groups\n", count);
    return count;
}

void o_0063faf0(void);

/* ItaCup::select_entrants, thiscall (): it takes its places by each club's last division and final place
 * (0x63fbd5..0x63fef0), and only the chain of ItaSerA records them (vtable +0xa4, League::record_final_positions
 * 0x68aa30, at 0x642423..0x642465). The cup can start before Serie A in the season change pass: in a 2014 game
 * with the extra division the Serie A and B clubs had no last division at the 2015 pick, 12 of the 16 upper places
 * stayed empty and the cup's first round stopped at "comp_stats..CPP 1664" (0x4a2553, from 0x640389). So in the
 * Serie C era, when Serie A has not started the cup's season yet, Serie A and B record their final places first
 * (the chain records the same places again), and the groups mark their clubs (mark_last_season). */
RT_WRAP(0063faf0, ita_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    int y = (int16_t)rd16(g_mem, cup + 0x40);
    if (era_of_year(y) || era_of_year(y + 1)) {
        uint32_t saved = cpu.ecx;
        const uint32_t keys[2] = {KEY_A, KEY_B};
        for (int i = 0; i < 2 && !rt_unwinding; i++) {
            uint32_t obj = object_of(keys[i]);
            int oy = obj ? (int16_t)rd16(g_mem, obj + 0x40) : 0;
            if (obj && oy > 0 && oy < y) call_this(rd32(g_mem, rd32(g_mem, obj) + 0xa4), obj, 0, NULL);
        }
        if (!rt_unwinding) mark_last_season();
        if (rt_unwinding) return;
        cpu.ecx = saved;
    }
    o_0063faf0();
}

/* ---- The manager of the year of an ended C2 group ----
 *
 * ItalyAwards::daily_update 0x660ec0 gives the manager of the year of C2/A, C2/B and C2/C with the extra division flag
 * (0x660f5b..0x660fa7), through nation_awards_award_manager_of_year 0x768940 (thiscall (date, award, comp id) ret
 * 0xc), which takes the league's manager by its vtable +0xa0 (0x768a02) from the table. An ended group (year 2999,
 * Q623) keeps the table of its last season, so its last winner's manager got the award again each year. So the
 * award of a league with the year of an ended group is not given. */
void o_00768940(void);

RT_WRAP(00768940, ita_manager_award)
{
    int32_t id = (int32_t)rt_arg(2);
    uint32_t table = rd32(g_mem, COMP_OBJECTS), obj = id >= 0 && readable(table) ? rd32(g_mem, table + 4u * (uint32_t)id) : 0;
    if (readable(obj) && c2_group(obj) >= 1 && (int16_t)rd16(g_mem, obj + 0x40) == IDLE_YEAR) {
        rt_ret_stdcall(cpu.eax, 3);
        return;
    }
    o_00768940();
}
