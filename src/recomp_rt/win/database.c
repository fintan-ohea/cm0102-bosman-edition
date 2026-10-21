/* The season lines of the Setup Game screen (#39, then 0.6.0 slice 13, then menu B of Max's play test).
 *
 * The Setup Game screen (build 0x81bab0, input 0x81bd70) fills 9 of the 25 menu items on its stack and calls
 * the menu builder 0x5ed300 (0x81bcc5) with (110, 145, 780, 535, 2 columns, 6 rows, font 3, count 9, items,
 * 0, 0). Menu B (Max chose it from mockups after he played the 0.6.0 preview):
 * - the season strip: one wide button where the sub-heading "Setup Game" was (0x803f80 at 0x81bb5f, which
 *   the wrap leaves out). It shows the season, the database and the changing rules state of the next new
 *   game, and a click opens the Choose Season screen (season_screen.c), which holds the changing rules switch
 *   now. The strip takes the place of the Choose Season item and the "Changing rules" item of 0.6.0 slice 13;
 * - a grid of 2 columns and 4 rows: items 0 to 5 of the original (Start New Game to Game Settings), then
 *   "Bosman Edition Options" (options_screen.c, since 0.5.0) and "Extras" in place of items 6 to 8;
 * - Extras: a screen of the runtime with the original's items 6 to 8, Hall Of Fame, Game Credits and Web
 *   Sites. A click on one goes to the original input 0x81bd70 with the same ids (7, 8, 9), so each opens as
 *   it did from Setup Game.
 * The rows are 80 pixels high from y 145: rows 0 to 2 still hold the click points of the original's rows of
 * 65 pixels (y 177, 242 and 307), so the test scripts of the start of a game do not change.
 * Item layout: +0x00 s16 font (-1 = default), +0x02 s16 text colour (0 = default), +0x04 text,
 * +0xcc id, +0xd0 data (docs/research/2026-10-13-screen-stack-inventory.md section 6a).
 *
 * At each build the season of the next new game comes again from the options database and season
 * (season_screen_year), so an old settings file with database = 3.9.60, 3.9.65 or folder:<name> starts that
 * data set, in the season of that data. The test variable RECOMP_START_SEASON (season.c) wins over them.
 *
 * The lines change no game state: a saved game loads with its own season and data set. The strip, the
 * Extras items and their texts are in runtime memory (win_static_alloc), not in the game's heap.
 *
 * The red title of the start screens: the original formats "Championship Manager 2001/02" (0xa80288) into
 * 0xdd70a8 and passes it to the heading 0x803f30 (0x81bb24..0x81bb30 on Setup Game, 0x81d0b8..0x81d0e9 on
 * the new game screens). The heading wrap shows "Championship Manager: Bosman Edition" in its place (Max's
 * play test of 0.6.0). The title is a text, not a picture. The widget keeps its text in the widget record
 * (+0x80), so the longer text takes no game memory. */
#include "win.h"
#include "draw.h"
#include "options.h"

#include "dbfolders.h"
#include "season.h"
#include "season_dates.h"
#include "season_screen.h"
#include "seasons_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint32_t rt_site_setup_menu, rt_site_setup_subheading;

#define WIDGETS 0xb77c94u + 0xba95eu   /* the widgets of screen manager 0 (names.c) */
#define WIDGET_SIZE 0x18cu
#define MAX_WIDGETS 1200
#define ITEM_SIZE 0xd4u
#define ITEM_TEXT_SIZE 0xc8u
#define SEASON_ID 0x7d39         /* the action id of the item; the original uses no such id */
#define OPTIONS_ID 0x7d3d        /* the item that opens the Bosman Edition Options box (options_screen.c) */
#define EXTRAS_ID 0x7d3f         /* the item that opens Extras */
#define FIRST_EXTRA 6            /* items 6 to 8 of the original: Hall Of Fame, Game Credits, Web Sites (ids 7 to 9) */
#define EXTRAS 3

#define SCREENS 0xb74450u        /* the screen stack S */
#define TOP_NODE (SCREENS + 0x7d4u + 0x0eu)
#define NODE_PREV 0x1f4u
#define NODE_TITLE 0x1fcu
#define PUSH 0x7fe600u           /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define SET_PICTURE 0x41c0d0u    /* cdecl (path): background_set_picture */
#define HEADING 0x803f30u        /* thiscall on S (bar colour, text colour, text) */
#define SUB_HEADING 0x803f80u    /* thiscall on S (text, unused) */
#define SIDE_BAR 0x75db00u       /* cdecl (mode, arg) */
#define MENU 0x5ed300u           /* cdecl, 11 args: the menu builder */
#define TEXT_WIDTH 0x5e5ad0u     /* cdecl (font, text): the width in screen pixels */
#define TEXT_YELLOW 0xaebdecu
#define NO_DRAW 0xaebe0cu        /* not 0: the draw functions of the original draw nothing */
#define DRAW_SURFACE 0xaebde4u   /* the surface that the draw functions of the original draw on */
#define BACK_BAR 0x5edcc0u       /* cdecl (back, next) */
#define CANNOT_GO_BACK 0x7fec60u /* thiscall on S (any): 0 when there is a screen to go back to */
#define SIDE_BAR_INPUT 0x7615d0u /* cdecl (widget): 0 when it does not handle the click */
#define GAME_INPUT 0x7643d0u     /* cdecl (widget): the game's own actions, Back among them */
#define TEXT_LIGHT 0xae3184u   /* u16 colours of the original (include/gui_utils.h) */
#define HEADING_RED 0xae31a8u

#define MENU_Y 145               /* the grid: the original's top, rows of ROW_H pixels (classic numbers) */
#define ROW_H 80
#define STRIP_Y1 78              /* the season strip, where the sub-heading was (100, 80)-(790, 125) */
#define STRIP_Y2 137

/* Runtime memory for the strip text and the Extras items (made at the first Setup Game build). */
static uint32_t strip_item, extras_items, measure;
/* The places of "On" and "Off" in the strip's second line, screen pixels from its start ([1] On, [0] Off). */
static struct { int set, full, x[2], w[2]; } strip_frame;
#define EXTRAS_TEXT 0x100u      /* the text buffer of the Extras build */

static uint32_t call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static void fill_item(uint32_t item, const char *text, uint32_t id)
{
    wr16(g_mem, item, 0xffff);
    wr16(g_mem, item + 2, 0);
    win_put_str(item + 4, text, ITEM_TEXT_SIZE);
    wr32(g_mem, item + 0xcc, id);
    wr32(g_mem, item + 0xd0, 0);
}

#define TITLE_ORIGINAL "Championship Manager 2001/02"
#define TITLE_BOSMAN "Championship Manager: Bosman Edition"

void o_00803f30(void);

/* The heading, thiscall on S (bar colour, text colour, text), ret 0xc. */
RT_WRAP(00803f30, heading)
{
    static uint32_t title;
    uint32_t at = rt_arg(2);
    if (at && !strcmp(win_str(at), TITLE_ORIGINAL) && !getenv("RECOMP_SETUP_TEXT")) {
        if (!title) {
            title = win_static_alloc(sizeof TITLE_BOSMAN);
            win_put_str(title, TITLE_BOSMAN, sizeof TITLE_BOSMAN);
        }
        wr32(g_mem, cpu.esp + 4 + 4 * 2, title);
    }
    o_00803f30();
}

void o_005ed300(void);

/* The sub-heading, thiscall on S (text, unused), ret 8. Setup Game's "Setup Game" makes way for the strip. */
void o_00803f80(void);
RT_WRAP(00803f80, sub_heading)
{
    if (rd32(g_mem, cpu.esp) == rt_site_setup_subheading && !getenv("RECOMP_SETUP_TEXT")) {
        rt_ret_stdcall(0xffffffffu, 2);
        return;
    }
    o_00803f80();
}

/* The first line of the strip: the season and the database (and the Custom note). */
static void strip_prefix(char *text, size_t room)
{
    int year = season_start_year();
    char db[DBFOLDER_NAME];
    season_screen_database_text(db, dbfolders_label_room(rt_screen_w));
    snprintf(text, room, "Season %d/%02d \xb7 %s", year, (year + 1) % 100, db);
    /* Slice 11b: a Custom database in another season whose dates could not be checked keeps its dates. */
    const char *opt = options_string(OPT_DATABASE);
    char path[1100];
    int shift;
    int unchecked = season_code_on() && year != SEASON_FIRST_YEAR && !strncmp(opt, "folder:", 7) &&
                    dbfolders_path(win_databases_root(), opt + 7, path, sizeof path) && !seasons_db_identify(path) &&
                    !season_dates_custom_check(path, &shift);
    if (unchecked) snprintf(text + strlen(text), room - strlen(text), " \xb7 Ages not checked");
}

#define RULES_TEXT "Changing rules:   "

static uint32_t text_width(uint32_t at, const char *s)
{
    win_put_str(at, s, ITEM_TEXT_SIZE);
    return call(TEXT_WIDTH, 2, (uint32_t[2]){3, at});
}

/* The strip: a menu of one item, made by the original's menu builder in the highlight style (bit 0 of the
 * highlight bits: style 0x822, yellow text, 0x5ed300), with two lines: the season and its database, then
 * "Changing rules:   On   Off". The runtime draws a yellow frame round the value of the next new game
 * (setup_widget_drawn), as the original's Game Settings box frames the current value (0x82477b).
 *
 * Same game (Q152): the game's widgets and texts are the same with changing rules on and off, so the
 * same-save runs rules-off and off equal normal. Heap traces showed why: a text that changed with the state
 * moved the original's text engine's work memory (docs/research/2026-10-16-hd-text.md), and a frame widget
 * of the game that changed with the state swapped two blocks. The widths below are measured the same way in
 * both states. */
static void make_strip(void)
{
    if (!strip_item) strip_item = win_static_alloc(ITEM_SIZE);
    if (!measure) measure = win_static_alloc(ITEM_TEXT_SIZE);
    char first[ITEM_TEXT_SIZE], text[ITEM_TEXT_SIZE];
    strip_prefix(first, sizeof first);
    snprintf(text, sizeof text, "%s\n" RULES_TEXT "On   Off", first);
    fill_item(strip_item, text, SEASON_ID);
    call(MENU, 11, (uint32_t[11]){110, STRIP_Y1, 780, STRIP_Y2, 1, 1, 3, 1, strip_item, 1, 0});

    const char *prefix = RULES_TEXT, *before_off = RULES_TEXT "On   ";
    int full = (int)text_width(measure, RULES_TEXT "On   Off"), on_x = (int)text_width(measure, prefix);
    int off_x = (int)text_width(measure, before_off), on_w = (int)text_width(measure, "On");
    int off_w = (int)text_width(measure, "Off");
    strip_frame.full = full;
    strip_frame.x[1] = on_x;
    strip_frame.w[1] = on_w;
    strip_frame.x[0] = off_x;
    strip_frame.w[0] = off_w;
    strip_frame.set = 1;
}

/* After the game draws a widget (names.c, through views_widget_drawn): on the strip, the yellow frame round
 * the value of the next new game, drawn by the runtime on the game's draw surface (draw.c), so the screen
 * dumps hold it too. */
void setup_widget_drawn(uint32_t w)
{
    if (!strip_frame.set || (int16_t)rd16(g_mem, w + 8) != SEASON_ID || rd32(g_mem, NO_DRAW)) return;
    uint32_t surface = rd32(g_mem, DRAW_SURFACE);
    if (!surface) return;
    DrawTarget t = {g_mem + rd32(g_mem, surface + 0x14), rd32(g_mem, surface + 0x10),
                    (int)rd32(g_mem, surface + 8), (int)rd32(g_mem, surface + 0xc)};
    int x1 = (int32_t)rd32(g_mem, w + 0x10), y1 = (int32_t)rd32(g_mem, w + 0x14);
    int x2 = (int32_t)rd32(g_mem, w + 0x18), y2 = (int32_t)rd32(g_mem, w + 0x1c);
    int v = season_code_on() && season_screen_rules_on();
    int start = x1 + (x2 - x1 + 1 - strip_frame.full) / 2, cy = (y1 + y2) / 2 + 10;   /* the second line */
    int fx1 = start + strip_frame.x[v] - 5, fx2 = start + strip_frame.x[v] + strip_frame.w[v] + 4;
    int fy1 = cy - 11, fy2 = cy + 10;
    uint16_t c = rd16(g_mem, TEXT_YELLOW);
    draw_line(&t, fx1, fy1, fx2, fy1, c);
    draw_line(&t, fx2, fy1, fx2, fy2, c);
    draw_line(&t, fx2, fy2, fx1, fy2, c);
    draw_line(&t, fx1, fy2, fx1, fy1, c);
}

/* The menu builder, cdecl (x1, y1, x2, y2, columns, rows, font, count, items, highlight bits, sort). */
RT_WRAP(005ed300, menu_build)
{
    if (rd32(g_mem, cpu.esp) == rt_site_setup_menu && rt_arg(7) == 9) {
        if (season_code_on() && (!getenv("RECOMP_START_SEASON") || !*getenv("RECOMP_START_SEASON")))
            season_set_new_game(season_screen_year(), season_screen_rules_choice());
        uint32_t item = rt_arg(8);
        const char *old = getenv("RECOMP_SETUP_TEXT");
        if (old && !strcmp(old, "0.5.0")) {
            /* Test only (Q342): the Setup Game text of 0.5.0, the Database button and the options item, so a
             * run shows that only this text moves memory against the 0.5.0 build. */
            const char *d = options_string(OPT_DATABASE);
            char text[ITEM_TEXT_SIZE];
            snprintf(text, sizeof text, "Database: %s", !strcmp(d, "3.9.60") ? "2001/02 (3.9.60)"
                                                        : !strcmp(d, "3.9.65") ? "2001/02 Dec (3.9.65)"
                                                                               : "2002/03 (3.9.68)");
            fill_item(item + 9 * ITEM_SIZE, text, SEASON_ID);
            fill_item(item + 10 * ITEM_SIZE, "Bosman Edition Options", OPTIONS_ID);
            wr32(g_mem, cpu.esp + 4 + 4 * 7, 11);
            o_005ed300();
            return;
        }
        if (old && !strcmp(old, "0.6.0-preview")) {
            /* Test only: the Setup Game of 0.6.0 slice 13 (the 0.6.0 preview, c3cb6ab): the Choose Season item,
             * the options item and the Changing rules item in a grid of 2 x 6, with the original's title and
             * sub-heading. A run with it shows that only Setup Game moves memory against the preview build. */
            int year = season_start_year();
            char db[DBFOLDER_NAME], text[ITEM_TEXT_SIZE];
            season_screen_database_text(db, dbfolders_label_room(rt_screen_w));
            snprintf(text, sizeof text, "Choose Season: %d/%02d\n%s", year, (year + 1) % 100, db);
            fill_item(item + 9 * ITEM_SIZE, text, SEASON_ID);
            fill_item(item + 10 * ITEM_SIZE, "Bosman Edition Options", OPTIONS_ID);
            fill_item(item + 11 * ITEM_SIZE,
                      season_code_on() && season_screen_rules_on() ? "Changing rules: On" : "Changing rules: Off", 0);
            wr32(g_mem, cpu.esp + 4 + 4 * 7, 12);
            o_005ed300();
            return;
        }
        /* Keep the original's items 6 to 8 for Extras, then put Options and Extras in their cells. */
        if (!extras_items) extras_items = win_static_alloc(EXTRAS * ITEM_SIZE);
        for (uint32_t i = 0; i < EXTRAS * ITEM_SIZE; i++)
            g_mem[extras_items + i] = g_mem[item + FIRST_EXTRA * ITEM_SIZE + i];
        fill_item(item + 6 * ITEM_SIZE, "Bosman Edition Options", OPTIONS_ID);
        fill_item(item + 7 * ITEM_SIZE, "Extras", EXTRAS_ID);
        make_strip();
        wr32(g_mem, cpu.esp + 4 + 4 * 3, MENU_Y + 4 * ROW_H);   /* y2 */
        wr32(g_mem, cpu.esp + 4 + 4 * 5, 4);                     /* rows */
        wr32(g_mem, cpu.esp + 4 + 4 * 7, 8);                     /* count */
    }
    o_005ed300();
}

/* ---- Extras: Hall Of Fame, Game Credits and Web Sites ---- */

static uint32_t extras_text(const char *s)
{
    static uint32_t at;
    if (!at) at = win_static_alloc(EXTRAS_TEXT);
    win_put_str(at, s, EXTRAS_TEXT);
    return at;
}

static void extras_build(void)
{
    /* The frame of the start screen 0x81bab0: its picture (the node title of Setup Game), heading, side bar. */
    uint32_t node = rd32(g_mem, TOP_NODE), prev = node ? rd32(g_mem, node + NODE_PREV) : 0;
    char title[260] = "";
    if (prev) snprintf(title, sizeof title, "%.259s", win_str(prev + NODE_TITLE));
    call(SET_PICTURE, 1, (uint32_t[1]){extras_text(title)});
    call_this(HEADING, SCREENS, 3,
              (uint32_t[3]){rd16(g_mem, HEADING_RED), rd16(g_mem, TEXT_LIGHT), extras_text(TITLE_BOSMAN)});
    call(SIDE_BAR, 2, (uint32_t[2]){4, 0});
    call_this(SUB_HEADING, SCREENS, 2, (uint32_t[2]){extras_text("Extras"), 0});
    call(MENU, 11, (uint32_t[11]){110, MENU_Y, 780, MENU_Y + 2 * ROW_H, 2, 2, 3, EXTRAS, extras_items, 0, 0});
    uint32_t back = call_this(CANNOT_GO_BACK, SCREENS, 1, (uint32_t[1]){0}) == 0;
    call(BACK_BAR, 2, (uint32_t[2]){back, 0});
    fprintf(stderr, "recomp: extras built\n");
    rt_ret_cdecl(0);
}

void o_0081bd70(void);

static void extras_input(void)
{
    int clicked = (int16_t)rt_arg(0);
    int id = clicked >= 0 && clicked < MAX_WIDGETS ? (int16_t)rd16(g_mem, WIDGETS + WIDGET_SIZE * (uint32_t)clicked + 8)
                                                   : 0;
    if (id >= FIRST_EXTRA + 1 && id <= FIRST_EXTRA + EXTRAS) {
        o_0081bd70();       /* the original's own action for ids 7 to 9; it returns for this call */
        return;
    }
    uint32_t a[1] = {rt_arg(0)};
    uint32_t r = call(SIDE_BAR_INPUT, 1, a);
    if (!r) r = call(GAME_INPUT, 1, a);
    rt_ret_cdecl(r);
}

static uint32_t extras_build_fn(void) { return rt_host_fn("extras_build", extras_build); }
uint32_t setup_extras_build_fn(void) { return extras_build_fn(); }

static void extras_open(void)
{
    uint32_t p[5] = {extras_build_fn(), rt_host_fn("extras_input", extras_input), 0, 0, 0};
    call_this(PUSH, SCREENS, 5, p);
    fprintf(stderr, "recomp: extras opened\n");
}

/* The Setup Game input, cdecl (clicked widget or -1). */
RT_WRAP(0081bd70, setup_input)
{
    int clicked = (int16_t)rt_arg(0);
    int id = clicked >= 0 && clicked < MAX_WIDGETS ? (int16_t)rd16(g_mem, WIDGETS + WIDGET_SIZE * (uint32_t)clicked + 8)
                                                   : 0;
    if (id == OPTIONS_ID) {
        options_screen_open(1);
        rt_ret_cdecl((uint32_t)-4);
        return;
    }
    if (id == SEASON_ID) {
        if (season_code_on()) season_screen_open();     /* RECOMP_SEASON_CODE=off: the item does nothing */
        rt_ret_cdecl((uint32_t)-4);
        return;
    }
    if (id == EXTRAS_ID) {
        extras_open();
        rt_ret_cdecl((uint32_t)-4);
        return;
    }
    o_0081bd70();
}
