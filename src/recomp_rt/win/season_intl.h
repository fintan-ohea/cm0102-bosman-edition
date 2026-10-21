/* International competitions in the original (0.6.0 slice 27; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md,
 * Q248, Q276, Q277, Q288, Q366, Q367). Research and evidence: docs/research/2026-10-18-international.md.
 *
 * The editions of the finals come from one table (season_intl_table.c, made by tools/seasons/make_intl.py
 * from the rules table): edition year, season, team count, golden goal, first day and hosts. Slices 28 and 29
 * add their competitions to the same table. */
#ifndef RECOMP_SEASON_INTL_H
#define RECOMP_SEASON_INTL_H

#include <stdint.h>

typedef struct {
    const char *file;       /* source file of the class: world_cup, euro_champ, fifa_confed, olympics, goldcup,
                               african_nations, asia_nations, oceania_nations */
    int season;             /* the season of the rules table row (2003 for 2003/04) */
    int year;               /* the edition year that the class holds at +0x40 */
    int teams;              /* teams in the finals */
    int golden;             /* 1: golden goal in extra time */
    int start_year;         /* the year of the first day of the finals (2025 for the 2026 edition of ACN) */
    int month, day;         /* first day of the finals (month 1 to 12), 0 when the table gives none */
    const char *hosts;      /* host nations: ";" between hosts, "/" between the names a database can use */
    const char *row;        /* id of the rules table row */
} SeasonIntlEdition;

extern const SeasonIntlEdition season_intl_editions[];
extern const int season_intl_edition_count;

/* 1 for a season game: the season code is on, and the start year is not 2001 or changing rules is on. The
 * base game (2001/02, changing rules off) and RECOMP_SEASON_CODE=off give 0. */
int season_intl_on(void);

/* The edition of a class in a year, or NULL. */
const SeasonIntlEdition *season_intl_edition(const char *file, int year);

/* The first edition of a class whose season is the start season or later, or NULL. */
const SeasonIntlEdition *season_intl_first(const char *file, int start_season);

/* The next edition of a class after the edition year, or NULL. */
const SeasonIntlEdition *season_intl_next(const char *file, int year);

/* The calls of the code sites (tools/recomp/seasons.py). Each gives the original's value when
 * season_intl_on() is 0, so the base game is unchanged. */
uint32_t rt_intl_first_year(uint32_t cls, uint32_t year);              /* ctor: year after the original's rule */
uint32_t rt_intl_next_year(uint32_t cls, uint32_t year, uint32_t step); /* start_new_season: year + step */
uint32_t rt_intl_round_flags(uint32_t cls, uint32_t year, uint32_t flags); /* golden goal bit 0x80 */
uint32_t rt_intl_wc_holder(uint32_t holder);       /* a qualifying section's holder: 0 in a season game */
uint32_t rt_intl_wc_holder_slot(uint32_t year);    /* 1 when the World Cup of the year gives the holder a slot */

/* A new game (season.c, before the original makes it): the start of Australia's move to Asia. */
void season_intl_at_create(void);

/* The Oceania section's left-out nation at 0x91fc89 (New Caledonia): none with Australia in Asia. */
uint32_t rt_intl_oceania_left_out(uint32_t id);

/* 1 when a European Championship without a host goes on (seasons.py 0x57be3d): in a season game. */
uint32_t rt_intl_euro_no_host(void);

/* The template function of the European Championship (vtable +0x3c, 0x57ab90) for editions of 24 teams
 * (season_intl_euro.c). The wrap of 0x57ab90 calls it at the entry state with the original o_0057ab90. It
 * completes the call (it calls the original or makes the knockout of 24 itself) and gives the result. */
uint32_t season_intl_euro_template(void (*original)(void));

/* The template function of the African Cup of Nations (cls INTL_ACN, 0x4012e0) and the Asian Cup (INTL_ASIAN,
 * 0x40eb00) for editions of 24 teams (season_intl_cups.c). The wraps call it at the entry state with the
 * original. It completes the call and gives the result. */
uint32_t season_intl_cup_template(uint32_t cls, void (*original)(void));

/* The template function of the Copa America (0x5e0900) for editions of 16 teams (season_intl_copa.c). The wrap
 * calls it at the entry state with the original o_005e0900. It completes the call and gives the result. */
uint32_t season_intl_copa_template(void (*original)(void));

/* The edition whose format a class plays in an edition year: that year's with changing rules on, the start
 * season's with changing rules off (Q232). NULL outside a season game. */
const SeasonIntlEdition *season_intl_rule_edition(const char *file, int year);

/* The days by which the finals of a class in an edition year move, so the first group day is the table's
 * (0 for the base game, an edition the table does not hold, or one without a date). */
int season_intl_date_shift(uint32_t cls, int year);

/* The source file of a class of the code sites ("?" for none). */
const char *season_intl_class_file(uint32_t cls);

/* Moves a date (day 1 to 31, month from 0, year) by shift days. */
void season_intl_shift_date(int shift, int *day, int *month0, int *year);

/* Fills the empty Euro team slots in a season game (season_intl.c): at the seeding and at the draw. */
void season_intl_euro_fill(uint32_t self);

/* The hosts of the fixed host row of a competition (its key id, the value of its key global) and an edition
 * year: up to six nation ids in the order of the table. Gives the count (slice 29: three hosts in 2026). */
int season_intl_host_ids(uint32_t key, int year, int ids[6]);

/* The inter-confederation ties of the World Cup of a year that this run built (season_intl_qual.c): up to max
 * pairs into teams (two teams each). Gives the pair count. */
int season_intl_wc_ties(int year, uint32_t *teams, int max);
void season_intl_qual_reset(void);             /* a load or new game: no ties (load sweep) */

/* The classes of the code sites (cls of the calls above). */
enum {
    INTL_WORLD_CUP, INTL_EURO, INTL_CONFED, INTL_COPA, INTL_ACN, INTL_ASIAN, INTL_OFC, INTL_OLYMPICS,
    INTL_CLASSES
};

#endif
