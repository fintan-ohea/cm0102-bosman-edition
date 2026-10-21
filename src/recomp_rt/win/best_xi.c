/* The Best XI tab of the Team views screen (spec 2026-10-15-features-0.4.0, slices 4 and 5; Q103 to Q107,
 * Q128, Q132, Q156, Q161; one screen with Squad depth since Max's play test of 0.6.0, mockup
 * docs/research/mockups/2026-10-20-best-xi-depth.html, option A with one card per slot). views.c builds the
 * frame and calls best_xi_build for tab 1 and best_xi_input for each click.
 *
 * The eleven comes from the picker (picker_compute_view, picker.c), the same function that "Pick best
 * team" uses, but it is never stored: the tab only reads the game (Q104). The picker leaves out the
 * players who cannot play the next match (injured, suspended, away with a national team), as the squad
 * screen does. For another club (Q108) the picker counts only the attributes that the player's screens
 * show (0x543280), so no hidden value feeds the pick or a star on the screen (Q111).
 *
 * The look is the original's own (Max, 2026-10-20: "make it seem identical font etc"; the facts with
 * addresses are in docs/research/2026-10-20-best-xi-original-look.md). The pitch is the original's pitch of the tactics
 * screen and the team award page, draw_pitch 0x8ac600, with its side-on branch (argument 9 not 0, 0x8ac686),
 * goal on the left. Each slot of the formation is a card at the place of its position mask: a column for each
 * line from the goalkeeper to the forwards, and in a column the left side at the top. A card is the dark
 * panel of the tactics list (area flags 2, 0x89c819) with the position in a dark blue box as the shirt
 * numbers of the award page list (style 0x30, [0xaebe04], font 1, pale grey, 0x4164c7), then three rows in
 * font 2 as the names of that list: the slot's player of the eleven in yellow, then the next two players for
 * that slot in pale grey (picker_depth with a row for each slot; a player of the eleven at another slot can
 * be one of them, Q420). A player who cannot play the next match keeps his colour and gets a tag in the
 * original's orange [0xaebdd4] after his name: Inj (injured), Int (away with a national team) or Out (the
 * squad screen's test 0x45cbe0: a ban, a loan or the competition's rules). Each player's score for the slot
 * (1 to 20, Q156) shows as 0.5 to 5 stars in half stars, which the runtime draws after the stars' widget
 * (views_stars, Q420).
 *
 * The Formation drop-down in the heading lists the tactics of the game's own Tactics drop-down (tactics.c).
 * Item 0 is the player's current tactic, where the tab opens. A choice sets node slot 2 and builds the
 * screen again; the cards are then made for the chosen tactic's slots. The team does not change. */
#include "win.h"
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREENS 0xb74450u           /* the screen stack S */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x55f990u     /* thiscall on a Display, 18 args */
#define BUTTON 0x804030u            /* thiscall on S, 12 args: a button with its own rectangle (0x456464) */
#define DISPLAYS 0xb74c4cu          /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define PLAYER_MANAGER 0xb74c26u    /* per player slot, 0x300 bytes apart: the manager */
#define STORE 0xde1d08u             /* the stored selections (picker.c) */
#define POPUP_COLOUR 0xae31aau      /* u16 colours of the Print items (0x4564ed) */
#define ITEM_HIGHLIGHT 0xaebdecu
#define ITEM_TEXT 0xaebdeau
#define BUTTON_TEXT 0xae3184u
#define LIST_TEXT 0xaebde8u         /* the text colour of the squad list's cells (0x45c781) */
#define GREY 0xae317eu              /* the grey of the game's texts (columns.c) */
#define PALE 0xae3184u              /* pale grey: the list's names and the shirt numbers (0x4164c7) */
#define ORANGE 0xaebdd4u            /* the original's orange (the comparison screen's masked values) */
#define TEXT_WIDTH 0x5e5ad0u        /* cdecl (font, text): the width in screen pixels */
#define DARK_BLUE 0xaebe04u         /* (0, 0, 128): the shirt number boxes of the eleven (0x4164c7) */
#define DRAW_PITCH 0x8ac600u        /* cdecl, 11 args: the original's pitch (see above) */
#define PITCH_SCALE 0x40800000u     /* 4.0f pixels per yard, as both callers of 0x8ac600 pass */

#define FORMATION_ID 0x62u          /* the Formation items; data 1 + the index in the list */
#define CHOSEN 2                    /* node slot: the index of the chosen tactic in the list */
#define FORM_PAGE 3                 /* node slot: the page of the Formation list */
#define PAGE_ITEM 100u              /* data of a page item: PAGE_ITEM + the page */
#define ITEM_PX 21                  /* a popup item, screen pixels: the items do not scale */
#define BUTTON_BOTTOM 35            /* the Formation button's lower edge, classic numbers */
#define SLOTS 11

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t node_slot(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    return call_this(NODE_SLOT, SCREENS, 1, a);
}

static void set_slot(int n, uint32_t value)
{
    uint32_t a[3] = {(uint32_t)n, value, 0};
    call_this(SET_SLOT, SCREENS, 3, a);
}

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static uint32_t human_manager(void)
{
    return rd32(g_mem, PLAYER_MANAGER + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

/* The short name of a player: the common name, else the second name (staff +0xc, +0x8). */
static void short_name(uint32_t staff, char *out, size_t size)
{
    uint32_t common = rd32(g_mem, staff + 0xc), second = rd32(g_mem, staff + 0x8);
    uint32_t at = common && g_mem[common] ? common : second;
    snprintf(out, size, "%s", at ? (const char *)(g_mem + at) : "");
}

/* The place of a slot on the pitch, from its position mask: the row from the position bits (GK at the
 * bottom, F at the top), the column from the side bits (0x80 left to 0x800 right). With more than one bit,
 * the average. Rows and columns in 1/1000 of the pitch. */
static void place(uint16_t mask, int *across, int *down)
{
    static const struct { uint16_t bit; int down; } rows[] = {
        {0x1, 925}, {0x2, 790}, {0x4, 700}, {0x8, 570}, {0x10, 445}, {0x20, 310}, {0x40, 160}};
    static const struct { uint16_t bit; int across; } sides[] = {
        {0x80, 100}, {0x100, 300}, {0x200, 500}, {0x400, 700}, {0x800, 900}};
    int n = 0, sum = 0;
    for (unsigned i = 0; i < sizeof rows / sizeof *rows; i++)
        if (mask & rows[i].bit) { sum += rows[i].down; n++; }
    *down = n ? sum / n : 500;
    n = sum = 0;
    for (unsigned i = 0; i < sizeof sides / sizeof *sides; i++)
        if (mask & sides[i].bit) { sum += sides[i].across; n++; }
    *across = n ? sum / n : 500;
}

/* By row, then across: the order in which the slots of one row spread over the pitch. */
typedef struct { int slot, across, down; } Spot;
static int by_place(const void *a, const void *b)
{
    const Spot *x = a, *y = b;
    if (x->down != y->down) return x->down - y->down;
    if (x->across != y->across) return x->across - y->across;
    return x->slot - y->slot;
}

/* The selection that the game stores for the club's first team, as staff indices (-1 for none): the
 * record [STORE + 0x601] + club index * 0x91, slots at +2 (0x8968f0, 0x896a90). For the log only. */
static void stored(uint32_t club, int *out)
{
    uint32_t record = rd32(g_mem, rd32(g_mem, STORE) + 0x601) + rd32(g_mem, club) * 0x91u;
    for (int s = 0; s < SLOTS; s++) out[s] = (int32_t)rd32(g_mem, record + 2 + 4 * (uint32_t)s);
}

static int log_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_VIEWS_LOG");
        on = v && *v == '1';
    }
    return on;
}

/* The number on the screen for a picker value (Pick.score): the score value / 1200, rounded, 1 to 20.
 * 0 for a player with no score (every key attribute hidden): the screen shows "-". */
static int shown_score(int value) { return pick_shown(value); }

static void log_draw(uint32_t club, int chosen, const Tactic *t, const Pick *p)
{
    if (!log_on()) return;
    int sel[SLOTS];
    stored(club, sel);
    fprintf(stderr, "recomp: best xi club %#x tactic %d \"%s\" masks", club, chosen, t->name);
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %x", t->masks[s]);
    fprintf(stderr, " staff");
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %d", p->staff[s] ? (int32_t)rd32(g_mem, p->staff[s]) : -1);
    fprintf(stderr, " fit");
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %d", p->fit[s]);
    fprintf(stderr, " selection");
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %d", sel[s]);
    fprintf(stderr, " score");
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %d", p->score[s]);
    fprintf(stderr, " shown");
    for (int s = 0; s < SLOTS; s++) fprintf(stderr, " %d", shown_score(p->score[s]));
    fprintf(stderr, "\n");
}

/* The tactics that one page of the Formation popup holds. A popup holds at most 30 widgets (0x4033fa) and
 * opens under its button when it fits above the screen's bottom (0x4039b5), so the list goes in pages
 * of 28 at most, with an item for the page before and the page after, as the squad drop-down of the
 * Compare tab does. RECOMP_VIEWS_FORMATION_PAGE sets the size for a test. */
static int formation_page_size(void)
{
    const char *v = getenv("RECOMP_VIEWS_FORMATION_PAGE");
    if (v && atoi(v) > 0) return atoi(v) > 28 ? 28 : atoi(v);
    int bottom = (BUTTON_BOTTOM * (int)rt_screen_h + 599) / 600;
    int n = ((int)rt_screen_h - bottom - 12) / ITEM_PX - 2;
    return n > 28 ? 28 : n < 4 ? 4 : n;
}

static void formation_item(uint32_t buf, uint32_t popup, const char *text, uint32_t data)
{
    win_put_str(buf, text, VIEWS_TEXT_SIZE);
    uint32_t w[18] = {2, 0, 0, 0, 0, 0, 0, 0x10, rd16(g_mem, POPUP_COLOUR), rd16(g_mem, ITEM_HIGHLIGHT), 1, 1,
                      rd16(g_mem, ITEM_TEXT), buf, 0, FORMATION_ID, data, popup};
    call_this(WIDGET_CREATE, display(), 18, w);
}

/* The Formation button in the heading, as the Print button of the club screen (0x45641b..0x4566a6), with
 * the chosen tactic's name, and its drop-down with the tactics of the list, a page at a time when the
 * list does not fit. */
static void formation(uint32_t buf, const Tactic *list, int n, int chosen, uint32_t bar)
{
    uint32_t fg = bar == rd16(g_mem, ITEM_TEXT) ? rd16(g_mem, BUTTON_TEXT) : rd16(g_mem, ITEM_TEXT);
    win_put_str(buf, list[chosen].name, VIEWS_TEXT_SIZE);
    uint32_t b[12] = {VIEWS_FORMATION_X1, 15, 785, BUTTON_BOTTOM, 0x30, bar, 0xc, 1, fg, buf, 0, 0};
    uint32_t button = (uint32_t)(int32_t)(int16_t)call_this(BUTTON, SCREENS, 12, b);
    uint32_t a[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0x130, rd16(g_mem, POPUP_COLOUR), button};
    uint32_t popup = (uint32_t)(int32_t)(int16_t)call_this(AREA_CREATE, display(), 11, a);
    int size = formation_page_size(), paged = n > size, page = paged ? (int)node_slot(FORM_PAGE) : 0;
    if (!paged) size = n;
    else if (page < 0 || page * size >= n) page = 0;
    if (paged && page > 0) formation_item(buf, popup, "Earlier formations", PAGE_ITEM + (uint32_t)page - 1);
    for (int i = page * size; i < n && i < (page + 1) * size; i++)
        formation_item(buf, popup, list[i].name, (uint32_t)i + 1);
    if (paged && (page + 1) * size < n) formation_item(buf, popup, "More formations", PAGE_ITEM + (uint32_t)page + 1);
}

/* A length of screen pixels in classic numbers, across and down (the widget create scales them back). */
static int across_px(int px) { return (px * 800 + (int)rt_screen_w - 1) / (int)rt_screen_w; }
static int down_px(int px) { return (px * 600 + (int)rt_screen_h - 1) / (int)rt_screen_h; }

/* The position text of a position bit with its side bits: GK, SW, then D, DM, M, AM or F with L, C or R. */
static void position_text(uint16_t mask, char *out, size_t size)
{
    static const struct { uint16_t bit; const char *name; } names[] = {
        {0x1, "GK"}, {0x2, "SW"}, {0x4, "D"}, {0x8, "DM"}, {0x10, "M"}, {0x20, "AM"}, {0x40, "F"}};
    const char *name = "-", *side = "";
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++)
        if (mask & names[i].bit) { name = names[i].name; break; }
    if (mask & 0x3) side = "";
    else if (mask & 0x80) side = "L";
    else if (mask & 0x800) side = "R";
    else if (mask & 0x700) side = "C";
    snprintf(out, size, "%s%s", name, side);
}

/* The order of the Squad depth rows: from the goalkeeper to the forwards (the lowest position bit), then
 * from left to right (the side bits 0x80 to 0x800), then the slot. */
static const Tactic *sort_tactic;
static int by_position(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    uint16_t mx = sort_tactic->masks[x], my = sort_tactic->masks[y];
    int px = mx & 0x7f, py = my & 0x7f;
    px &= -px;
    py &= -py;
    if (px != py) return px - py;
    int sx = mx & 0xf80, sy = my & 0xf80;
    sx &= -sx;
    sy &= -sy;
    if (sx != sy) return sx - sy;
    return x - y;
}

/* Half stars for a score 1 to 20: 1 to 10, 0 for a player with no score (every key attribute hidden). */
static int halves(int shown) { return shown ? (shown + 1) / 2 : 0; }

/* A widget of the original at its own rectangle (classic numbers), with no owner area. */
static int item(uint32_t buf, uint32_t flags, int x1, int y1, int x2, int y2, uint32_t style, uint32_t colour,
                uint32_t text_flags, uint32_t font, uint32_t text_colour, const char *text)
{
    win_put_str(buf, text, VIEWS_TEXT_SIZE);
    uint32_t w[18] = {flags, (uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 0, 0, style, colour, colour,
                      text_flags, font, text_colour, buf, 0, 0, 0, 0xffffffffu};
    return (int16_t)call_this(WIDGET_CREATE, display(), 18, w);
}

/* The width of a text in font 2, in classic numbers. */
static int width2(uint32_t buf, const char *text)
{
    win_put_str(buf, text, VIEWS_TEXT_SIZE);
    return across_px((int32_t)call(TEXT_WIDTH, 2, (uint32_t[2]){2, buf}));
}

/* One card: the dark panel, the position box, then up to three rows of a name and its stars. Classic numbers. */
static void card(uint32_t buf, int x1, int y1, int x2, int head, int row, const char *pos, uint32_t const *staff,
                 const int *score, const int *out, int n)
{
    int y2 = y1 + head + 3 * row + 4;
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 1, 0, 1, 0, 2, 0, 0xffffffffu};
    call_this(AREA_CREATE, display(), 11, a);
    item(buf, 1, x1 + 2, y1 + 2, x2 - 2, y1 + 2 + head, 0x30, rd16(g_mem, DARK_BLUE), 0xc, 1, rd16(g_mem, PALE), pos);
    int star_w = across_px(5 * 12 + 4 + 6);
    static const char *const tags[4] = {"", "Inj", "Int", "Out"};
    for (int r = 0; r < n && r < 3; r++) {
        int ry = y1 + 3 + head + r * row, nx = x1 + across_px(5), nx2 = x2 - star_w;
        char name[64];
        short_name(staff[r], name, sizeof name);
        uint32_t colour = r == 0 ? rd16(g_mem, ITEM_HIGHLIGHT) : rd16(g_mem, PALE);
        if (out[r] > 0 && out[r] < 4) {
            /* The tag after the name, the name cut to the room that is left. */
            int tw = width2(buf, tags[out[r]]) + across_px(4), room = nx2 - nx - tw;
            int w = width2(buf, name);
            /* A name wider than the room loses letters at its end and gets a full stop. */
            for (size_t len = strlen(name); w > room && len > 3; len--) {
                name[len - 1] = '.';
                name[len] = 0;
                if (len > 1 && name[len - 2] == ' ') continue;
                w = width2(buf, name);
            }
            item(buf, 1, nx, ry, nx + w + across_px(2), ry + row - 1, 1, 0, 1, 2, colour, name);
            item(buf, 1, nx + w + across_px(4), ry, nx2, ry + row - 1, 1, 0, 1, 2, rd16(g_mem, ORANGE), tags[out[r]]);
        } else {
            item(buf, 1, nx, ry, nx2, ry + row - 1, 1, 0, 1, 2, colour, name);
        }
        int st = item(buf, 1, x2 - star_w, ry + 2, x2 - across_px(5), ry + row - 3, 0, 0, 0xc, 1, 0, "");
        views_stars(st, halves(shown_score(score[r])));
    }
    if (log_on()) {
        fprintf(stderr, "recomp: best xi card pos %s rect %d %d %d %d staff", pos, x1, y1, x2, y2);
        for (int r = 0; r < n; r++) fprintf(stderr, " %d", (int32_t)rd32(g_mem, staff[r]));
        fprintf(stderr, " score");
        for (int r = 0; r < n; r++) fprintf(stderr, " %d", score[r]);
        fprintf(stderr, " halves");
        for (int r = 0; r < n; r++) fprintf(stderr, " %d", halves(shown_score(score[r])));
        fprintf(stderr, " out");
        for (int r = 0; r < n; r++) fprintf(stderr, " %d", out[r]);
        fprintf(stderr, "\n");
    }
}

/* The cards of the eleven on the pitch x1..y2 (classic numbers). */
static void cards(uint32_t buf, uint32_t club, uint32_t team, const Tactic *t, const Pick *p, int x1, int y1, int x2,
                  int y2)
{
    /* The original's pitch on its side; the cards go on its playing area. */
    uint32_t outs = buf + VIEWS_TEXT_SIZE - 16;
    uint32_t pa[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, outs, outs + 4, outs + 8, outs + 12, 1,
                       0xffffffffu, PITCH_SCALE};
    call(DRAW_PITCH, 11, pa);
    x1 = (int32_t)rd32(g_mem, outs);
    y1 = (int32_t)rd32(g_mem, outs + 4);
    x2 = (int32_t)rd32(g_mem, outs + 8);
    y2 = (int32_t)rd32(g_mem, outs + 12);
    /* The Squad depth rows: one per position text, the slots in pitch order. */
    int order[SLOTS], row_of[SLOTS], rows = 0, size[SLOTS] = {0}, nth[SLOTS] = {0};
    char pos[SLOTS][8];
    for (int s = 0; s < SLOTS; s++) order[s] = s;
    sort_tactic = t;
    qsort(order, SLOTS, sizeof *order, by_position);
    for (int i = 0; i < SLOTS; i++) {
        int s = order[i], r = 0;
        char text[8];
        position_text(t->masks[s], text, sizeof text);
        while (r < rows && strcmp(pos[r], text)) r++;
        if (r == rows) memcpy(pos[rows++], text, sizeof text);
        row_of[s] = r;
        nth[s] = size[r]++;
    }
    /* The next players of each slot: a Depth row for each slot (Q420). */
    int own_row[SLOTS];
    for (int s = 0; s < SLOTS; s++) own_row[s] = s;
    static Depth d;
    picker_depth(club, team, t->masks, human_manager(), p, own_row, SLOTS, &d);

    /* The columns: one per line of the formation, the goalkeeper on the left. */
    Spot spots[SLOTS];
    for (int s = 0; s < SLOTS; s++) {
        spots[s].slot = s;
        place(t->masks[s], &spots[s].across, &spots[s].down);
    }
    qsort(spots, SLOTS, sizeof *spots, by_place);       /* by down: the forwards first */
    int columns = 0;
    for (int i = 0; i < SLOTS; i++)
        if (!i || spots[i].down != spots[i - 1].down) columns++;
    int most = 1;
    for (int i = 0, k = 1; i < SLOTS; i++, k++) {
        if (i + 1 == SLOTS || spots[i + 1].down != spots[i].down) {
            if (k > most) most = k;
            k = 0;
        }
    }
    int w = x2 - x1, h = y2 - y1, gap = across_px(8);
    int card_w = w / columns - gap, widest = across_px(230);
    if (card_w > widest) card_w = widest;
    /* Rows of font 2 (18 pixels) and a position box of font 1, smaller when the most cards of a column do not
     * fit. The fonts do not scale with the screen. */
    int row = down_px(19), head = down_px(14);
    while ((head + 3 * row + 4 + down_px(6)) * most > h && row > down_px(14)) row--;
    int card_h = head + 3 * row + 4;
    if (log_on())
        fprintf(stderr, "recomp: best xi pitch %d %d %d %d columns %d most %d card %d x %d\n", x1, y1, x2, y2, columns,
                most, card_w, card_h);
    for (int i = 0, c = columns - 1; i < SLOTS; c--) {
        int j = i;
        while (j < SLOTS && spots[j].down == spots[i].down) j++;
        int k = j - i, cx = x1 + w * (2 * c + 1) / (2 * columns);
        for (int m = i; m < j; m++) {
            int s = spots[m].slot, r = row_of[s], cy = y1 + h * (2 * (m - i) + 1) / (2 * k);
            char label[12];
            if (size[r] == 1) snprintf(label, sizeof label, "%s", pos[r]);
            else if (size[r] == 2) snprintf(label, sizeof label, "%s%s", pos[r], nth[s] ? "R" : "L");
            else if (size[r] == 3) snprintf(label, sizeof label, "%s%s", pos[r], nth[s] == 0 ? "L" : nth[s] == 2 ? "R" : "");
            else snprintf(label, sizeof label, "%s %d", pos[r], nth[s] + 1);
            /* The slot's player, then the next two of the slot's own row (Q420). */
            uint32_t staff[3];
            int score[3], out[3], n = 0;
            if (p->staff[s]) {
                staff[n] = p->staff[s];
                score[n] = p->score[s];
                out[n++] = 0;
            }
            for (int q = 0; q < d.n[s] && n < 3; q++) {
                if (d.staff[s][q] == p->staff[s]) continue;
                staff[n] = d.staff[s][q];
                score[n] = d.score[s][q];
                out[n++] = d.out[s][q];
            }
            int top = cy - card_h / 2;
            if (top < y1 + 2) top = y1 + 2;
            if (top + card_h > y2 - 2) top = y2 - 2 - card_h;
            card(buf, cx - card_w / 2, top, cx + card_w / 2, head, row, label, staff, score, out, n);
        }
        i = j;
    }
}

int best_xi_build(uint32_t club, uint32_t team, uint32_t buf, int x1, int y1, int x2, int y2, uint32_t bar)
{
    static Tactic list[VIEWS_TACTICS];
    int n = tactics_list(human_manager(), list, VIEWS_TACTICS);
    if (n <= 0) return 0;
    int chosen = (int)node_slot(CHOSEN);
    if (chosen < 0 || chosen >= n) chosen = 0;
    formation(buf, list, n, chosen, bar);
    static int listed;
    if (!listed++ && getenv("RECOMP_VIEWS_LOG")) fprintf(stderr, "recomp: best xi tactics %d current 0\n", n);
    Pick p;
    picker_compute_view(club, team, 1, list[chosen].masks, human_manager(), &p);
    log_draw(club, chosen, &list[chosen], &p);
    cards(buf, club, team, &list[chosen], &p, x1, y1, x2, y2);
    return n;
}

int best_xi_input(uint32_t id, uint32_t data)
{
    if (id != FORMATION_ID || data < 1) return 0;
    if (data >= PAGE_ITEM) set_slot(FORM_PAGE, data - PAGE_ITEM);    /* a page item: only the page changes */
    else set_slot(CHOSEN, data - 1);
    return 1;
}

void views_position_text(uint16_t mask, char *out, size_t size) { position_text(mask, out, size); }
