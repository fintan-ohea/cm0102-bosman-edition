/* The comparison's axes and rows: see compare_layout.h. */
#include "compare_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const CompareAxis outfield[COMPARE_AXES] = {
    {"Defending", "Def", {ATTR_TACKLING, ATTR_MARKING, ATTR_POSITIONING}},
    {"Physical", "Phy", {ATTR_STRENGTH, ATTR_STAMINA, ATTR_AGILITY, ATTR_BALANCE}},
    {"Speed", "Spd", {ATTR_PACE, ATTR_ACCELERATION}},
    {"Vision", "Vis", {ATTR_PASSING, ATTR_CREATIVITY, ATTR_FLAIR}},
    {"Attacking", "Att", {ATTR_FINISHING, ATTR_OFF_THE_BALL, ATTR_LONG_SHOTS}},
    {"Technique", "Tec", {ATTR_TECHNIQUE, ATTR_DRIBBLING, ATTR_CROSSING}},
    {"Aerial", "Aer", {ATTR_HEADING, ATTR_JUMPING}},
    {"Mental", "Men", {ATTR_DECISIONS, ATTR_ANTICIPATION, ATTR_WORK_RATE, ATTR_TEAMWORK, ATTR_DETERMINATION}},
};

/* The goalkeeper axes (proposal of slice 6, for Max's approval): only attributes that a goalkeeper's
 * profile shows (Q129). Shot Stopping, Handling, Aerial and Command hold the picker's goalkeeper keys
 * (Handling, Reflexes, Agility, Positioning, Jumping); Speed, Physical and Mental are as for outfield
 * players without Agility, which Shot Stopping holds. */
static const CompareAxis keeper[COMPARE_AXES] = {
    {"Shot Stopping", "Sho", {ATTR_REFLEXES, ATTR_AGILITY}},
    {"Handling", "Han", {ATTR_HANDLING}},
    {"Aerial", "Aer", {ATTR_JUMPING, ATTR_BRAVERY}},
    {"Command", "Com", {ATTR_POSITIONING, ATTR_INFLUENCE}},
    {"Distribution", "Dis", {ATTR_PASSING, ATTR_TECHNIQUE}},
    {"Speed", "Spd", {ATTR_PACE, ATTR_ACCELERATION}},
    {"Physical", "Phy", {ATTR_STRENGTH, ATTR_STAMINA, ATTR_BALANCE}},
    {"Mental", "Men", {ATTR_DECISIONS, ATTR_ANTICIPATION, ATTR_WORK_RATE, ATTR_TEAMWORK, ATTR_DETERMINATION}},
};

const CompareAxis *compare_axes(int keepers) { return keepers ? keeper : outfield; }

int compare_axis_value(const CompareAxis *a, const uint8_t *attr)
{
    int sum = 0, n = 0;
    for (int i = 0; a->attr[i]; i++)
        if (attr[a->attr[i]]) {
            sum += attr[a->attr[i]];
            n++;
        }
    return n ? (sum * 20 + n) / (2 * n) : -1;
}

int compare_rows(int keepers, int *rows, int max)
{
    GroupLayout l;
    groups_layout(keepers, &l);
    int n = 0;
    for (int c = 0; c < GROUP_COLUMNS; c++)
        for (int r = 0; r < GROUP_ROWS; r++) {
            int k = l.cell[c][r];
            if ((k >= 1 && k <= 31) || groups_heading(k))
                if (n < max) rows[n++] = k;
        }
    return n;
}

void compare_corner(int i, int tenths, int cx, int cy, int r, int *x, int *y)
{
    double a = -M_PI / 2 + i * 2 * M_PI / COMPARE_AXES, d = tenths < 0 ? 0 : r * tenths / 200.0;
    *x = cx + (int)lround(cos(a) * d);
    *y = cy + (int)lround(sin(a) * d);
}

void compare_value_text(int tenths, char *out, size_t size)
{
    if (tenths < 0) snprintf(out, size, "-");
    else snprintf(out, size, "%d.%d", tenths / 10, tenths % 10);
}

void compare_fit(const char *text, int box, CompareWidth width, void *ctx, char *out, size_t size)
{
    snprintf(out, size, "%s", text ? text : "");
    if (width(out, ctx) <= box) return;
    char base[256];
    snprintf(base, sizeof base, "%s", out);
    for (size_t n = strlen(base); n > 0; n--) {
        size_t k = n - 1;
        while (k > 0 && (base[k - 1] == ' ' || base[k - 1] == ',')) k--;
        if (k == 0) break;
        snprintf(out, size, "%.*s.", (int)k, base);
        if (width(out, ctx) <= box) return;
    }
    snprintf(out, size, "%.1s", base);   /* nothing fits: the first letter */
}

static int empty(const char *s) { return !s || !*s; }

void compare_fit_name(const char *first, const char *second, const char *common, int box, CompareWidth width,
                      void *ctx, char *out, size_t size)
{
    char t[160];
    if (!empty(common)) {
        compare_fit(common, box, width, ctx, out, size);
        return;
    }
    snprintf(t, sizeof t, "%s%s%s", empty(first) ? "" : first, !empty(first) && !empty(second) ? " " : "",
             empty(second) ? "" : second);
    if (width(t, ctx) <= box || empty(first) || empty(second)) {
        compare_fit(t, box, width, ctx, out, size);
        return;
    }
    snprintf(t, sizeof t, "%.1s. %s", first, second);
    if (width(t, ctx) <= box) {
        snprintf(out, size, "%s", t);
        return;
    }
    compare_fit(second, box, width, ctx, out, size);
}

void compare_fit_heading(const char *full1, const char *first1, const char *second1, const char *common1,
                         const char *full2, const char *first2, const char *second2, const char *common2, int box,
                         CompareWidth width, void *ctx, char *out, size_t size)
{
    snprintf(out, size, "%s ~ %s", full1, full2);
    if (width(out, ctx) <= box) return;
    /* A name that fits in half the room stays, and the other takes the rest; else each gets half. The parts
     * of a text can measure less than the whole (the font's spacing), so the rooms shrink by what is still
     * over until the whole fits. */
    int room = box - width(" ~ ", ctx), half = room / 2;
    int keep1 = width(full1, ctx) <= half, keep2 = !keep1 && width(full2, ctx) <= half;
    int r1 = keep2 ? room - width(full2, ctx) : half, r2 = keep1 ? room - width(full1, ctx) : half;
    char a[160], b[160];
    for (int tries = 0; tries < 8; tries++) {
        if (keep1) snprintf(a, sizeof a, "%s", full1);
        else compare_fit_name(first1, second1, common1, r1, width, ctx, a, sizeof a);
        if (keep2) snprintf(b, sizeof b, "%s", full2);
        else compare_fit_name(first2, second2, common2, r2, width, ctx, b, sizeof b);
        snprintf(out, size, "%s ~ %s", a, b);
        int over = width(out, ctx) - box;
        if (over <= 0) return;
        if (!keep1 && !keep2) {
            r1 -= (over + 1) / 2;
            r2 -= (over + 1) / 2;
        } else if (keep1) r2 -= over;
        else r1 -= over;
    }
    snprintf(a, sizeof a, "%s", out);
    compare_fit(a, box, width, ctx, out, size);
}

int compare_info_lines(const char *club, const char *positions, int age, int box, CompareWidth width, void *ctx,
                       char line[2][96])
{
    char t[256];
    snprintf(t, sizeof t, "%s%s%s, age %d", club, *club ? ", " : "", positions, age);
    line[1][0] = 0;
    if (width(t, ctx) <= box || !*club) {
        compare_fit(t, box, width, ctx, line[0], sizeof line[0]);
        return 1;
    }
    compare_fit(club, box, width, ctx, line[0], sizeof line[0]);
    snprintf(t, sizeof t, "%s, age %d", positions, age);
    compare_fit(t, box, width, ctx, line[1], sizeof line[1]);
    return 2;
}
