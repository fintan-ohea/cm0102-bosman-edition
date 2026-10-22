/* Test of the editions of the international finals (0.6.0 slice 27, src/recomp_rt/win/season_intl_rules.c and
 * the table season_intl_table.c that tools/seasons/make_intl.py makes).
 *
 * Usage: season_intl_test
 * The test gives the season state itself (start year, changing rules, season code). The checks: the base game
 * and RECOMP_SEASON_CODE=off keep the original's years and flags; the first edition of a later start season;
 * the next edition with changing rules on and the original's step with it off; the Confederations Cup waits
 * after 2017; the Tokyo Olympics in 2020 (Q366); the golden goal to the 2003 Confederations Cup and none at
 * Euro 2004 (Q367); the hosts of some editions. */
#include "season.h"
#include "season_intl.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* The season state of the test (season.c in the game). */
static int code_on = 1, start_year = SEASON_FIRST_YEAR, changing = 0;
int season_code_on(void) { return code_on; }
int season_start_year(void) { return start_year; }
int season_changing_rules(void) { return changing; }
static int creating;
int season_change_is_creating(void) { return creating; }

static void game(int on, int year, int rules)
{
    code_on = on;
    start_year = year;
    changing = rules;
}

/* The nation names of a database for season_intl_later_held (a list that ends with NULL). */
static int held_in(const char *name, void *ctx)
{
    for (const char *const *n = ctx; *n; n++)
        if (!strcasecmp(*n, name)) return 1;
    return 0;
}

static int rename_row(const char *old)
{
    for (int i = 0; i < season_intl_rename_count; i++)
        if (!strcmp(season_intl_renames[i].old, old)) return i;
    return -1;
}

int main(void)
{
    /* The base game and the season code off: the original's values. */
    game(1, 2001, 0);
    CHECK(!season_intl_on(), "the base game is not a season game");
    CHECK(rt_intl_first_year(INTL_COPA, 2001) == 2001, "base game: Copa America year");
    CHECK(rt_intl_next_year(INTL_CONFED, 2017, 2) == 2019, "base game: Confederations Cup step");
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x383, "base game: golden goal stays");
    CHECK(rt_intl_wc_holder(0x1234) == 0x1234, "base game: the holder is exempt");
    CHECK(rt_intl_wc_holder_slot(2006), "base game: the holder has slot 0");
    game(0, 2010, 1);
    CHECK(!season_intl_on(), "season code off");
    CHECK(rt_intl_first_year(INTL_ACN, 2012) == 2012, "season code off: ACN year");
    CHECK(rt_intl_next_year(INTL_ACN, 2012, 2) == 2014, "season code off: ACN step");

    /* A 2001 start with changing rules on: the first editions are the original's, the steps the table's. */
    game(1, 2001, 1);
    CHECK(season_intl_on(), "2001 with changing rules on is a season game");
    CHECK(rt_intl_first_year(INTL_EURO, 2004) == 2004, "2001 on: Euro first edition stays");
    CHECK(rt_intl_next_year(INTL_CONFED, 2003, 2) == 2005, "2001 on: Confederations Cup 2005");
    CHECK(rt_intl_next_year(INTL_CONFED, 2005, 2) == 2009, "2001 on: Confederations Cup 2009");
    CHECK(rt_intl_next_year(INTL_CONFED, 2017, 2) == 2200, "2001 on: no Confederations Cup after 2017");
    CHECK(rt_intl_next_year(INTL_CONFED, 2200, 2) == 2200, "2001 on: the waiting object stays");
    CHECK(rt_intl_next_year(INTL_COPA, 2001, 2) == 2004, "2001 on: Copa America 2004");
    CHECK(rt_intl_next_year(INTL_COPA, 2015, 2) == 2016, "2001 on: Copa America Centenario");
    CHECK(rt_intl_next_year(INTL_ASIAN, 2004, 4) == 2007, "2001 on: Asian Cup 2007");
    CHECK(rt_intl_next_year(INTL_ACN, 2012, 2) == 2013, "2001 on: ACN moves to odd years");
    CHECK(rt_intl_next_year(INTL_ACN, 2023, 2) == 2026, "2001 on: ACN 2025 plays in January 2026");
    CHECK(rt_intl_next_year(INTL_OLYMPICS, 2016, 4) == 2020, "2001 on: Olympics 2020 (Q366)");
    CHECK(rt_intl_next_year(INTL_WORLD_CUP, 2018, 4) == 2022, "2001 on: World Cup 2022");
    CHECK(rt_intl_next_year(INTL_OFC, 2016, 2) == 2024, "2001 on: no OFC Nations Cup in 2020");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2002, 0x383) == 0x383, "2001 on: golden goal in 2002");
    CHECK(rt_intl_round_flags(INTL_CONFED, 2003, 0x83) == 0x83, "2001 on: golden goal in the 2003 Confed Cup");
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x303, "2001 on: no golden goal at Euro 2008");
    CHECK(rt_intl_round_flags(INTL_CONFED, 2005, 0x83) == 0x3, "2001 on: no golden goal in 2005");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2002, 0x3) == 0x3, "2001 on: a round without golden goal");

    CHECK(rt_intl_wc_holder(0x1234) == 0, "2001 on: no holder exemption in the sections");
    CHECK(rt_intl_wc_holder_slot(2002), "2001 on: the holder has slot 0 in 2002");
    CHECK(!rt_intl_wc_holder_slot(2006), "2001 on: no holder slot in 2006");

    /* A later start: the first edition from the start season. */
    game(1, 2005, 0);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2006) == 2006, "2005: World Cup 2006");
    CHECK(rt_intl_first_year(INTL_CONFED, 2005) == 2009, "2005: the 2005 Confederations Cup lies before");
    CHECK(rt_intl_first_year(INTL_ASIAN, 2008) == 2007, "2005: Asian Cup 2007");
    CHECK(rt_intl_first_year(INTL_COPA, 2005) == 2007, "2005: Copa America 2007");
    CHECK(rt_intl_first_year(INTL_ACN, 2006) == 2006, "2005: ACN 2006");
    CHECK(rt_intl_next_year(INTL_COPA, 2007, 2) == 2009, "2005 off: the original's step");
    CHECK(rt_intl_round_flags(INTL_WORLD_CUP, 2006, 0x383) == 0x303, "2005 off: no golden goal");
    game(1, 2002, 0);
    CHECK(rt_intl_round_flags(INTL_EURO, 2008, 0x383) == 0x383, "2002 off: the golden goal of 2002/03 stays");
    game(1, 2016, 1);
    CHECK(rt_intl_first_year(INTL_EURO, 2016) == 2020, "2016: Euro 2016 lies before the start");
    CHECK(rt_intl_first_year(INTL_COPA, 2017) == 2019, "2016: Copa America 2019");
    CHECK(rt_intl_first_year(INTL_ACN, 2016) == 2017, "2016: ACN 2017");
    CHECK(rt_intl_first_year(INTL_OLYMPICS, 2016) == 2016, "2016: Olympics in August 2016");
    game(1, 2018, 1);
    CHECK(rt_intl_first_year(INTL_CONFED, 2019) == 2200, "2018: no Confederations Cup");
    game(1, 2026, 1);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2026) == 2030, "2026: the 2030 World Cup (slice 29, Q560)");
    game(1, 2030, 1);
    CHECK(rt_intl_first_year(INTL_WORLD_CUP, 2030) == 2200, "2030: no World Cup in the table after 2030");

    /* Finals dates. */
    game(1, 2001, 0);
    CHECK(season_intl_date_shift(INTL_ACN, 2019) == 0, "base game: no date shift");
    game(1, 2010, 1);
    CHECK(season_intl_date_shift(INTL_ACN, 2019) == 133, "ACN 2019: 8 February to 21 June");
    CHECK(season_intl_date_shift(INTL_ACN, 2026) == -49, "ACN 2025/26: 8 February 2026 to 21 December 2025");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2022) == 161, "World Cup 2022: 12 June to 20 November");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2010) == -1, "World Cup 2010: 12 June to 11 June");
    CHECK(season_intl_date_shift(INTL_ASIAN, 2011) == -279, "Asian Cup 2011: 13 October to 7 January");
    CHECK(season_intl_date_shift(INTL_OFC, 2012) == -18, "OFC 2012: 19 June (2012 %% 4 is 0) to 1 June");
    CHECK(season_intl_date_shift(INTL_OFC, 2008) == 0, "OFC 2008: no date in the table");
    CHECK(season_intl_date_shift(INTL_COPA, 2009) == 0, "Copa America 2009: not an edition of the table");
    game(1, 2001, 1);
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2002) == 0, "2001 on: the 2002 World Cup keeps its dates");
    CHECK(season_intl_date_shift(INTL_ACN, 2002) == 0, "2001 on: the ACN 2002 keeps its dates");
    CHECK(season_intl_date_shift(INTL_ACN, 2004) != 0, "2001 on: the ACN 2004 moves to the table's dates");
    CHECK(season_intl_date_shift(INTL_CONFED, 2003) == 0, "2001 on: the Confederations Cup 2003 keeps its dates");
    CHECK(rt_intl_round_flags(INTL_EURO, 2004, 0x383) == 0x383, "2001 on: Euro 2004, built at the new game, keeps its flags");
    creating = 1;
    CHECK(rt_intl_wc_holder(0x1234) == 0x1234, "2001 on: the sections made at the new game keep the holder");
    creating = 0;
    game(1, 2010, 0);
    CHECK(season_intl_rule_edition("euro_champ", 2016)->teams == 16, "2010 off: Euro 2016 keeps 16 teams (Q232)");
    CHECK(season_intl_date_shift(INTL_WORLD_CUP, 2022) == 0, "2010 off: World Cup 2022 in June, as 2014 (the first edition from 2010)");
    game(1, 2015, 0);
    CHECK(season_intl_rule_edition("euro_champ", 2020)->teams == 24, "2015 off: Euro 2020 with 24 teams");
    game(1, 2010, 1);
    CHECK(season_intl_rule_edition("euro_champ", 2016)->teams == 24, "2010 on: Euro 2016 with 24 teams");
    int d = 8, m = 1, y = 2026;
    season_intl_shift_date(-49, &d, &m, &y);
    CHECK(d == 21 && m == 11 && y == 2025, "shift into the year before: %d.%d.%d", d, m + 1, y);
    d = 28, m = 1, y = 2020;
    season_intl_shift_date(1, &d, &m, &y);
    CHECK(d == 29 && m == 1 && y == 2020, "leap day");

    /* The table. */
    const SeasonIntlEdition *e = season_intl_edition("olympics", 2020);
    CHECK(e && e->season == 2020 && e->month == 7 && !strcmp(e->hosts, "Japan"), "Tokyo Olympics in 2020/21");
    CHECK(!season_intl_edition("olympics", 2021), "no Olympics in 2021");
    e = season_intl_edition("euro_champ", 2008);
    CHECK(e && !strcmp(e->hosts, "Austria;Switzerland") && e->teams == 16, "Euro 2008 hosts");
    e = season_intl_edition("euro_champ", 2020);
    CHECK(e && !*e->hosts && e->teams == 24, "Euro 2020: no host places, 24 teams");
    e = season_intl_edition("world_cup", 2010);
    CHECK(e && !strcmp(e->hosts, "South Africa") && e->month == 6 && e->day == 11, "World Cup 2010");
    e = season_intl_edition("asia_nations", 2004);
    CHECK(e && !strcmp(e->hosts, "China PR/China"), "Asian Cup 2004 host names");
    for (int i = 1; i < season_intl_edition_count; i++) {
        const SeasonIntlEdition *a = &season_intl_editions[i - 1], *b = &season_intl_editions[i];
        int c = strcmp(a->file, b->file);
        CHECK(c < 0 || (c == 0 && a->year < b->year), "table order at %s", b->row);
    }

    /* Nation records (the names of the databases' nation.dat, O188). October 2022: a dead Yugoslavia (continent
     * Europe) beside Serbia, dead FYR of Macedonia and Swaziland beside North Macedonia and eSwatini. */
    static const char *const oct2022[] = {"Yugoslavia", "Serbia", "Montenegro", "FYR of Macedonia",
                                          "North Macedonia", "Swaziland", "eSwatini", "Czech Republic", "Turkey",
                                          "Kosovo", NULL};
    CHECK(season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)oct2022), "October 2022: Yugoslavia dead");
    CHECK(season_intl_later_held(rename_row("FYR of Macedonia"), held_in, (void *)oct2022), "October 2022: FYR dead");
    CHECK(season_intl_later_held(rename_row("Swaziland"), held_in, (void *)oct2022),
          "October 2022: Swaziland dead beside eSwatini (case)");
    CHECK(!season_intl_later_held(rename_row("Czech Republic"), held_in, (void *)oct2022), "Czech Republic is live");
    CHECK(!season_intl_later_held(rename_row("Turkey"), held_in, (void *)oct2022), "Turkey is live");
    /* March 2014: Yugoslavia is the Serbian side beside a new Montenegro; it is renamed, not dead. */
    static const char *const mar2014[] = {"Yugoslavia", "Montenegro", "FYR of Macedonia", "Swaziland", NULL};
    CHECK(!season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)mar2014), "March 2014: Yugoslavia live");
    CHECK(!season_intl_later_held(rename_row("Serbia and Montenegro"), held_in, (void *)mar2014), "no S&M chain");
    /* September 2011 ODB: a dead Yugoslavia beside Serbia, two steps down the chain. */
    static const char *const sep2011[] = {"Yugoslavia", "Serbia", "Montenegro", NULL};
    CHECK(season_intl_later_held(rename_row("Yugoslavia"), held_in, (void *)sep2011), "September 2011: dead");
    CHECK(!season_intl_later_held(-1, held_in, (void *)sep2011), "no row");
    /* Kosovo: UEFA from 2016/17 (no continent in the April 2015, October 2015 and April 2016 databases). */
    CHECK(season_intl_uefa_from("Kosovo") == 2016, "Kosovo joins UEFA in 2016");
    CHECK(!season_intl_uefa_from("Gibraltar") && !season_intl_uefa_from("Serbia") && !season_intl_uefa_from(NULL),
          "no other nation");

    if (failures) {
        fprintf(stderr, "season_intl_test: %d failures\n", failures);
        return 1;
    }
    printf("season_intl_test: all checks pass\n");
    return 0;
}
