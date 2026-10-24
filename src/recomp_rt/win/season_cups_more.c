/* The part of slice 21c that does not touch the game's memory (season_cups.h): the four-club super cups. */
#include "season_cups.h"
#include "season_rules.h"

#include <stddef.h>
#include <string.h>

/* The rows of the rules table that a file reads: a class file by its comp_file, a new competition (comp_file
 * "new") by its nation and comp. */
typedef struct {
    const char *file, *nation, *comp;
} NewKey;

static const NewKey new_keys[] = {
    {"new_tur_super", "Turkey", "super cup"},
    {"new_rus_super", "Russia", "super cup"},
    {"new_nor_super", "Norway", "Norwegian Super Cup"},
    {"new_rus_lge_cup", "Russia", "league cup"},
    {"new_conf_cup", "England", "Conference League Cup"},
    {"new_all_ireland", "Northern Ireland and Republic of Ireland", "All-Ireland cup"},
};

static int matches(const SeasonRule *r, const char *file)
{
    for (size_t i = 0; i < sizeof new_keys / sizeof new_keys[0]; i++)
        if (!strcmp(new_keys[i].file, file))
            return !strcmp(r->comp_file, "new") && !strcmp(r->nation, new_keys[i].nation) &&
                   !strcmp(r->comp, new_keys[i].comp);
    return !strcmp(r->comp_file, file);
}

/* The latest row of the file not later than season s (Q248), or NULL. */
static const SeasonRule *row_of(const char *file, int s)
{
    const SeasonRule *best = NULL;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (r->season > s || !matches(r, file)) continue;
        if (!best || r->season > best->season) best = r;
    }
    return best;
}

/* ---- Four-club super cups ----
 * The dates of the rows (rules table, column format): the Supercoppa Italiana 2023/24 (first semi-final 18
 * January 2024, final 22 January), 2024/25 (2 January 2025), 2025/26 (18 December 2025), 90 minutes then
 * penalties; the Supercopa de Espana 2019/20 to 2023/24 in January with extra time then penalties (2020: 8 and
 * 12 January), from 2024/25 a draw goes to penalties after 90 minutes, 2026/27 on 2 to 7 February 2027. The days
 * of the semi-finals and finals after the first season of a cup are the real ones where the row gives them, and
 * the January week of the row before where it does not (inference). The Turkish Super Cup of 2025/26 has no date
 * in its row: January as Spain (inference). */
static const SeasonSuperFour four_rows[] = {
    {"ita_super", 2023, CUP_PENALTIES, 18, 0, 1, 22, 0, 1},
    {"ita_super", 2024, CUP_PENALTIES, 2, 0, 1, 6, 0, 1},
    {"ita_super", 2025, CUP_PENALTIES, 18, 11, 0, 22, 11, 0},
    {"spa_super", 2019, CUP_PENALTIES | CUP_EXTRA_TIME, 8, 0, 1, 12, 0, 1},
    {"spa_super", 2020, CUP_PENALTIES | CUP_EXTRA_TIME, 13, 0, 1, 17, 0, 1},
    {"spa_super", 2021, CUP_PENALTIES | CUP_EXTRA_TIME, 12, 0, 1, 16, 0, 1},
    {"spa_super", 2022, CUP_PENALTIES | CUP_EXTRA_TIME, 11, 0, 1, 15, 0, 1},
    {"spa_super", 2023, CUP_PENALTIES | CUP_EXTRA_TIME, 10, 0, 1, 14, 0, 1},
    {"spa_super", 2024, CUP_PENALTIES, 8, 0, 1, 12, 0, 1},
    {"spa_super", 2025, CUP_PENALTIES, 7, 0, 1, 11, 0, 1},
    {"spa_super", 2026, CUP_PENALTIES, 3, 1, 1, 7, 1, 1},
    {"spa_super", 2027, CUP_PENALTIES, 12, 0, 1, 16, 0, 1},
    {"new_tur_super", 2025, CUP_PENALTIES | CUP_EXTRA_TIME, 6, 0, 1, 10, 0, 1},
};

const SeasonSuperFour *season_super_four(const char *file, int season)
{
    const SeasonRule *r = file ? row_of(file, season) : NULL;
    if (!r || r->clubs != 4) return NULL;
    const SeasonSuperFour *best = NULL;
    for (size_t i = 0; i < sizeof four_rows / sizeof four_rows[0]; i++) {
        const SeasonSuperFour *f = &four_rows[i];
        if (strcmp(f->file, file) || f->season > season) continue;
        if (!best || f->season > best->season) best = f;
    }
    return best;
}

static int in_list(const uint32_t *list, int n, uint32_t club)
{
    for (int i = 0; i < n; i++)
        if (list[i] == club) return 1;
    return 0;
}

int season_super_four_pick(const uint32_t *league, int count, uint32_t cw, uint32_t cru, uint32_t out[4])
{
    if (count < 2 || !league[0] || !league[1] || league[0] == league[1]) return 0;
    uint32_t got[4] = {league[0], league[1], 0, 0};
    int n = 2, next = 2;
    const uint32_t cup[2] = {cw, cru};
    for (int c = 0; c < 2; c++) {
        uint32_t club = cup[c];
        if (!club || in_list(got, n, club)) {
            /* The best league club with no place yet. A club named twice in the league list counts once. */
            club = 0;
            while (next < count && league[next] && in_list(got, n, league[next])) next++;
            if (next < count && league[next]) club = league[next++];
            if (!club) return 0;
        }
        got[n++] = club;
    }
    out[0] = got[0], out[1] = got[3], out[2] = got[2], out[3] = got[1];
    return 1;
}

/* ---- New cups ----
 * The formats (rules table rows, columns format and notes; dates marked "inference" are not in the row):
 * - Turkish Super Cup (rus-tur.tsv): one match at a neutral venue from 2006/07, cancelled in 2011/12; the game plays it
 *   in early August (inference: the real dates are late July to mid August). Four clubs from 2025/26
 *   (season_super_four).
 * - Russian Super Cup (rus-tur.tsv): one match, the league champions v the cup winners. Editions 2003 to 2011 in March
 *   before the season (the row of the season before), from 2012 in July (its own row); days 8 March and 14 July are an
 *   inference.
 * - Norwegian Super Cup (nor-fin.tsv): one match in March before the season, editions 2009, 2010, 2017 and 2018 (the
 *   row of the season before); 15 March is an inference.
 * - Russian Premier League Cup (rus-tur.tsv): 2003 only, the 16 Premier League clubs, every round over two legs,
 *   played on national-team dates (the dates of the Euro 2004 qualifiers of 2003 are an inference).
 * - Conference League Cup (eng.tsv): 2004/05, 2007/08 and 2008/09, a knockout of the Conference clubs with extra time
 *   and penalties (the game holds one Conference division; the months are an inference). From 2024/25 the National
 *   League Cup has 16 Premier League 2 under-21 sides, which the game does not hold (Q336): not played.
 * - All-Ireland cup (wal-nir-irl.tsv): 2010/11 to 2012/13 12 clubs (two-legged first round, quarter-finals and
 *   semi-finals, one-match final), 2013/14 8 clubs (4 from each league). The group formats of 2004/05 to 2007/08 and
 *   2009/10 are not played (docs/research/2026-10-18-cups-more.md, section 4). */
#define ET_PENS (CUP_PENALTIES | CUP_EXTRA_TIME)
static const SeasonNewCup new_cups[] = {
    {"new_tur_super", 2006, 0, 2, {{1, ET_PENS, 7, 7, 0, 0}}},
    {"new_rus_super", 2003, 1, 2, {{1, ET_PENS, 8, 2, 0, 0}}},
    {"new_rus_super", 2012, 0, 2, {{1, ET_PENS, 14, 6, 0, 0}}},
    {"new_nor_super", 2009, 1, 2, {{1, ET_PENS, 15, 2, 0, 0}}},
    {"new_rus_lge_cup", 2003, 1, 0, {{2, ET_PENS, 11, 9, 0, 4}, {2, ET_PENS, 6, 8, 0, 4}, {2, ET_PENS, 7, 5, 0, 4},
                                     {2, ET_PENS, 29, 2, 0, 4}}},
    {"new_conf_cup", 2004, 0, 0, {{1, ET_PENS, 22, 2, 1, 0}, {1, ET_PENS, 18, 0, 1, 0}, {1, ET_PENS, 14, 11, 0, 0},
                                  {1, ET_PENS, 16, 10, 0, 0}, {1, ET_PENS, 19, 9, 0, 0}}},
    {"new_conf_cup", 2024, 0, -1, {{0}}},
    {"new_all_ireland", 2004, 0, -1, {{0}}},
    {"new_all_ireland", 2010, 0, 12, {{1, ET_PENS, 21, 4, 1, 0}, {2, ET_PENS, 9, 3, 1, 7}, {2, ET_PENS, 7, 2, 1, 7},
                                      {2, ET_PENS, 14, 1, 1, 7}}},
    {"new_all_ireland", 2013, 0, 8, {{1, ET_PENS, 17, 4, 1, 0}, {2, ET_PENS, 5, 3, 1, 7}, {2, ET_PENS, 1, 2, 1, 7}}},
};
#define NEW_CUP_COUNT ((int)(sizeof new_cups / sizeof new_cups[0]))

const SeasonNewCup *season_new_cup(const char *file, int edition)
{
    const SeasonNewCup *best = NULL;
    for (int i = 0; file && i < NEW_CUP_COUNT; i++) {
        const SeasonNewCup *f = &new_cups[i];
        if (strcmp(f->file, file) || f->from > edition) continue;
        if (!best || f->from > best->from) best = f;
    }
    return best;
}

/* The row shift of an edition: its format's, or the first format's before the first. */
static int shift_of(const char *file, int edition)
{
    const SeasonNewCup *f = season_new_cup(file, edition);
    if (f) return f->row_shift;
    for (int i = 0; i < NEW_CUP_COUNT; i++)
        if (!strcmp(new_cups[i].file, file)) return new_cups[i].row_shift;
    return 0;
}

int season_new_cup_row_edition(const char *file, int season)
{
    for (int e = season; e <= season + 1; e++)
        if (e - shift_of(file, e) == season) return e;
    return season;
}

int season_new_cup_held(const char *file, int edition)
{
    const SeasonRule *r = file ? row_of(file, edition - shift_of(file, edition)) : NULL;
    if (!r) return -1;
    if (r->clubs == SEASON_RULE_NOT_HELD || !strcmp(r->format, "not_held")) return 0;
    /* A row "cancelled" (the Turkish Super Cup of 2011/12) holds no edition either. */
    if (r->format_text && !strncmp(r->format_text, "cancelled", 9)) return 0;
    const SeasonNewCup *f = season_new_cup(file, edition);
    return f && f->clubs >= 0;
}

int season_new_cup_next(const char *file, int edition)
{
    int h = season_new_cup_held(file, edition);
    if (h < 0) return 0;
    if (h) return edition;
    int last = 0;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (matches(r, file) && r->season > last) last = r->season;
    }
    for (int e = edition + 1; e <= last + 1; e++)
        if (season_new_cup_held(file, e) == 1) return e;
    return SEASON_CUPS_NEVER;
}

int season_new_cup_layout(int clubs, int *ties0, int *rounds)
{
    if (clubs < 2 || clubs > 32) return 0;
    int p = 1, n = 0;
    while (p * 2 <= clubs) p *= 2, n++;
    *rounds = clubs == p ? n : n + 1;
    *ties0 = clubs == p ? p / 2 : clubs - p;
    return 1;
}

int season_new_cup_round_code(int rounds, int r)
{
    int from_final = rounds - 1 - r;
    if (from_final == 0) return 150;
    if (from_final == 1) return 130;
    if (from_final == 2) return 120;
    return 10 * (r + 1);
}

/* ---- Group stages ----
 * The formats (rules table rows; the dates are the real weeks where the row gives them, else an inference):
 * - Scottish League Cup (sco.tsv) from 2016/17: 8 groups of 5 (the game's SPFL clubs not in Europe: the class holds
 *   42 clubs, so the groups are of 5 and 4), single round robin in July, 8 winners and the best runners-up join the
 *   European clubs in a last 16; single matches with extra time and penalties; the semi-finals in October and the
 *   final in late November (the final of 2016/17 was on 27 November). The row's bonus point for a penalty win after a
 *   draw needs a match engine change: a draw stays a draw (docs/research/2026-10-18-cups-more.md, section 4).
 * - EFL Trophy (eng.tsv) from 2016/17: 16 groups of 4 with the academy sides; the game holds no academy sides (Q336),
 *   so the class's own clubs (56 in a game of the October 2016 database) play 16 groups of 4 and 3 (August to
 *   November), the top 2 of each to a last 32; ties settled by penalties after 90 minutes, the final with extra
 *   time and penalties in April at Wembley.
 * - Turkish Cup (rus-tur.tsv): 2005/06 to 2010/11 groups of 5 after 2 rounds and 2014/15 to 2016/17 8 groups of 4
 *   after 3 rounds; the class (TurCup 0x8fc670) holds 64 clubs in 7 rounds (36, 28, 32 and 16 clubs; round 3 on 13
 *   December, round 4 on 17 January), so the groups come after its rounds 1 and 2 (32 clubs: 8 groups of 4, top 2
 *   to a last 16) for 2014/15 to 2016/17, and after its
 *   rounds 1 to 3 (16 clubs: 4 groups of 4, top 2 to the quarter-finals) for 2005/06 to 2010/11 and 2024/25 to
 *   2025/26 (the real groups of 5, 6 or 8 clubs need more clubs than the class holds at that point); 2012/13 and
 *   2013/14 2 groups of 4 after its rounds 1 to 4, top 2 to the semi-finals. Quarter-finals and semi-finals over two
 *   legs where the row says so. */
static const SeasonCupGroups group_formats[] = {
    {"sco_lge_cup", 2016, 8, 0, 1, 1, 1, 16, {{15, 6, 0}, {18, 6, 0}, {22, 6, 0}, {25, 6, 0}, {29, 6, 0}},
     {{1, ET_PENS, 26, 10, 0, 0}, {1, ET_PENS, 22, 9, 0, 0}, {1, ET_PENS, 20, 8, 0, 0}, {1, ET_PENS, 9, 7, 0, 0}}},
    {"eng_auto_cup", 2016, 16, 0, 1, 2, 0, 32, {{30, 7, 0}, {4, 9, 0}, {8, 10, 0}},
     {{1, ET_PENS, 2, 3, 1, 0}, {1, CUP_PENALTIES, 20, 1, 1, 0}, {1, CUP_PENALTIES, 23, 0, 1, 0},
      {1, CUP_PENALTIES, 9, 0, 1, 0}, {1, CUP_PENALTIES, 3, 11, 0, 0}}},
    {"tur_cup", 2005, 4, 3, 1, 2, 0, 8, {{7, 0, 1}, {14, 0, 1}, {21, 0, 1}},
     {{1, ET_PENS, 9, 4, 1, 0}, {2, ET_PENS, 1, 2, 1, 21}, {2, ET_PENS, 4, 1, 1, 7}}},
    {"tur_cup", 2011, 0},
    {"tur_cup", 2012, 2, 4, 2, 2, 0, 4, {{30, 0, 1}, {6, 1, 1}, {13, 1, 1}, {27, 1, 1}, {6, 2, 1}, {13, 2, 1}},
     {{1, ET_PENS, 22, 4, 1, 0}, {2, ET_PENS, 3, 3, 1, 14}}},
    {"tur_cup", 2014, 8, 2, 2, 2, 0, 16,
     {{2, 11, 0}, {16, 11, 0}, {6, 0, 1}, {13, 0, 1}, {20, 0, 1}, {27, 0, 1}},
     {{1, ET_PENS, 26, 4, 1, 0}, {2, ET_PENS, 1, 3, 1, 21}, {2, ET_PENS, 1, 2, 1, 7}, {2, ET_PENS, 3, 1, 1, 7}}},
    {"tur_cup", 2017, 0},
    {"tur_cup", 2024, 4, 3, 1, 2, 0, 8, {{7, 0, 1}, {21, 0, 1}, {4, 1, 1}},
     {{1, ET_PENS, 14, 4, 1, 0}, {1, ET_PENS, 2, 3, 1, 0}, {1, ET_PENS, 4, 2, 1, 0}}},
    {"tur_cup", 2026, 0},
    /* Test only (RECOMP_CUP_GROUPS_TEST=1, season_cup_groups.c): the League Cup with 16 groups of 5 and 6 and a last
     * 32, so a run with England alone plays the path of the Scottish League Cup (the same Cup base slots). */
    {"test_eng_cc_cup", 2001, 16, 0, 1, 2, 0, 32, {{13, 7, 0}, {27, 7, 0}, {10, 8, 0}, {24, 8, 0}, {8, 9, 0}},
     {{1, ET_PENS, 25, 1, 1, 0}, {1, ET_PENS, 21, 0, 1, 0}, {1, ET_PENS, 17, 11, 0, 0}, {1, ET_PENS, 26, 10, 0, 0},
      {1, ET_PENS, 29, 9, 0, 0}}},
};
#define GROUP_FORMAT_COUNT ((int)(sizeof group_formats / sizeof group_formats[0]))

const SeasonCupGroups *season_cup_groups(const char *file, int season)
{
    const SeasonCupGroups *best = NULL;
    for (int i = 0; file && i < GROUP_FORMAT_COUNT; i++) {
        const SeasonCupGroups *g = &group_formats[i];
        if (strcmp(g->file, file) || g->from > season) continue;
        if (!best || g->from > best->from) best = g;
    }
    return best && best->groups > 0 ? best : NULL;
}

int season_cup_group_sizes(int clubs, int groups, int *sizes)
{
    if (groups < 1 || clubs < 2 * groups || clubs > 10 * groups) return 0;
    for (int g = 0; g < groups; g++) sizes[g] = clubs / groups + (g < clubs % groups ? 1 : 0);
    return 1;
}

int season_cup_group_days(int clubs, int meetings)
{
    return (clubs % 2 ? clubs : clubs - 1) * meetings;
}
