/* Test of the comparison's axes and rows (src/recomp_rt/win/compare_layout.c).
 * Usage: compare_test */
#include "compare_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

/* A width of 7 pixels a character, as font 1 is about. */
static int width7(const char *text, void *ctx)
{
    (void)ctx;
    return 7 * (int)strlen(text);
}

/* A width where a long text measures more than its parts, as the game's font 7 does for the heading. */
static int width_uneven(const char *text, void *ctx)
{
    (void)ctx;
    return 7 * (int)strlen(text) + (strlen(text) > 30 ? 12 : 0);
}

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

/* 1 when axis a has the name and exactly the attributes of want (ended by 0). */
static int axis_is(const CompareAxis *a, const char *name, const int *want)
{
    if (strcmp(a->name, name)) return 0;
    int i = 0;
    for (; want[i]; i++)
        if (a->attr[i] != want[i]) return 0;
    return a->attr[i] == 0;
}

/* 1 when attribute k is a row of the side list. */
static int row_has(const int *rows, int n, int k)
{
    for (int i = 0; i < n; i++)
        if (rows[i] == k) return 1;
    return 0;
}

int main(void)
{
    /* 1. The eight outfield axes of the spec (Q110, Q117), in order. */
    const CompareAxis *o = compare_axes(0);
    static const int defending[] = {ATTR_TACKLING, ATTR_MARKING, ATTR_POSITIONING, 0};
    static const int physical[] = {ATTR_STRENGTH, ATTR_STAMINA, ATTR_AGILITY, ATTR_BALANCE, 0};
    static const int speed[] = {ATTR_PACE, ATTR_ACCELERATION, 0};
    static const int vision[] = {ATTR_PASSING, ATTR_CREATIVITY, ATTR_FLAIR, 0};
    static const int attacking[] = {ATTR_FINISHING, ATTR_OFF_THE_BALL, ATTR_LONG_SHOTS, 0};
    static const int technique[] = {ATTR_TECHNIQUE, ATTR_DRIBBLING, ATTR_CROSSING, 0};
    static const int aerial[] = {ATTR_HEADING, ATTR_JUMPING, 0};
    static const int mental[] = {ATTR_DECISIONS, ATTR_ANTICIPATION, ATTR_WORK_RATE, ATTR_TEAMWORK,
                                 ATTR_DETERMINATION, 0};
    CHECK(axis_is(&o[0], "Defending", defending));
    CHECK(axis_is(&o[1], "Physical", physical));
    CHECK(axis_is(&o[2], "Speed", speed));
    CHECK(axis_is(&o[3], "Vision", vision));
    CHECK(axis_is(&o[4], "Attacking", attacking));
    CHECK(axis_is(&o[5], "Technique", technique));
    CHECK(axis_is(&o[6], "Aerial", aerial));
    CHECK(axis_is(&o[7], "Mental", mental));

    /* 2. Every attribute of an axis is a row of the side list, for outfield players and goalkeepers. */
    for (int keepers = 0; keepers <= 1; keepers++) {
        int rows[COMPARE_ROWS];
        int n = compare_rows(keepers, rows, COMPARE_ROWS);
        CHECK(n > 0 && n <= COMPARE_ROWS);
        const CompareAxis *a = compare_axes(keepers);
        for (int i = 0; i < COMPARE_AXES; i++) {
            CHECK(a[i].name && a[i].name[0]);
            CHECK(a[i].abbr && strlen(a[i].abbr) == 3 && !strncmp(a[i].abbr, a[i].name, 2));
            CHECK(a[i].attr[0] != 0);
            for (int j = 0; a[i].attr[j]; j++) CHECK(row_has(rows, n, a[i].attr[j]));
        }
        /* The rows are the profile's groups without its five other rows (Q126). */
        CHECK(!row_has(rows, n, CELL_FOOT) && !row_has(rows, n, CELL_ON_LOAN) && !row_has(rows, n, CELL_BLANK));
        CHECK(rows[0] == (keepers ? CELL_HEAD_GOALKEEPING : CELL_HEAD_TECHNICAL));
        CHECK(row_has(rows, n, ATTR_HANDLING) == keepers);
        CHECK(row_has(rows, n, ATTR_FINISHING) == !keepers);
    }
    int rows[COMPARE_ROWS];
    CHECK(compare_rows(0, rows, COMPARE_ROWS) == 32);    /* 29 attributes and 3 headings */
    /* 24 attributes and 3 headings: a goalkeeper has no Technical heading, and One On Ones and Consistency
     * are not rows of the comparison (Q111). */
    CHECK(compare_rows(1, rows, COMPARE_ROWS) == 27);
    compare_rows(1, rows, COMPARE_ROWS);
    CHECK(!row_has(rows, 27, CELL_HEAD_TECHNICAL) && !row_has(rows, 27, CELL_ONE_ON_ONES) &&
          !row_has(rows, 27, CELL_CONSISTENCY));

    /* 3. Two goalkeepers get other axes, from shown goalkeeper attributes. */
    const CompareAxis *k = compare_axes(1);
    int handling = 0, reflexes = 0;
    for (int i = 0; i < COMPARE_AXES; i++)
        for (int j = 0; k[i].attr[j]; j++) {
            handling |= k[i].attr[j] == ATTR_HANDLING;
            reflexes |= k[i].attr[j] == ATTR_REFLEXES;
            CHECK(k[i].attr[j] != ATTR_FINISHING && k[i].attr[j] != ATTR_CROSSING);
        }
    CHECK(handling && reflexes);

    /* 4. An axis value is the average of its shown attributes, in tenths, rounded half up. */
    uint8_t attr[32] = {0};
    for (int i = 1; i <= 31; i++) attr[i] = 10;
    attr[ATTR_TACKLING] = 15; attr[ATTR_MARKING] = 12; attr[ATTR_POSITIONING] = 9;
    CHECK(compare_axis_value(&o[0], attr) == 120);                   /* 36 / 3 = 12.0 */
    attr[ATTR_POSITIONING] = 10;
    CHECK(compare_axis_value(&o[0], attr) == 123);                   /* 37 / 3 = 12.33 */
    attr[ATTR_PACE] = 17; attr[ATTR_ACCELERATION] = 14;
    CHECK(compare_axis_value(&o[2], attr) == 155);                   /* 31 / 2 = 15.5 */
    attr[ATTR_STRENGTH] = 20; attr[ATTR_STAMINA] = 19; attr[ATTR_AGILITY] = 18; attr[ATTR_BALANCE] = 18;
    CHECK(compare_axis_value(&o[1], attr) == 188);                   /* 75 / 4 = 18.75 */
    /* A hidden attribute (0) does not count; all hidden gives -1 (Q111). */
    attr[ATTR_MARKING] = 0;
    CHECK(compare_axis_value(&o[0], attr) == 125);                   /* 25 / 2 = 12.5 */
    attr[ATTR_TACKLING] = 0; attr[ATTR_POSITIONING] = 0;
    CHECK(compare_axis_value(&o[0], attr) == -1);

    /* 5. The corners: axis 0 points up, axis 2 right, axis 4 down, axis 6 left; value 20 is the radius. */
    int x, y;
    compare_corner(0, 200, 100, 100, 50, &x, &y);
    CHECK(x == 100 && y == 50);
    compare_corner(2, 200, 100, 100, 50, &x, &y);
    CHECK(x == 150 && y == 100);
    compare_corner(4, 100, 100, 100, 50, &x, &y);
    CHECK(x == 100 && y == 125);
    compare_corner(6, 200, 100, 100, 50, &x, &y);
    CHECK(x == 50 && y == 100);
    compare_corner(1, 200, 0, 0, 100, &x, &y);
    CHECK(x == 71 && y == -71);
    compare_corner(3, -1, 100, 100, 50, &x, &y);                     /* no value: the centre */
    CHECK(x == 100 && y == 100);

    /* 6. The text of a value in tenths. */
    char t[16];
    compare_value_text(123, t, sizeof t);
    CHECK(!strcmp(t, "12.3"));
    compare_value_text(200, t, sizeof t);
    CHECK(!strcmp(t, "20.0"));
    compare_value_text(-1, t, sizeof t);
    CHECK(!strcmp(t, "-"));

    /* 7. Texts that must fit their box (Tousled12, 2026-10-21): the worst cases of names and lines. */
    char f[160], line[2][96];
    compare_fit("Arsenal", 70, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Arsenal"));
    compare_fit("Van Bronckhorst", 61, width7, NULL, f, sizeof f);        /* the classic legend */
    CHECK(!strcmp(f, "Van Bro.") && width7(f, NULL) <= 61);
    compare_fit("Gilberto Silva", 61, width7, NULL, f, sizeof f);         /* no space before the "." */
    CHECK(!strcmp(f, "Gilbert.") && width7(f, NULL) <= 61);
    compare_fit("Liverpool, GK", 70, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Liverpo.") || !strcmp(f, "Liverpool."));
    CHECK(width7(f, NULL) <= 70);
    compare_fit("Wolverhampton", 5, width7, NULL, f, sizeof f);           /* nothing fits: the first letter */
    CHECK(!strcmp(f, "W"));

    compare_fit_name("Edson", "Gilberto Silva", "Gilberto Silva", 300, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Gilberto Silva"));                                  /* the common name */
    compare_fit_name("Giovanni", "Van Bronckhorst", "", 226, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Giovanni Van Bronckhorst"));
    compare_fit_name("Giovanni", "Van Bronckhorst", NULL, 140, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "G. Van Bronckhorst"));
    compare_fit_name("Giovanni", "Van Bronckhorst", NULL, 110, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Van Bronckhorst"));
    compare_fit_name("Jan", "Vennegoor of Hesselink", NULL, 120, width7, NULL, f, sizeof f);
    CHECK(width7(f, NULL) <= 120 && !strncmp(f, "Vennegoor", 9) && f[strlen(f) - 1] == '.');

    CHECK(compare_info_lines("Arsenal", "DM LC", 26, 226, width7, NULL, line) == 1);
    CHECK(!strcmp(line[0], "Arsenal, DM LC, age 26") && !line[1][0]);
    /* A long club and a long positions text: club on the first line, positions and age on the second. */
    CHECK(compare_info_lines("Universidad de Concepcion", "D/WB/M/AM RLC", 33, 226, width7, NULL, line) == 2);
    CHECK(!strcmp(line[0], "Universidad de Concepcion") && !strcmp(line[1], "D/WB/M/AM RLC, age 33"));
    CHECK(compare_info_lines("Universidad de Concepcion", "D/WB/M/AM RLC", 33, 150, width7, NULL, line) == 2);
    CHECK(width7(line[0], NULL) <= 150 && width7(line[1], NULL) <= 150);

    compare_fit_heading("David Seaman", "David", "Seaman", "", "Rami Shaaban", "Rami", "Shaaban", "", 690, width7,
                        NULL, f, sizeof f);
    CHECK(!strcmp(f, "David Seaman ~ Rami Shaaban"));
    compare_fit_heading("Giovanni Van Bronckhorst", "Giovanni", "Van Bronckhorst", "", "Jan Vennegoor of Hesselink",
                        "Jan", "Vennegoor of Hesselink", "", 300, width7, NULL, f, sizeof f);
    CHECK(width7(f, NULL) <= 300 && strstr(f, " ~ "));
    CHECK(!strcmp(f, "G. Van Bronckhorst ~ Vennegoor of Hesse."));
    /* A short name leaves its room to the long one. */
    compare_fit_heading("Jan Vennegoor of Hesselink", "Jan", "Vennegoor of Hesselink", "", "Pegguy Arphexad", "Pegguy",
                        "Arphexad", "", 300, width7, NULL, f, sizeof f);
    CHECK(!strcmp(f, "Vennegoor of Hesselink ~ Pegguy Arphexad"));
    for (int box = 200; box <= 320; box += 3) {
        compare_fit_heading("Jan Vennegoor of Hesselink", "Jan", "Vennegoor of Hesselink", "", "Pegguy Arphexad",
                            "Pegguy", "Arphexad", "", box, width_uneven, NULL, f, sizeof f);
        CHECK(width_uneven(f, NULL) <= box && strstr(f, " ~ "));
        CHECK(box < 260 || strstr(f, " ~ Pegguy Arphexad"));       /* the short name stays when it can */
    }

    if (failures) printf("%d failures\n", failures);
    else printf("compare: all checks passed\n");
    return failures != 0;
}
