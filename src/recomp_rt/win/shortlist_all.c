/* Shortlist All (fork of fintan-ohea; docs/specs/2026-10-25-shortlist-all-spec.md).
 *
 * With shortlist_all = on, the Player & Staff Search screen (build 0x808690, input 0x80eb00) has a Shortlist All
 * button left of Filters and Delete. A click adds every person the current list shows to the manager's shortlist.
 *
 * The screen (docs/research/2026-10-25-shortlist-all.md): node slot 0 is the SearchEditSession. Its byte +0 is the
 * current list and its ten PLAYER_SEARCH records of 0x2b0 bytes start at +1, each with its list type at +0xa
 * (1 the Short List; SearchEditSession::get_list_type 0x80eaa0). get_current_results 0x804a40 gives the LLIST at
 * +0x361e that the screen shows: PLAYER_SEARCH::get_result_list 0x7af160 adds only the results that pass the
 * filters (0x807450 at 0x7af40d), one 0x2d-byte entry each with the staff id at +0x0c. An LLIST is a u16 count at
 * +0, the head node at +2; a node holds its data at +0 and the next node at +6 (0x68d0d0, 0x68d1b0).
 *
 * The add: SearchEditSession::add_player_to_short_list 0x804590 takes an undo copy of the ten searches, adds the
 * person to the Short List (PLAYER_SEARCH::add_player 0x7af720), marks it changed and clears the result cache. It
 * runs for the first person only, so the batch has one undo copy; the others go to add_player of the Short List
 * record at once. A person already on it is left out (is_player_on_short_list 0x804540), and so is one that the
 * profile's Action drop-down would not offer to shortlist (staff_screens_build_profile_screen 0x870700 at
 * 0x8710cb..0x8711d6): outside the loaded leagues, at a club the manager runs, a board member, not a player, or
 * retired. A manager in state 5 (0x8711ad) adds nobody. After a click the button reads "Added n" or "None to add"
 * until the next click on the screen.
 *
 * The button: the area of Filters and Delete is made at 0x80acea (call site search_buttons_area, classic pixels
 * x 530 to 780) and Filters at 0x80ad2b (search_filters). After Filters the wrap makes a one-column area at the
 * left of that row with the button, in the style of Filters. The Short List itself gets no button.
 *
 * Same game (Q152): with the option off nothing is made and every call goes on unchanged. The text block is taken
 * at the first build of the screen with the option on or off, so the heap is the same. */
#include "win.h"
#include "options.h"

#include <stdio.h>

extern const uint32_t rt_site_search_buttons_area, rt_site_search_filters;

#define SCREENS 0xb74450u           /* the screen stack S */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define LIST_TYPE 0x80eaa0u         /* thiscall on the session (index, -1 the current list), returns the type */
#define RESULTS 0x804a40u           /* thiscall on the session (LLIST **out, int *count, compare) */
#define ON_SHORT_LIST 0x804540u     /* thiscall on the session (staff) */
#define ADD_TO_SHORT_LIST 0x804590u /* thiscall on the session (staff): with an undo copy */
#define ADD_PLAYER 0x7af720u        /* thiscall on a PLAYER_SEARCH (staff) */
#define DISPLAYS 0xb74c4cu
#define PLAYER_SLOT 0xb77c7au
#define WIDGET_ID 0xba966u          /* Display + this + w * 0x18c: the id of widget w */
#define WIDGET_SIZE 0x18cu
#define BUTTON_COLOUR 0xae317eu     /* the u16 colours of Filters (0x80ad06, 0x80ad00) */
#define BUTTON_TEXT 0xae3184u
#define STAFF_TABLE 0xae23bcu       /* the staff table, 0x6e bytes a person */
#define STAFF_SIZE 0x6eu
#define STAFF_COUNT 0xae2364u       /* the people of the loaded leagues: ids below it */
#define STAFF_CLUB 0x39u            /* staff +0x39: the club */
#define STAFF_PLAYER 0x61u          /* staff +0x61: the player record, 0 for a non-player */
#define MANAGERS 0xb74c26u          /* the human manager of a player slot, 0x300 bytes apart */
#define MANAGER_STATE 0x5fu         /* 5: the profile offers no shortlist (0x8711ad) */
#define CONTROLS_CLUB 0x600c90u     /* cdecl (club, 0, 1): the current human manager runs the club */
#define CONTRACTS 0xae1050u         /* CONTRACT_MANAGER */
#define NOT_BOARD 0x4cb960u         /* thiscall on CONTRACTS (staff): 0 for a board member */
#define REGEN 0xb7434cu             /* PLAYER_REGEN */
#define RETIREMENT 0x7a7eb0u        /* thiscall on REGEN (staff): 2 retired */

#define SEARCHES 1u                 /* the session's ten PLAYER_SEARCH records */
#define SEARCH_SIZE 0x2b0u
#define SEARCH_TYPE 0x0au
#define SHORT_LIST 1                /* the list type of the Short List (0x8045b9) */
#define RESULT_STAFF 0x0cu
#define SHORTLIST_ID 0x7d60u        /* the button's widget id: the screen does not use it */
#define REBUILD 0xfffffff5u
#define BUTTON_W 130                /* classic pixels, with a gap of 5 before Filters */

static uint32_t scratch;            /* the button text and the out values of RESULTS */
static int row_y1 = -1, row_y2, row_x1;
static int added = -1;              /* the people the last click added, shown until the next click; -1 none */
static uint32_t added_session;      /* the session of that click: another opening of the screen shows the name */

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

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static int current_type(uint32_t session)
{
    uint32_t a[1] = {0xffffffffu};
    return (int8_t)call_this(LIST_TYPE, session, 1, a);
}

static uint32_t short_list_of(uint32_t session)
{
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t s = session + SEARCHES + i * SEARCH_SIZE;
        if (rd8(g_mem, s + SEARCH_TYPE) == SHORT_LIST) return s;
    }
    return 0;
}

/* The manager may shortlist: the profile offers Add To Shortlist only when his state is not 5 (0x8711ad). */
static int manager_may_shortlist(void)
{
    uint32_t m = rd32(g_mem, MANAGERS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
    return m && rd8(g_mem, m + MANAGER_STATE) != 5;
}

/* The person may go on the shortlist: the tests of the profile's Action drop-down before Add To Shortlist
 * (staff_screens_build_profile_screen 0x870700, 0x8710cb..0x8711d6). A person of the loaded leagues, not at a club
 * the manager runs, not a board member, a player, and not retired. */
static int may_shortlist(uint32_t staff)
{
    if (rd32(g_mem, staff) >= rd32(g_mem, STAFF_COUNT) || !rd32(g_mem, staff + STAFF_PLAYER)) return 0;
    uint32_t club = rd32(g_mem, staff + STAFF_CLUB);
    uint32_t c[3] = {club, 0, 1};
    if (club && call(CONTROLS_CLUB, 3, c)) return 0;
    uint32_t a[1] = {staff};
    if (!call_this(NOT_BOARD, rd32(g_mem, CONTRACTS), 1, a)) return 0;
    return (uint8_t)call_this(RETIREMENT, rd32(g_mem, REGEN), 1, a) != 2;
}

/* Adds every person of the current list to the Short List. Returns the count added, -1 when nothing could. */
static int add_all(uint32_t session)
{
    uint32_t list_at = short_list_of(session);
    if (!list_at || current_type(session) == SHORT_LIST || !manager_may_shortlist()) return -1;
    uint32_t r[3] = {scratch, scratch + 4, 0};
    call_this(RESULTS, session, 3, r);
    uint32_t list = rd32(g_mem, scratch), staff_table = rd32(g_mem, STAFF_TABLE);
    if (!list || !staff_table) return -1;
    int shown = 0, n = 0, tried = 0;
    for (uint32_t node = rd32(g_mem, list + 2); node && !rt_unwinding; node = rd32(g_mem, node + 6)) {
        uint32_t entry = rd32(g_mem, node);
        if (!entry) continue;
        shown++;
        uint32_t staff[1] = {staff_table + rd32(g_mem, entry + RESULT_STAFF) * STAFF_SIZE};
        if (call_this(ON_SHORT_LIST, session, 1, staff) || !may_shortlist(staff[0])) continue;
        if (tried++ == 0) {
            if (!call_this(ADD_TO_SHORT_LIST, session, 1, staff)) return -1;
        } else {
            call_this(ADD_PLAYER, list_at, 1, staff);
        }
        if (call_this(ON_SHORT_LIST, session, 1, staff)) n++;
    }
    fprintf(stderr, "recomp: shortlist all: list %d, %d shown, %d added\n", rd8(g_mem, session), shown, n);
    return n;
}

/* The area create (display.c), before the classic pixels are scaled: the row of Filters and Delete. */
void shortlist_all_area_before(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_search_buttons_area) return;
    row_x1 = (int)rt_arg(0);
    row_y1 = (int)rt_arg(1);
    row_y2 = (int)rt_arg(3);
}

static void make_button(uint32_t session)
{
    char s[32];
    if (session != added_session) added = -1;
    if (added > 0) snprintf(s, sizeof s, "Added %d", added);
    else if (added == 0) snprintf(s, sizeof s, "None to add");
    else snprintf(s, sizeof s, "Shortlist All");
    win_put_str(scratch + 8, s, 32);
    uint32_t a[11] = {(uint32_t)(row_x1 - BUTTON_W - 5), (uint32_t)row_y1, (uint32_t)(row_x1 - 5), (uint32_t)row_y2,
                      1, 0, 1, 0, 1, 0, 0xffffffffu};
    uint32_t area = (uint32_t)(int32_t)(int16_t)call_this(AREA_CREATE, display(), 11, a);
    uint32_t b[11] = {area, 0, 0, 0x30, rd16(g_mem, BUTTON_COLOUR), 0xc, 1, rd16(g_mem, BUTTON_TEXT), scratch + 8,
                      SHORTLIST_ID, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

void o_008040a0(void);

/* The cell button create (assistant.c): after Filters, Shortlist All. Returns 1 when it made Filters. */
int shortlist_all_cell_button(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_search_filters || row_y1 < 0) return 0;
    if (!scratch) scratch = rt_alloc_zero(64);     /* on and off: the same heap blocks */
    o_008040a0();
    if (rt_unwinding) return 1;
    uint32_t widget = cpu.eax, session = node_slot(0);
    if (options_bool(OPT_SHORTLIST_ALL) && session && current_type(session) != SHORT_LIST) make_button(session);
    row_y1 = -1;
    cpu.eax = widget;
    return 1;
}

void o_0080eb00(void);

/* The input of Player & Staff Search, cdecl (s16 widget). A click on Shortlist All adds the list. */
RT_WRAP(0080eb00, search_input)
{
    int16_t w = (int16_t)rt_arg(0);
    if (w < 0) {
        o_0080eb00();
        return;
    }
    uint32_t id = (uint16_t)rd32(g_mem, display() + WIDGET_ID + (uint32_t)w * WIDGET_SIZE);
    if (id != SHORTLIST_ID || !options_bool(OPT_SHORTLIST_ALL)) {
        added = -1;                                 /* any other click: the button reads Shortlist All again */
        o_0080eb00();
        return;
    }
    uint32_t session = node_slot(0);
    added = session ? add_all(session) : -1;
    added_session = session;
    rt_ret_cdecl(REBUILD);
}
