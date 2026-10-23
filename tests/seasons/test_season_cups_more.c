/* Test of the host part of slice 21c (0.6.0, src/recomp_rt/win/season_cups_more.c): the four-club super cups,
 * with the rules table of the build.
 *
 * Usage: season_cups_more_test <rules.tsv made by tools/seasons/make_rules.py>
 * ctest makes the table first (test season_rules_table). The values come from the rules table parts
 * (docs/research/rules-0.6.0/ita.tsv, spa.tsv, rus-tur.tsv): the Supercoppa Italiana has four clubs from
 * 2023/24 to 2025/26 and two again from 2026/27, the Supercopa de Espana four clubs from 2019/20, with penalties
 * after 90 minutes from 2024/25, the Turkish Super Cup four clubs from 2025/26; the new cups (rus-tur.tsv,
 * nor-fin.tsv, eng.tsv, wal-nir-irl.tsv) and the knockout layout of a new cup. */
#include "season_cups.h"
#include "season_rules.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            failures++; \
            printf("FAIL line %d: ", __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } \
    } while (0)

static void four_clubs(void)
{
    const SeasonSuperFour *f = season_super_four("ita_super", 2022);
    CHECK(!f, "Supercoppa 2022/23 has two clubs");
    f = season_super_four("ita_super", 2023);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercoppa 2023/24: four clubs, penalties after 90 minutes");
    CHECK(f && f->sf_day == 18 && f->sf_month == 0 && f->sf_year == 1 && f->f_day == 22, "Supercoppa 2023/24 dates");
    f = season_super_four("ita_super", 2025);
    CHECK(f && f->sf_month == 11 && f->sf_year == 0, "Supercoppa 2025/26 in December 2025");
    CHECK(!season_super_four("ita_super", 2026), "Supercoppa 2026/27 has two clubs again");
    CHECK(!season_super_four("spa_super", 2018), "Supercopa 2018/19 has two clubs");
    f = season_super_four("spa_super", 2019);
    CHECK(f && f->flags == (CUP_PENALTIES | CUP_EXTRA_TIME), "Supercopa 2019/20: extra time and penalties");
    f = season_super_four("spa_super", 2024);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercopa 2024/25: penalties after 90 minutes");
    f = season_super_four("spa_super", 2026);
    CHECK(f && f->sf_month == 1, "Supercopa 2026/27 in February 2027");
    /* After the last row of the table the rule stays (Q248). */
    f = season_super_four("spa_super", 2035);
    CHECK(f && f->flags == CUP_PENALTIES, "Supercopa after the table");
    CHECK(!season_super_four("new_tur_super", 2024), "Turkish Super Cup 2024/25 has two clubs");
    CHECK(season_super_four("new_tur_super", 2025) != NULL, "Turkish Super Cup 2025/26 has four clubs");
    CHECK(!season_super_four("fra_super", 2023), "no four-club row for another cup");
}

static void picks(void)
{
    /* in: league winner, league runner-up, cup winner, cup runner-up, league third. out: the semi-final pairs. */
    uint32_t out[4];
    const uint32_t a[5] = {1, 2, 3, 4, 5};
    CHECK(season_super_four_pick(a, out) && out[0] == 1 && out[1] == 4 && out[2] == 3 && out[3] == 2,
          "four clubs: league winner v cup runner-up, cup winner v league runner-up");
    const uint32_t b[5] = {1, 2, 1, 4, 5};      /* the champion won the cup: the league third takes its place */
    CHECK(season_super_four_pick(b, out) && out[0] == 1 && out[1] == 4 && out[2] == 5 && out[3] == 2,
          "the league third for the cup winner");
    const uint32_t c[5] = {1, 2, 3, 2, 5};      /* the runner-up lost the final */
    CHECK(season_super_four_pick(c, out) && out[1] == 5 && out[2] == 3, "the league third for the cup runner-up");
    const uint32_t d[5] = {1, 2, 2, 1, 5};      /* both finalists came first and second: no fourth club */
    CHECK(!season_super_four_pick(d, out), "two places free and one league third: the two-club format");
    const uint32_t e[5] = {1, 2, 3, 0, 5};      /* no cup runner-up in the history */
    CHECK(season_super_four_pick(e, out) && out[1] == 5, "no cup runner-up");
    const uint32_t h[5] = {1, 2, 3, 1, 3};      /* the cup winner came third, the champion lost the final */
    CHECK(!season_super_four_pick(h, out), "the league third won the cup: no spare, the two-club format");
    const uint32_t k[5] = {1, 2, 2, 4, 3};      /* the runner-up won the cup: the third takes its place once */
    CHECK(season_super_four_pick(k, out) && out[1] == 4 && out[2] == 3 && out[3] == 2, "the league third once");
    const uint32_t g[5] = {0, 2, 3, 4, 5};
    CHECK(!season_super_four_pick(g, out), "no league winner");
}

static void new_cups(void)
{
    /* The Turkish Super Cup: first held in 2006/07, cancelled in 2011/12 (rus-tur.tsv). */
    CHECK(season_new_cup_held("new_tur_super", 2005) == 0, "Turkish Super Cup 2005/06");
    CHECK(season_new_cup_held("new_tur_super", 2006) == 1, "Turkish Super Cup 2006/07");
    CHECK(season_new_cup_held("new_tur_super", 2011) == 0, "Turkish Super Cup 2011/12");
    CHECK(season_new_cup_held("new_tur_super", 2040) == 1, "Turkish Super Cup after the table (Q248)");
    /* The Russian Super Cup: the edition of March 2003 is in row 2002/03, the edition of July 2012 in row 2012/13. */
    CHECK(season_new_cup_held("new_rus_super", 2002) == 0, "Russian Super Cup 2002");
    CHECK(season_new_cup_held("new_rus_super", 2003) == 1, "Russian Super Cup 2003");
    CHECK(season_new_cup_held("new_rus_super", 2011) == 1, "Russian Super Cup 2011");
    CHECK(season_new_cup_held("new_rus_super", 2012) == 1, "Russian Super Cup 2012");
    const SeasonNewCup *f = season_new_cup("new_rus_super", 2011);
    CHECK(f && f->round[0].month == 2, "Russian Super Cup 2011 in March");
    f = season_new_cup("new_rus_super", 2012);
    CHECK(f && f->round[0].month == 6, "Russian Super Cup 2012 in July");
    /* The Norwegian Super Cup: editions 2009, 2010, 2017 and 2018 (rows 2008/09, 2009/10, 2016/17, 2017/18). */
    CHECK(season_new_cup_held("new_nor_super", 2008) == 0, "Norwegian Super Cup 2008");
    CHECK(season_new_cup_held("new_nor_super", 2009) == 1, "Norwegian Super Cup 2009");
    CHECK(season_new_cup_held("new_nor_super", 2011) == 0, "Norwegian Super Cup 2011");
    CHECK(season_new_cup_held("new_nor_super", 2017) == 1, "Norwegian Super Cup 2017");
    CHECK(season_new_cup_held("new_nor_super", 2018) == 1, "Norwegian Super Cup 2018");
    CHECK(season_new_cup_held("new_nor_super", 2019) == 0, "Norwegian Super Cup 2019");
    CHECK(season_new_cup_next("new_nor_super", 2011) == 2017, "Norwegian Super Cup next from 2011");
    /* The edition that a season's row holds (a start with changing rules off makes that edition). */
    CHECK(season_new_cup_row_edition("new_rus_lge_cup", 2002) == 2003, "Russian Premier League Cup in row 2002/03");
    CHECK(season_new_cup_row_edition("new_rus_super", 2010) == 2011, "Russian Super Cup of March 2011 in row 2010/11");
    CHECK(season_new_cup_row_edition("new_rus_super", 2012) == 2012, "Russian Super Cup of July 2012 in row 2012/13");
    CHECK(season_new_cup_row_edition("new_nor_super", 2008) == 2009, "Norwegian Super Cup of 2009 in row 2008/09");
    CHECK(season_new_cup_row_edition("new_conf_cup", 2007) == 2007, "Conference League Cup in row 2007/08");
    CHECK(season_new_cup_next("new_nor_super", 2019) == SEASON_CUPS_NEVER, "Norwegian Super Cup after 2018");
    /* The Russian Premier League Cup: one edition, 2003 (row 2002/03). */
    CHECK(season_new_cup_held("new_rus_lge_cup", 2003) == 1, "Russian Premier League Cup 2003");
    CHECK(season_new_cup_held("new_rus_lge_cup", 2004) == 0, "Russian Premier League Cup 2004");
    f = season_new_cup("new_rus_lge_cup", 2003);
    CHECK(f && f->round[0].legs == 2 && f->round[1].legs == 2, "Russian Premier League Cup: two legs");
    /* The Conference League Cup: 2004/05, 2007/08 and 2008/09; the National League Cup from 2024/25 has
     * under-21 sides that the game does not hold (Q336), so the game does not play it. */
    CHECK(season_new_cup_held("new_conf_cup", 2004) == 1, "Conference League Cup 2004/05");
    CHECK(season_new_cup_held("new_conf_cup", 2005) == 0, "Conference League Cup 2005/06");
    CHECK(season_new_cup_held("new_conf_cup", 2008) == 1, "Conference League Cup 2008/09");
    CHECK(season_new_cup_held("new_conf_cup", 2024) == 0, "National League Cup 2024/25");
    /* The All-Ireland cup: the knockout seasons 2010/11 to 2013/14 (12 clubs, then 8). */
    CHECK(season_new_cup_held("new_all_ireland", 2010) == 1, "Setanta Cup 2010/11");
    CHECK(season_new_cup_held("new_all_ireland", 2014) == 0, "Setanta Cup 2014/15");
    f = season_new_cup("new_all_ireland", 2013);
    CHECK(f && f->clubs == 8, "Setanta Cup 2013/14: 8 clubs");
    CHECK(season_new_cup_held("unknown_cup", 2010) == -1, "no row");
}

static void layouts(void)
{
    int ties0 = 0, rounds = 0;
    CHECK(season_new_cup_layout(16, &ties0, &rounds) && ties0 == 8 && rounds == 4, "16 clubs: 4 rounds");
    CHECK(season_new_cup_layout(12, &ties0, &rounds) && ties0 == 4 && rounds == 4, "12 clubs: 4 ties, 4 byes");
    CHECK(season_new_cup_layout(22, &ties0, &rounds) && ties0 == 6 && rounds == 5, "22 clubs: 6 ties, 10 byes");
    CHECK(season_new_cup_layout(2, &ties0, &rounds) && ties0 == 1 && rounds == 1, "2 clubs: the final");
    CHECK(!season_new_cup_layout(1, &ties0, &rounds), "1 club");
    CHECK(season_new_cup_round_code(5, 0) == 10 && season_new_cup_round_code(5, 1) == 20 &&
          season_new_cup_round_code(5, 2) == 120 && season_new_cup_round_code(5, 4) == 150, "round codes");
}

static void groups(void)
{
    const SeasonCupGroups *g = season_cup_groups("sco_lge_cup", 2015);
    CHECK(!g, "Scottish League Cup 2015/16 has no groups");
    g = season_cup_groups("sco_lge_cup", 2016);
    CHECK(g && g->groups == 8 && g->ko_clubs == 16 && g->byes == 1 && g->base_rounds == 0,
          "Scottish League Cup 2016/17");
    CHECK(season_cup_groups("sco_lge_cup", 2030) == g, "Scottish League Cup after the table");
    g = season_cup_groups("tur_cup", 2005);
    CHECK(g && g->base_rounds == 3 && g->groups == 4 && g->ko_clubs == 8, "Turkish Cup 2005/06");
    CHECK(!season_cup_groups("tur_cup", 2011), "Turkish Cup 2011/12 has no groups");
    g = season_cup_groups("tur_cup", 2012);
    CHECK(g && g->base_rounds == 4 && g->groups == 2 && g->meetings == 2 && g->ko_clubs == 4, "Turkish Cup 2012/13");
    g = season_cup_groups("tur_cup", 2014);
    CHECK(g && g->base_rounds == 2 && g->groups == 8 && g->ko_clubs == 16, "Turkish Cup 2014/15");
    CHECK(!season_cup_groups("tur_cup", 2017), "Turkish Cup 2017/18 has no groups");
    CHECK(season_cup_groups("tur_cup", 2024) != NULL, "Turkish Cup 2024/25");
    CHECK(!season_cup_groups("tur_cup", 2026), "Turkish Cup 2026/27 has no groups");
    g = season_cup_groups("eng_auto_cup", 2016);
    CHECK(g && g->groups == 16 && g->per_group == 2 && g->ko_clubs == 32, "EFL Trophy 2016/17");
    /* Every format: the knockout is a power of 2 that its rounds hold, the group days are in order. */
    const char *files[] = {"sco_lge_cup", "eng_auto_cup", "tur_cup", "test_eng_cc_cup"};
    for (int f = 0; f < 4; f++)
        for (int y = 2001; y <= 2027; y++) {
            g = season_cup_groups(files[f], y);
            if (!g) continue;
            int ties0 = 0, rounds = 0;
            CHECK(season_new_cup_layout(g->ko_clubs, &ties0, &rounds) && 2 * ties0 == g->ko_clubs && rounds <= 5 &&
                  g->ko[rounds - 1].day > 0, "%s %d: knockout of %d clubs", files[f], y, g->ko_clubs);
            for (int r = 0; r + 1 < rounds; r++) {
                const SeasonNewRound *a = &g->ko[rounds - 1 - r], *b = &g->ko[rounds - 2 - r];
                CHECK(a->year * 400 + a->month * 31 + a->day < b->year * 400 + b->month * 31 + b->day,
                      "%s %d: knockout round %d after round %d", files[f], y, r, r + 1);
            }
            for (int d = 0; d + 1 < 10 && g->days[d + 1][0]; d++)
                CHECK(g->days[d][2] * 400 + g->days[d][1] * 31 + g->days[d][0] <
                      g->days[d + 1][2] * 400 + g->days[d + 1][1] * 31 + g->days[d + 1][0],
                      "%s %d: group day %d", files[f], y, d);
        }
    int sizes[16];
    CHECK(season_cup_group_sizes(36, 8, sizes) && sizes[0] == 5 && sizes[3] == 5 && sizes[4] == 4 && sizes[7] == 4,
          "36 clubs in 8 groups: four of 5, four of 4");
    CHECK(season_cup_group_sizes(48, 16, sizes) && sizes[15] == 3, "48 clubs in 16 groups of 3");
    CHECK(!season_cup_group_sizes(15, 8, sizes), "fewer than 2 clubs a group");
    CHECK(season_cup_group_days(5, 1) == 5 && season_cup_group_days(4, 1) == 3 && season_cup_group_days(4, 2) == 6 &&
          season_cup_group_days(3, 1) == 3, "group days");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: season_cups_more_test <rules.tsv>\n");
        return 2;
    }
    if (season_rules_load(argv[1]) <= 0) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    four_clubs();
    picks();
    new_cups();
    layouts();
    groups();
    if (failures) printf("%d failures\n", failures);
    else printf("season_cups_more: all checks passed\n");
    return failures ? 1 : 0;
}
