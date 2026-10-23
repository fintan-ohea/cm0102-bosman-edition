/* Test of the Nations League rules (0.6.0 slice 28, src/recomp_rt/win/season_nl_rules.c and the table
 * season_nl_table.c that tools/seasons/make_nl.py makes).
 *
 * Usage: season_nl_test
 * The checks: the editions of the table; the real leagues of each edition hold the sizes of the edition; the
 * leagues of 2020-21 from the final places of 2018-19 (the real 2020-21 leagues come out); relegation from
 * 2020-21; the four Euro 2020 play-off paths and their semi-finals from the 2018-19 ranking (the real paths
 * and pairs come out). The points, goal differences and goals of the rows below are test values that give the
 * real order of the overall ranking; only the group places are the real ones. The names are those of the game's
 * nation.dat (3.9.68 data), as the table writes them. */
#include "season_nl.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* The nations of the test, the id is the index. */
static const char *const names[] = {
    /* League A 2018-19 */
    "Holland", "France", "Germany", "Switzerland", "Belgium", "Iceland", "Portugal", "Italy", "Poland",
    "England", "Spain", "Croatia",
    /* League B */
    "Ukraine", "Czech Republic", "Slovakia", "Sweden", "Russia", "Turkey", "Bosnia-Herzegovina", "Austria",
    "Northern Ireland", "Denmark", "Wales", "Republic of Ireland",
    /* League C */
    "Scotland", "Israel", "Albania", "Finland", "Hungary", "Greece", "Estonia", "Norway", "Bulgaria", "Cyprus",
    "Slovenia", "Serbia", "Romania", "Montenegro", "Lithuania",
    /* League D */
    "Georgia", "Kazakhstan", "Latvia", "Andorra", "Belarus", "Luxembourg", "Moldova", "San Marino", "Kosovo",
    "Azerbaijan", "Faroe Islands", "Malta", "FYR of Macedonia", "Armenia", "Gibraltar", "Liechtenstein",
};
#define NATIONS ((int)(sizeof names / sizeof names[0]))

static int id(const char *name)
{
    for (int i = 0; i < NATIONS; i++)
        if (!strcmp(names[i], name)) return i;
    fprintf(stderr, "test: no nation %s\n", name);
    return -1;
}

/* The final places of the 2018-19 UEFA Nations League (league, group, place). Points: test values (see
 * above). */
typedef struct { const char *nation; int league, group, pos, points; } Result;
static const Result results_2018[] = {
    {"Holland", 0, 1, 1, 7}, {"France", 0, 1, 2, 5}, {"Germany", 0, 1, 3, 2},
    {"Switzerland", 0, 2, 1, 9}, {"Belgium", 0, 2, 2, 9}, {"Iceland", 0, 2, 3, 0},
    {"Portugal", 0, 3, 1, 8}, {"Italy", 0, 3, 2, 5}, {"Poland", 0, 3, 3, 2},
    {"England", 0, 4, 1, 7}, {"Spain", 0, 4, 2, 6}, {"Croatia", 0, 4, 3, 4},
    {"Ukraine", 1, 1, 1, 9}, {"Czech Republic", 1, 1, 2, 6}, {"Slovakia", 1, 1, 3, 3},
    {"Sweden", 1, 2, 1, 7}, {"Russia", 1, 2, 2, 7}, {"Turkey", 1, 2, 3, 3},
    {"Bosnia-Herzegovina", 1, 3, 1, 10}, {"Austria", 1, 3, 2, 7}, {"Northern Ireland", 1, 3, 3, 0},
    {"Denmark", 1, 4, 1, 8}, {"Wales", 1, 4, 2, 6}, {"Republic of Ireland", 1, 4, 3, 2},
    {"Scotland", 2, 1, 1, 9}, {"Israel", 2, 1, 2, 8}, {"Albania", 2, 1, 3, 3},
    {"Finland", 2, 2, 1, 12}, {"Hungary", 2, 2, 2, 6}, {"Greece", 2, 2, 3, 9}, {"Estonia", 2, 2, 4, 2},
    {"Norway", 2, 3, 1, 8}, {"Bulgaria", 2, 3, 2, 7}, {"Cyprus", 2, 3, 3, 5}, {"Slovenia", 2, 3, 4, 3},
    {"Serbia", 2, 4, 1, 7}, {"Romania", 2, 4, 2, 5}, {"Montenegro", 2, 4, 3, 7}, {"Lithuania", 2, 4, 4, 0},
    {"Georgia", 3, 1, 1, 16}, {"Kazakhstan", 3, 1, 2, 6}, {"Latvia", 3, 1, 3, 4}, {"Andorra", 3, 1, 4, 3},
    {"Belarus", 3, 2, 1, 12}, {"Luxembourg", 3, 2, 2, 11}, {"Moldova", 3, 2, 3, 9}, {"San Marino", 3, 2, 4, 1},
    {"Kosovo", 3, 3, 1, 14}, {"Azerbaijan", 3, 3, 2, 10}, {"Faroe Islands", 3, 3, 3, 4}, {"Malta", 3, 3, 4, 2},
    {"FYR of Macedonia", 3, 4, 1, 16}, {"Armenia", 3, 4, 2, 10}, {"Gibraltar", 3, 4, 3, 6},
    {"Liechtenstein", 3, 4, 4, 2},
};
#define RESULTS ((int)(sizeof results_2018 / sizeof results_2018[0]))

/* The nations that qualified for Euro 2020 from the qualifying groups (UEFA Euro 2020 qualifying, groups A to
 * J: winners and runners-up). */
static const char *const qualified_2020[] = {
    "England", "Czech Republic", "Ukraine", "Portugal", "Germany", "Holland", "Switzerland", "Denmark",
    "Croatia", "Wales", "Spain", "Sweden", "Poland", "Austria", "France", "Turkey", "Belgium", "Russia", "Italy",
    "Finland",
};

static const SeasonNlEdition *edition(const char *confed, const char *name)
{
    for (int i = 0; i < season_nl_edition_count; i++)
        if (!strcmp(season_nl_editions[i].confed, confed) && !strcmp(season_nl_editions[i].edition, name))
            return &season_nl_editions[i];
    return NULL;
}

static int load_rows(SeasonNlRow *rows)
{
    for (int i = 0; i < RESULTS; i++) {
        const Result *r = &results_2018[i];
        rows[i] = (SeasonNlRow){id(r->nation), r->league, r->group, r->pos, 4, r->points, 0, 0, 0};
        for (size_t q = 0; q < sizeof qualified_2020 / sizeof qualified_2020[0]; q++)
            if (!strcmp(qualified_2020[q], r->nation)) rows[i].qualified = 1;
    }
    return RESULTS;
}

/* The league of a nation in the table's real leagues of an edition, -1 when it is not in them. */
static int real_league(const char *confed, const char *ed, const char *nation)
{
    for (int i = 0; i < season_nl_tier_count; i++) {
        const SeasonNlTier *t = &season_nl_tiers[i];
        if (strcmp(t->confed, confed) || strcmp(t->edition, ed)) continue;
        const char *n = t->nation;
        size_t len = strlen(nation);
        for (;;) {
            if (!strncmp(n, nation, len) && (n[len] == 0 || n[len] == '/')) return t->league;
            n = strchr(n, '/');
            if (!n) break;
            n++;
        }
    }
    return -1;
}

static void check_table(void)
{
    CHECK(season_nl_edition_count >= 10, "the table holds the UEFA and CONCACAF editions (%d)",
          season_nl_edition_count);
    const SeasonNlEdition *e = edition("UEFA", "2018-19");
    CHECK(e && e->season == 2018 && e->leagues == 4, "UEFA 2018-19: season 2018, four leagues");
    if (e) CHECK(e->size[0] == 12 && e->size[1] == 12 && e->size[2] == 15 && e->size[3] == 16,
                 "UEFA 2018-19: 12, 12, 15, 16");
    e = edition("UEFA", "2020-21");
    if (e) CHECK(e->size[0] == 16 && e->size[1] == 16 && e->size[2] == 16 && e->size[3] == 7,
                 "UEFA 2020-21: 16, 16, 16, 7");
    CHECK(e && e->down_last[0] && e->down_last[1], "UEFA 2020-21: the last of the A and B groups go down");
    e = edition("UEFA", "2024-25");
    if (e) CHECK(e->size[3] == 6 && e->groups[3] == 2, "UEFA 2024-25: league D of 6 in 2 groups");
    CHECK(season_nl_first("UEFA", 2001) == edition("UEFA", "2018-19"), "the first UEFA edition from 2001");
    CHECK(season_nl_first("UEFA", 2019) == edition("UEFA", "2020-21"), "the first UEFA edition from 2019/20");
    CHECK(season_nl_first("UEFA", 2027) == NULL, "no UEFA edition from 2027/28 (Q248)");
    CHECK(season_nl_after(edition("UEFA", "2024-25")) == edition("UEFA", "2026-27"), "UEFA after 2024-25");
    CHECK(season_nl_first("CONCACAF", 2018) && season_nl_first("CONCACAF", 2018)->season == 2019,
          "the first CONCACAF league phase is 2019/20 (the qualifying of 2018/19 sets the leagues)");
    CHECK(edition("UEFA", "2018-19") && real_league("UEFA", "2018-19", "Iceland") == 0, "Iceland in A 2018-19");
    /* Every edition's real leagues hold the edition's sizes. */
    for (int i = 0; i < season_nl_edition_count; i++) {
        const SeasonNlEdition *d = &season_nl_editions[i];
        int count[NL_MAX_LEAGUES] = {0};
        int any = 0;
        for (int t = 0; t < season_nl_tier_count; t++) {
            const SeasonNlTier *x = &season_nl_tiers[t];
            if (strcmp(x->confed, d->confed) || strcmp(x->edition, d->edition)) continue;
            any = 1;
            if (x->league >= 0 && x->league < NL_MAX_LEAGUES) count[x->league]++;
        }
        if (!any) continue;
        for (int l = 0; l < d->leagues; l++)
            CHECK(count[l] == d->size[l], "%s %s league %c: %d nations in the table, the edition gives %d",
                  d->confed, d->edition, 'A' + l, count[l], d->size[l]);
    }
}

static void check_promotion(void)
{
    const SeasonNlEdition *from = edition("UEFA", "2018-19"), *to = edition("UEFA", "2020-21");
    if (!from || !to) { CHECK(0, "no 2018-19 or 2020-21 edition"); return; }
    SeasonNlRow rows[NL_MAX_NATIONS];
    int next[NL_MAX_NATIONS];
    int n = load_rows(rows);
    CHECK(season_nl_next_leagues(from, to, rows, n, next) == n, "the 2020-21 leagues are made");
    int size[NL_MAX_LEAGUES] = {0};
    for (int i = 0; i < n; i++) {
        size[next[i]]++;
        const char *nation = names[rows[i].nation];
        int want = real_league("UEFA", "2020-21", nation);
        CHECK(next[i] == want, "2020-21: %s in league %c, the real league is %c", nation, 'A' + next[i],
              want < 0 ? '?' : 'A' + want);
    }
    CHECK(size[0] == 16 && size[1] == 16 && size[2] == 16 && size[3] == 7, "2020-21 sizes %d %d %d %d", size[0],
          size[1], size[2], size[3]);

    /* 2020-21 to 2022-23: the last of each A and B group go down, the B, C and D winners go up. Each league here
     * is 4 groups of 4 (D: 2 groups); the places follow the nation order of each league. */
    from = to;
    to = edition("UEFA", "2022-23");
    if (!to) { CHECK(0, "no 2022-23 edition"); return; }
    n = 0;
    for (int l = 0; l < 4; l++) {
        int k = 0;
        for (int i = 0; i < NATIONS && n < NL_MAX_NATIONS; i++) {
            if (real_league("UEFA", "2020-21", names[i]) != l) continue;
            int groups = from->groups[l];
            rows[n] = (SeasonNlRow){i, l, 1 + k % groups, 1 + k / groups, 6, 20 - k, 0, 0, 0};
            k++;
            n++;
        }
    }
    CHECK(season_nl_next_leagues(from, to, rows, n, next) == n, "the 2022-23 leagues are made");
    int up = 0, down = 0;
    for (int i = 0; i < n; i++) {
        if (next[i] < rows[i].league) up++;
        if (next[i] > rows[i].league) down++;
        if (rows[i].league == 0 && rows[i].pos == 4) CHECK(next[i] == 1, "the last of an A group goes down");
        if (rows[i].league == 1 && rows[i].pos == 1) CHECK(next[i] == 0, "a B group winner goes up");
    }
    CHECK(up == down, "as many up as down (%d, %d)", up, down);
    for (int l = 0; l < 4; l++) {
        int c = 0;
        for (int i = 0; i < n; i++) c += next[i] == l;
        CHECK(c == to->size[l], "2022-23 league %c: %d, the edition gives %d", 'A' + l, c, to->size[l]);
    }
}

static void check_paths(void)
{
    SeasonNlRow rows[NL_MAX_NATIONS];
    int n = load_rows(rows);
    season_nl_rank(rows, n);
    CHECK(rows[0].league == 0 && rows[n - 1].league == 3, "the ranking puts league A first");
    int out[16];
    CHECK(season_nl_paths(rows, n, 4, 4, out) == 16, "16 nations in the Euro 2020 paths");
    static const char *const want[4][4] = {
        {"Iceland", "Bulgaria", "Hungary", "Romania"},
        {"Bosnia-Herzegovina", "Slovakia", "Republic of Ireland", "Northern Ireland"},
        {"Scotland", "Norway", "Serbia", "Israel"},
        {"Georgia", "FYR of Macedonia", "Kosovo", "Belarus"},
    };
    for (int p = 0; p < 4; p++)
        for (int k = 0; k < 4; k++)
            CHECK(out[p * 4 + k] == id(want[p][k]), "path %c place %d: %s, the real one is %s", 'A' + p, k + 1,
                  out[p * 4 + k] >= 0 ? names[out[p * 4 + k]] : "none", want[p][k]);
    int pairs[4];
    season_nl_path_pairs(&out[0], pairs);
    CHECK(pairs[0] == id("Iceland") && pairs[1] == id("Romania") && pairs[2] == id("Bulgaria") &&
          pairs[3] == id("Hungary"), "path A: Iceland v Romania, Bulgaria v Hungary");
    season_nl_path_pairs(&out[4], pairs);
    CHECK(pairs[0] == id("Bosnia-Herzegovina") && pairs[1] == id("Northern Ireland") &&
          pairs[2] == id("Slovakia") && pairs[3] == id("Republic of Ireland"),
          "path B: Bosnia and Herzegovina v Northern Ireland, Slovakia v Republic of Ireland");

    /* A path that its league and the ranking cannot fill keeps -1. */
    CHECK(season_nl_paths(&rows[n - 3], 3, 2, 4, out) == 3 && out[2] >= 0 && out[3] == -1 && out[4] == -1,
          "too few nations: -1");
}

/* Part b: the CONCACAF moves with test places (nation ids 100 on, group places in order). 2022-23 to 2023-24:
 * no one goes down, the B and C winners go up, so the leagues grow to 16, 16 and 9. 2023-24 to 2024-25 (League A
 * of 4 groups of 4, decision 11): the last of each A and B group go down, the B and C winners and the best C
 * runner-up go up. */
static int concacaf_rows(const SeasonNlEdition *e, SeasonNlRow *rows)
{
    int n = 0, nation = 100;
    for (int l = 0; l < e->leagues; l++)
        for (int k = 0; k < e->size[l]; k++) {
            int g = k % e->groups[l], pos = k / e->groups[l];
            rows[n++] = (SeasonNlRow){nation++, l, g + 1, pos + 1, 4, 12 - 3 * pos - g % 2, 0, 0, 0};
        }
    return n;
}

static void check_concacaf(void)
{
    const char *pairs[][2] = {{"2022-23", "2023-24"}, {"2023-24", "2024-25"}};
    for (int p = 0; p < 2; p++) {
        const SeasonNlEdition *from = edition("CONCACAF", pairs[p][0]), *to = edition("CONCACAF", pairs[p][1]);
        if (!from || !to) { CHECK(0, "no CONCACAF %s or %s", pairs[p][0], pairs[p][1]); continue; }
        SeasonNlRow rows[NL_MAX_NATIONS];
        int next[NL_MAX_NATIONS], size[NL_MAX_LEAGUES] = {0}, up = 0, down = 0;
        int n = concacaf_rows(from, rows);
        CHECK(season_nl_next_leagues(from, to, rows, n, next) == n, "CONCACAF %s: the leagues are made", pairs[p][1]);
        for (int i = 0; i < n; i++) {
            size[next[i]]++;
            up += next[i] < rows[i].league;
            down += next[i] > rows[i].league;
            CHECK(next[i] >= rows[i].league - 1 && next[i] <= rows[i].league + 1, "CONCACAF %s: a move of one league",
                  pairs[p][1]);
        }
        CHECK(size[0] == 16 && size[1] == 16 && size[2] == 9, "CONCACAF %s sizes %d %d %d", pairs[p][1], size[0],
              size[1], size[2]);
        CHECK(p == 0 ? up == 8 && down == 0 : up == 8 && down == 8, "CONCACAF %s: %d up, %d down", pairs[p][1], up, down);
    }
    CHECK(season_nl_gold_cup_count == 12 && season_nl_gold_cups[0].year == 2005 && season_nl_gold_cups[0].teams == 12,
          "the Gold Cups from 2005 (12 teams)");
    for (int i = 0; i < season_nl_gold_cup_count; i++)
        CHECK(season_nl_gold_cups[i].teams == (season_nl_gold_cups[i].year >= 2019 ? 16 : 12) &&
              season_nl_gold_cups[i].start.month >= 6 && season_nl_gold_cups[i].start.month <= 7,
              "Gold Cup %d: %d teams from %d.%d", season_nl_gold_cups[i].year, season_nl_gold_cups[i].teams,
              season_nl_gold_cups[i].start.day, season_nl_gold_cups[i].start.month);
}

int main(void)
{
    CHECK(season_nl_weekday(6, 9, 2018) == 3, "6 September 2018 is a Thursday");
    CHECK(season_nl_weekday(18, 10, 2026) == 6, "18 October 2026 is a Sunday");
    CHECK(season_nl_weekday(29, 2, 2020) == 5, "29 February 2020 is a Saturday");
    CHECK(season_nl_qualifying_count >= 3 && season_nl_qualifying[0].euro == 2020 &&
          season_nl_qualifying[0].md[0].month == 3 && season_nl_qualifying[0].md[0].year == 1,
          "Euro 2020 qualifying starts in March of the year after the campaign year");
    CHECK(season_nl_qualifying[2].world_cup && season_nl_qualifying[2].euro == 2022 && season_nl_qualifying[2].paths == 3 &&
          season_nl_qualifying[2].po_final.day == 29 && season_nl_qualifying[2].po_final.year == 2,
          "the 2022 World Cup paths: 3, the finals on 29 March 2022");
    CHECK(season_nl_qualifying_count == 4 && season_nl_qualifying[3].euro == 2028 && !season_nl_qualifying[3].world_cup &&
          season_nl_qualifying[3].paths == 0 && season_nl_qualifying[3].md[0].day == 26 &&
          season_nl_qualifying[3].md[9].month == 11 && season_nl_qualifying[3].po_sf.day == 23 &&
          season_nl_qualifying[3].po_final.year == 2,
          "Euro 2028 qualifying: 26 March to November 2027, the play-off shape by the hosts, March 2028");
    check_table();
    check_promotion();
    check_paths();
    check_concacaf();
    if (failures) fprintf(stderr, "season_nl_test: %d failures\n", failures);
    else printf("season_nl_test: all checks pass\n");
    return failures != 0;
}
