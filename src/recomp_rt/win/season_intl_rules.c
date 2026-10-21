/* International competitions in the original (0.6.0 slice 27): the editions and the rules of the code sites
 * (season_intl.h). This file reads no game memory, so the unit test tests/seasons/test_season_intl.c builds
 * it with its own season state. Research and evidence: docs/research/2026-10-18-international.md. */
#include "season.h"
#include "season_intl.h"

int season_change_is_creating(void);    /* season_change.c: 1 while create_game 0x828110 runs */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* The source file of each class of the code sites (enum of season_intl.h). */
static const char *const class_files[INTL_CLASSES] = {
    "world_cup", "euro_champ", "fifa_confed", "goldcup", "african_nations", "asia_nations", "oceania_nations",
    "olympics",
};

/* The year that a class holds when the table has no later edition: the object waits and plays no more
 * finals (the Confederations Cup after 2017). No game reaches it.
 * TODO (slice 16 interface): idle the object with season_change_skip() in a before hook instead, once a game
 * run has tested that interface with an international class. */
#define IDLE_YEAR 2200u

/* The last season with a golden goal (int-world: the 2003 Confederations Cup used it, "golden goal ceased in
 * 2004"; Q367: the silver goal of Euro 2004, season 2003/04, becomes full extra time and penalties). */
#define LAST_GOLDEN_SEASON 2002
#define GOLDEN_GOAL 0x80u

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_INTL_LOG");
        on = v && *v == '1';
    }
    return on;
}

int season_intl_on(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

const SeasonIntlEdition *season_intl_edition(const char *file, int year)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (season_intl_editions[i].year == year && !strcmp(season_intl_editions[i].file, file))
            return &season_intl_editions[i];
    return NULL;
}

/* The table is sorted by file, then year (make_intl.py). */
const SeasonIntlEdition *season_intl_first(const char *file, int start_season)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file) && season_intl_editions[i].season >= start_season)
            return &season_intl_editions[i];
    return NULL;
}

const SeasonIntlEdition *season_intl_next(const char *file, int year)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file) && season_intl_editions[i].year > year)
            return &season_intl_editions[i];
    return NULL;
}

static const char *class_file(uint32_t cls)
{
    return cls < INTL_CLASSES ? class_files[cls] : "?";
}

const char *season_intl_class_file(uint32_t cls) { return class_file(cls); }

/* The constructor of a class: the original rounds the game year up to its cycle. A game that starts in
 * another season takes the first edition of the table from its start season. The base game and a 2001 start
 * keep the original's year (the table holds the same editions for 2001/02). */
uint32_t rt_intl_first_year(uint32_t cls, uint32_t year)
{
    if (!season_intl_on() || season_start_year() == SEASON_FIRST_YEAR) return year;
    const SeasonIntlEdition *e = season_intl_first(class_file(cls), season_start_year());
    uint32_t now = e ? (uint32_t)e->year : IDLE_YEAR;
    if (logging())
        fprintf(stderr, "recomp: season intl: %s first edition %u (the original %u)\n", class_file(cls), now,
                year & 0xffffu);
    return now;
}

/* start_new_season of a class: the original adds its step. With changing rules on, the next edition of the
 * table. With changing rules off, the rules of the start season stay: the original's step from the first
 * edition. After the last edition of the table, the object waits (Q248: the rules stay; a competition that
 * ended does not come back). */
uint32_t rt_intl_next_year(uint32_t cls, uint32_t year, uint32_t step)
{
    uint32_t old = year & 0xffffu;
    if (!season_intl_on() || !season_changing_rules()) return old + step;
    const SeasonIntlEdition *e = season_intl_next(class_file(cls), (int)(int16_t)old);
    uint32_t now = e ? (uint32_t)e->year : (old >= IDLE_YEAR ? old : IDLE_YEAR);
    if (logging())
        fprintf(stderr, "recomp: season intl: %s next edition %u after %u (the original %u)\n", class_file(cls),
                now, old, old + step);
    return now;
}

/* The rule flags of a knockout round (sweep-international section 7): bit 0x80 is the golden goal. With
 * changing rules on, the edition of the table decides, or the season of the year for a year that the table
 * has no edition for. With changing rules off, the rule of the start season stays. */
/* 1 for the edition of a class that a game started in 2001/02 builds at the new game: the original's first
 * edition (the table's first edition from 2001/02 is the same year). It keeps the original's dates and round
 * flags, because its stages are made at the new game (Q152: a 2001 game with changing rules on equals the base
 * game at the start). */
static int start_edition_2001(uint32_t cls, int year)
{
    if (season_start_year() != SEASON_FIRST_YEAR || cls >= INTL_CLASSES) return 0;
    const SeasonIntlEdition *e = season_intl_first(class_files[cls], SEASON_FIRST_YEAR);
    return e && e->year == year;
}

uint32_t rt_intl_round_flags(uint32_t cls, uint32_t year, uint32_t flags)
{
    if (!season_intl_on() || !(flags & GOLDEN_GOAL)) return flags;
    if (start_edition_2001(cls, (int)(int16_t)(year & 0xffffu))) return flags;
    int golden;
    if (season_changing_rules()) {
        int y = (int)(int16_t)(year & 0xffffu);
        const SeasonIntlEdition *e = season_intl_edition(class_file(cls), y);
        golden = e ? e->golden : y <= LAST_GOLDEN_SEASON + 1;
    } else {
        golden = season_start_year() <= LAST_GOLDEN_SEASON;
    }
    return golden ? flags : flags & ~GOLDEN_GOAL;
}

/* The first World Cup at which the holder had to qualify (int-world: "2002 is the last World Cup with an
 * automatic place for the holders"). */
#define FIRST_WC_WITHOUT_HOLDER 2006

/* The World Cup holder that a qualifying section leaves out of its pool. In a season game every section plays
 * for the 2006 World Cup or later, or for 2002 without the holder in its pool (season_intl.h, seasons.py). */
uint32_t rt_intl_wc_holder(uint32_t holder)
{
    /* A game started in 2001/02 makes its sections at the new game as the base game does (Q152). */
    if (season_start_year() == SEASON_FIRST_YEAR && season_change_is_creating()) return holder;
    return season_intl_on() ? 0u : holder;
}

/* Slot 0 of the World Cup teams: the holder, to 2002. */
uint32_t rt_intl_wc_holder_slot(uint32_t year)
{
    return !season_intl_on() || (int)(int16_t)(year & 0xffffu) < FIRST_WC_WITHOUT_HOLDER;
}

/* ---- Finals dates ---- */

/* The first group day of each class in its template function (vtable +0x3c), the first call of
 * League_Stage date 0x68a160 there: day, then the month from 0 (read at each address). */
static const struct { int day, month0; } class_first[INTL_CLASSES] = {
    {12, 5},    /* World Cup 12 June: 0x92c335 */
    {9, 5},     /* European Championship 9 June: 0x57ac35 */
    {29, 6},    /* Confederations Cup 29 July: 0x58b601 */
    {6, 6},     /* Copa America 6 July: 0x5e09a5 */
    {8, 1},     /* African Cup of Nations 8 February: 0x401385 */
    {13, 9},    /* Asian Cup 13 October: 0x40eba5 */
    {10, 10},   /* Oceania Nations Cup 10 November when the year % 4 is 2 (0x798b70, 0x798c23), else 19 June */
    {13, 8},    /* Olympics 13 September: 0x79f875 */
};

static long days_from_civil(long y, int m, int d)    /* m 1 to 12; days from 1 January 1970 */
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* The edition whose format a class plays in an edition year. With changing rules on, the edition of that year.
 * With changing rules off, the rules of the start season stay (Q232): the first edition from the start season
 * gives the size, the golden goal and the time of year of every edition. NULL outside a season game. */
const SeasonIntlEdition *season_intl_rule_edition(const char *file, int year)
{
    if (!season_intl_on()) return NULL;
    if (season_changing_rules()) return season_intl_edition(file, year);
    return season_intl_first(file, season_start_year());
}

int season_intl_date_shift(uint32_t cls, int year)
{
    if (!season_intl_on() || cls >= INTL_CLASSES) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition(class_files[cls], year);
    if (!e || e->month < 1) return 0;
    /* A game that starts in 2001/02 keeps the dates of the editions it builds at the new game (Q152: the World
     * Cup 2002, the African Cup of Nations 2002, the Confederations Cup 2003, Euro 2004 and the others of a 2001
     * game with changing rules on play as in the base game). */
    if (start_edition_2001(cls, year)) return 0;
    int day = class_first[cls].day, month0 = class_first[cls].month0;
    if (cls == INTL_OFC && year % 4 != 2) {
        day = 19;
        month0 = 5;
    }
    long from = days_from_civil(year, month0 + 1, day);
    long to = days_from_civil(year + (e->start_year - e->year), e->month, e->day > 0 ? e->day : 1);
    return (int)(to - from);
}

void season_intl_shift_date(int shift, int *day, int *month0, int *year)
{
    if (!shift) return;
    int y, m, d;
    civil_from_days(days_from_civil(*year, *month0 + 1, *day) + shift, &y, &m, &d);
    *day = d;
    *month0 = m - 1;
    *year = y;
}

/* ---- Nation records ---- */

/* The renamed nations. A nation record (nation.dat, 0x122 bytes) holds the long name at +4 (51 bytes), the short
 * name at +0x38 (26), the code at +0x53 (4) and the adjective at +0x57 (26; the continent follows at +0x71), read
 * from the 3.9.68 nation.dat (Yugoslavia, row 209). Every season database keeps the record "Yugoslavia" and adds
 * Serbia, Montenegro and North Macedonia as new records when it has them (the nation.dat of the 49 databases),
 * so a rename applies only when the data holds the old name and no record with a later name. */
const SeasonIntlRename season_intl_renames[] = {
    {2002, "Yugoslavia", "Serbia and Montenegro", "Serbia & Montenegro", "SCG", "Serbian", "int-world-13"},
    {2006, "Serbia and Montenegro", "Serbia", "Serbia", "SRB", "Serbian", "int-world-61"},
    {2016, "Czech Republic", "Czechia", "Czechia", "CZE", "Czech", "int-world-181"},
    {2017, "Swaziland", "Eswatini", "Eswatini", "SWZ", "Swazi", "int-world-193"},
    {2018, "FYR of Macedonia", "North Macedonia", "North Macedonia", "MKD", "Macedonian", "int-world-205"},
    {2022, "Turkey", "T\xfcrkiye", "T\xfcrkiye", "TUR", "Turkish", "int-world-253"},
};
const int season_intl_rename_count = (int)(sizeof season_intl_renames / sizeof season_intl_renames[0]);

int season_intl_later_held(int i, int (*held)(const char *name, void *ctx), void *ctx)
{
    if (i < 0 || i >= season_intl_rename_count) return 0;
    const char *n = season_intl_renames[i].name;
    for (int step = 0; n && step <= season_intl_rename_count; step++) {
        if (held(n, ctx)) return 1;
        const char *next = NULL;
        for (int k = 0; k < season_intl_rename_count; k++)
            if (!strcasecmp(season_intl_renames[k].old, n)) next = season_intl_renames[k].name;
        n = next;
    }
    return 0;
}

int season_intl_uefa_from(const char *name)
{
    /* UEFA Congress of 3 May 2016 (docs/research/rules-0.6.0/uefa-access.tsv, the 2016/17 rows: "Kosovo (55):
     * admitted May 2016"); the 2018-19 Nations League has Kosovo in League D (nations-league-tiers.tsv). */
    return name && !strcasecmp(name, "Kosovo") ? 2016 : 0;
}
