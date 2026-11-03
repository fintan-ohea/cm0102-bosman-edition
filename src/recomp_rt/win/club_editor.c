/* The club editor (fork of fintan-ohea; docs/specs/2026-10-25-club-editor-spec.md).
 *
 * With club_editor = on, the club screen (build 0x454900) has a sixth tab, Edit Club. The build hands the tab bar
 * 0x5ed780 ten entries of 0xd4 bytes at 0x455243 (call site club_tab_bar), five tabs on top and five links below,
 * and switches on node slot 4 (jump table 0x456724, cases 1..5; any other value shows an error box at 0x456381).
 * The wrap of the tab bar passes eleven entries, Edit Club (data 6) after Finances & Info, so the bar puts six
 * tabs on top ((count + 1) / 2, 0x5ed96c). A click on a tab is widget id 2 with its data, and the screen's input
 * 0x4671f0 puts the data into slot 4 (0x467701..0x467783), so the new tab needs nothing there. When slot 4 is 6
 * the wrap hands the build case 5 (esi, which the build keeps across the call), and the wrap of case 5's
 * draw_club_information_screen 0x460590 (cdecl (top, bottom)) draws the editor in its place.
 *
 * The values (docs/research/2026-10-25-club-editor.md): the finance record's balance (s64 at +0) and the funds
 * adjustment of editor.c; club +0x80 reputation (s16), +0x7f training facilities, +0x64 status (1 professional,
 * 2 semi-professional, 3 amateur), +0x6d owns the ground; the board's confidence, s16 at [[0xb67a44]] + club id
 * * 0x49 + 6 (MANAGER_MANAGER::get_board_confidence_band 0x690b70); the stadium (club +0x69): +0x3c capacity,
 * +0x40 seated, +0x44 expansion limit, +0x4c retractable roof, +0x4d under-soil heating. Each click changes the
 * game's memory at once and marks the game as edited (editor.c).
 *
 * Same game (Q152): with the option off the wrap passes the call on unchanged and the tab is never made. */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <string.h>

extern const uint32_t rt_site_club_tab_bar;

#define SCREENS 0xb74450u           /* the screen stack S */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define COLOUR 0x5e4800u            /* cdecl (0, green, 0, 0): an RGB565 colour */
#define IS_NATIONAL 0x53b900u       /* cdecl (club): not 0 for a national team (0x454efa) */
#define DISPLAYS 0xb74c4cu
#define PLAYER_SLOT 0xb77c7au
#define WIDGETS 0xba95eu
#define WIDGET_SIZE 0x18cu
#define TEXT_YELLOW 0xaebdecu
#define TEXT_LIGHT 0xae3154u

#define ENTRY_SIZE 0xd4u            /* a tab bar entry: s16 font, u16 colour, text, +0xcc id, +0xd0 data */
#define ENTRY_TEXT 0x04u
#define ENTRY_ID 0xccu
#define ENTRY_DATA 0xd0u
#define CLUB_ENTRIES 10
#define EDIT_TAB 6
#define INFO_TAB 5
#define TAB_ID 2u                   /* the widget id of a tab (0x467701) */
#define STEP_ID 0x7d50u             /* data: field * 4 + op (0 big down, 1 down, 2 up, 3 big up) */
#define REBUILD 0xfffffff5u

#define MANAGERS 0xb67a44u          /* MANAGER_MANAGER: [this] + club id * 0x49, +6 board confidence */
#define BOARD_SIZE 0x49u
#define CLUB_STATUS 0x64u
#define CLUB_STADIUM 0x69u
#define CLUB_OWNS 0x6du
#define CLUB_TRAINING 0x7fu
#define CLUB_REPUTATION 0x80u
#define STADIUM_CAPACITY 0x3cu
#define STADIUM_SEATED 0x40u
#define STADIUM_EXPANSION 0x44u
#define STADIUM_ROOF 0x4cu
#define STADIUM_HEATING 0x4du
#define CAPACITY_MAX 250000

enum { F_BALANCE, F_FUNDS, F_REPUTATION, F_TRAINING, F_STATUS, F_CONFIDENCE, F_CAPACITY, F_SEATED, F_EXPANSION,
       F_HEATING, F_ROOF, F_OWNS, FIELDS };

static uint32_t scratch, entries;
static int on_tab;                  /* the build that runs shows the Edit Club tab */

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

static void set_slot(int n, uint32_t v)
{
    uint32_t a[3] = {(uint32_t)n, v, 0};
    call_this(SET_SLOT, SCREENS, 3, a);
}

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static int is_club(uint32_t club)
{
    uint32_t a[1] = {club};
    return club && !call(IS_NATIONAL, 1, a);
}

/* ---- The values ---- */

static uint32_t stadium_of(uint32_t club) { return rd32(g_mem, club + CLUB_STADIUM); }

static uint32_t board_of(uint32_t club)
{
    uint32_t m = rd32(g_mem, MANAGERS);
    uint32_t base = m ? rd32(g_mem, m) : 0;
    return base ? base + rd32(g_mem, club) * BOARD_SIZE : 0;
}

static const char *const status_names[4] = {"Unknown", "Professional", "Semi-Professional", "Amateur"};

static int64_t clamp64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* The value of a field as a number, for the log and the steps; -1 when the club has none (no stadium). */
static int64_t get(uint32_t club, int field)
{
    uint32_t s = stadium_of(club), f = editor_finance(club), b = board_of(club);
    switch (field) {
    case F_BALANCE: return f ? (int64_t)rd64(g_mem, f) : -1;
    case F_FUNDS: return f ? editor_funds(f) : -1;
    case F_REPUTATION: return (int16_t)rd16(g_mem, club + CLUB_REPUTATION);
    case F_TRAINING: return rd8(g_mem, club + CLUB_TRAINING);
    case F_STATUS: return rd8(g_mem, club + CLUB_STATUS);
    case F_CONFIDENCE: return b ? (int16_t)rd16(g_mem, b + 6) : -1;
    case F_CAPACITY: return s ? (int32_t)rd32(g_mem, s + STADIUM_CAPACITY) : -1;
    case F_SEATED: return s ? (int32_t)rd32(g_mem, s + STADIUM_SEATED) : -1;
    case F_EXPANSION: return s ? (int32_t)rd32(g_mem, s + STADIUM_EXPANSION) : -1;
    case F_HEATING: return s ? rd8(g_mem, s + STADIUM_HEATING) : -1;
    case F_ROOF: return s ? rd8(g_mem, s + STADIUM_ROOF) : -1;
    case F_OWNS: return rd8(g_mem, club + CLUB_OWNS);
    }
    return -1;
}

static const char *const field_names[FIELDS] = {"balance", "funds", "reputation", "training", "status",
                                                "confidence", "capacity", "seated", "expansion", "heating", "roof",
                                                "owns"};

/* One click: op 0 and 3 are the big steps, 1 and 2 the small ones; a switch (heating, roof, owns) turns over. */
static void step(uint32_t club, int field, int op)
{
    static const int dir[4] = {-1, -1, 1, 1};
    int big = op == 0 || op == 3, d = dir[op];
    int64_t v0 = get(club, field);
    if (v0 < 0) return;
    uint32_t s = stadium_of(club), f = editor_finance(club), b = board_of(club);
    switch (field) {
    case F_BALANCE: {
        int64_t v = v0 + d * money_internal(big ? 10000000 : 1000000);      /* steps as shown */
        if (v > ((int64_t)1 << 50) || v < -((int64_t)1 << 50)) return;
        wr64(g_mem, f, (uint64_t)v);
        break;
    }
    case F_FUNDS: {
        int32_t id = (int32_t)rd32(g_mem, club);
        int64_t a = editor_adjustment(id) + d * money_internal(big ? 10000000 : 1000000);
        if (a > ((int64_t)1 << 50) || a < -((int64_t)1 << 50) || !editor_set_adjustment(id, a)) return;
        break;
    }
    case F_REPUTATION:
        wr16(g_mem, club + CLUB_REPUTATION, (uint32_t)clamp64(v0 + d * (big ? 1000 : 100), 1, 10000));
        break;
    case F_TRAINING:
        wr8(g_mem, club + CLUB_TRAINING, (uint32_t)clamp64(v0 + d * (big ? 5 : 1), 1, 20));
        break;
    case F_STATUS:
        wr8(g_mem, club + CLUB_STATUS, (uint32_t)clamp64(v0 + d, 1, 3));
        break;
    case F_CONFIDENCE:
        wr16(g_mem, b + 6, (uint32_t)clamp64(v0 + d * (big ? 1000 : 100), 1, 10000));
        break;
    case F_CAPACITY: {
        int64_t v = clamp64(v0 + d * (big ? 5000 : 500), 0, CAPACITY_MAX);
        wr32(g_mem, s + STADIUM_CAPACITY, (uint32_t)v);
        if ((int32_t)rd32(g_mem, s + STADIUM_SEATED) > v) wr32(g_mem, s + STADIUM_SEATED, (uint32_t)v);
        break;
    }
    case F_SEATED:
        wr32(g_mem, s + STADIUM_SEATED,
             (uint32_t)clamp64(v0 + d * (big ? 5000 : 500), 0, (int32_t)rd32(g_mem, s + STADIUM_CAPACITY)));
        break;
    case F_EXPANSION:
        wr32(g_mem, s + STADIUM_EXPANSION, (uint32_t)clamp64(v0 + d * (big ? 5000 : 500), 0, CAPACITY_MAX));
        break;
    case F_HEATING: wr8(g_mem, s + STADIUM_HEATING, !v0); break;
    case F_ROOF: wr8(g_mem, s + STADIUM_ROOF, !v0); break;
    case F_OWNS: wr8(g_mem, club + CLUB_OWNS, !v0); break;
    }
    int64_t v = get(club, field);
    if (v == v0) return;
    fprintf(stderr, "recomp: club editor: club %d %s %lld -> %lld\n", (int32_t)rd32(g_mem, club),
            field_names[field], (long long)v0, (long long)v);
    editor_mark_edited();
}

/* ---- The tab ---- */

#define TEXT_SIZE 0x200u

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, TEXT_SIZE);
    return scratch;
}

static int area_flags(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights, uint32_t flags)
{
    uint32_t w = 0;
    if (weights) {
        w = scratch + TEXT_SIZE;
        memcpy(g_mem + w, weights, (size_t)columns);
    }
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, (uint32_t)columns, w, 1, 0, flags, 0,
                      0xffffffffu};
    return (int16_t)call_this(AREA_CREATE, display(), 11, a);
}

static int area(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights)
{
    return area_flags(x1, y1, x2, y2, columns, weights, 1);
}

static void box_text(int x1, int y1, int x2, int y2, uint32_t font, uint32_t colour, const char *s)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 1, 0, 1, font, colour, text(s), 1};
    call_this(BOX_TEXT, SCREENS, 11, a);
}

static void plain(int a, int column, const char *s, uint32_t colour)
{
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 1, 2, colour, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

static void value_text(int a, int column, const char *s)
{
    uint32_t y = rd16(g_mem, TEXT_YELLOW);
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 0x221, y, 0xc, 2, y, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

static void button(int a, int column, const char *s, int field, int op, uint32_t green)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 0xc, 2, green, text(s), STEP_ID,
                      (uint32_t)(field * 4 + op)};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

/* A row: name, big down, down, value, up, big up. A switch row: name, a gap, value, Change. */
static const uint8_t weights_row[] = {28, 11, 6, 22, 6, 11};
static const uint8_t weights_switch[] = {28, 17, 22, 17};
#define LEFT_X1 110
#define LEFT_X2 440
#define RIGHT_X1 455
#define RIGHT_X2 785
#define ROW_H 25

static void row(int x1, int x2, int y, const char *label, const char *shown, int field, const char *big,
                const char *small, uint32_t green, uint32_t light)
{
    int a = area(x1, y, x2, y + ROW_H - 3, 6, weights_row);
    plain(a, 0, label, light);
    if (big) button(a, 1, big[0] ? big : "", field, 0, green);
    if (small) button(a, 2, "-", field, 1, green);
    value_text(a, 3, shown);
    if (small) button(a, 4, "+", field, 2, green);
    if (big) {
        char up[16];
        snprintf(up, sizeof up, "+%s", big + 1);
        button(a, 5, up, field, 3, green);
    }
}

static void switch_row(int x1, int x2, int y, const char *label, const char *shown, int field, uint32_t green,
                       uint32_t light)
{
    int a = area(x1, y, x2, y + ROW_H - 3, 4, weights_switch);
    plain(a, 0, label, light);
    value_text(a, 2, shown);
    button(a, 3, "Change", field, 2, green);
}

static void heading(int x1, int x2, int y, const char *s)
{
    box_text(x1, y, x2, y + 20, 3, rd16(g_mem, TEXT_YELLOW), s);
}

static void number(char *out, size_t room, int64_t v)
{
    if (v < 0) snprintf(out, room, "-");
    else if (v >= 1000) snprintf(out, room, "%lld,%03lld", (long long)(v / 1000), (long long)(v % 1000));
    else snprintf(out, room, "%lld", (long long)v);
}

static void draw(uint32_t club, int top, int bottom)
{
    if (!scratch) scratch = rt_alloc_zero(TEXT_SIZE + 16);
    uint32_t g[4] = {0, 0xc0, 0, 0};
    uint32_t green = call(COLOUR, 4, g) & 0xffff, light = rd16(g_mem, TEXT_LIGHT);
    char s[64];
    int y = top + 6;
    area_flags(LEFT_X1 - 6, top, LEFT_X2 + 6, top + 236, 1, NULL, 2);     /* dark panels, as the tab bar's (0x5ed9fe) */
    area_flags(RIGHT_X1 - 6, top, RIGHT_X2 + 4, top + 236, 1, NULL, 2);

    heading(LEFT_X1, LEFT_X2, y, "Money");
    int yl = y + 24;
    uint32_t f = editor_finance(club);
    if (f) editor_money_text(get(club, F_BALANCE), 0, s, sizeof s); else snprintf(s, sizeof s, "-");
    row(LEFT_X1, LEFT_X2, yl, "Balance", s, F_BALANCE, "-10m", "", green, light);
    yl += ROW_H;
    if (f) editor_money_text(get(club, F_FUNDS), 1, s, sizeof s); else snprintf(s, sizeof s, "-");
    row(LEFT_X1, LEFT_X2, yl, "Transfer funds", s, F_FUNDS, "-10m", "", green, light);
    yl += ROW_H + 8;
    heading(LEFT_X1, LEFT_X2, yl, "Club");
    yl += 24;
    number(s, sizeof s, get(club, F_REPUTATION));
    row(LEFT_X1, LEFT_X2, yl, "Reputation", s, F_REPUTATION, "-1k", "", green, light);
    yl += ROW_H;
    snprintf(s, sizeof s, "%d", (int)get(club, F_TRAINING));
    row(LEFT_X1, LEFT_X2, yl, "Training facilities", s, F_TRAINING, "-5", "", green, light);
    yl += ROW_H;
    int st = (int)get(club, F_STATUS);
    row(LEFT_X1, LEFT_X2, yl, "Status", status_names[st >= 1 && st <= 3 ? st : 0], F_STATUS, NULL, "", green,
        light);
    yl += ROW_H;
    number(s, sizeof s, get(club, F_CONFIDENCE));
    row(LEFT_X1, LEFT_X2, yl, "Board confidence", s, F_CONFIDENCE, "-1k", "", green, light);

    heading(RIGHT_X1, RIGHT_X2, y, "Stadium");
    int yr = y + 24;
    if (!stadium_of(club)) {
        box_text(RIGHT_X1, yr, RIGHT_X2, yr + 20, 2, light, "The club has no stadium.");
    } else {
        number(s, sizeof s, get(club, F_CAPACITY));
        row(RIGHT_X1, RIGHT_X2, yr, "Capacity", s, F_CAPACITY, "-5k", "", green, light);
        yr += ROW_H;
        number(s, sizeof s, get(club, F_SEATED));
        row(RIGHT_X1, RIGHT_X2, yr, "Seated", s, F_SEATED, "-5k", "", green, light);
        yr += ROW_H;
        number(s, sizeof s, get(club, F_EXPANSION));
        row(RIGHT_X1, RIGHT_X2, yr, "Expansion limit", s, F_EXPANSION, "-5k", "", green, light);
        yr += ROW_H;
        switch_row(RIGHT_X1, RIGHT_X2, yr, "Under-soil heating", get(club, F_HEATING) ? "Yes" : "No", F_HEATING,
                   green, light);
        yr += ROW_H;
        switch_row(RIGHT_X1, RIGHT_X2, yr, "Retractable roof", get(club, F_ROOF) ? "Yes" : "No", F_ROOF, green,
                   light);
        yr += ROW_H;
    }
    switch_row(RIGHT_X1, RIGHT_X2, yr, "Ground", get(club, F_OWNS) ? "Owned" : "Rented", F_OWNS, green, light);
    box_text(LEFT_X1, bottom - 26, RIGHT_X2, bottom - 6, 1, light,
             "Changes apply at once and mark the game as edited. Money steps are amounts as shown.");
}

static void log_values(uint32_t club)
{
    fprintf(stderr, "recomp: club editor opened: club %d", (int32_t)rd32(g_mem, club));
    for (int k = 0; k < FIELDS; k++) fprintf(stderr, " %s %lld", field_names[k], (long long)get(club, k));
    fprintf(stderr, "\n");
}

/* ---- The club screen ---- */

void o_005ed780(void);

/* The tab bar, cdecl (entries, count, selected, int *y_top, int *y_bottom, split). */
RT_WRAP(005ed780, tab_bar)
{
    if (hidden_tab_bar()) return;                   /* the Hidden tab of a player's profile (hidden.c) */
    on_tab = 0;
    if (rd32(g_mem, cpu.esp) != rt_site_club_tab_bar || rt_arg(1) != CLUB_ENTRIES) {
        o_005ed780();
        return;
    }
    uint32_t club = node_slot(0), selected = rt_arg(2);
    if (!entries) entries = rt_alloc_zero((CLUB_ENTRIES + 1) * ENTRY_SIZE);   /* on and off: the same heap blocks */
    if (!options_bool(OPT_CLUB_EDITOR) || !is_club(club)) {
        if (selected == EDIT_TAB) {                 /* the option went off on the tab: back to Finances & Info */
            set_slot(4, INFO_TAB);
            wr32(g_mem, cpu.esp + 12, INFO_TAB);
            o_005ed780();
            cpu.esi = INFO_TAB;
            return;
        }
        o_005ed780();
        return;
    }
    uint32_t from = rt_arg(0), tab = entries + INFO_TAB * ENTRY_SIZE;
    memcpy(g_mem + entries, g_mem + from, INFO_TAB * ENTRY_SIZE);
    memcpy(g_mem + tab, g_mem + from + (INFO_TAB - 1) * ENTRY_SIZE, ENTRY_SIZE);
    memcpy(g_mem + tab + ENTRY_SIZE, g_mem + from + INFO_TAB * ENTRY_SIZE,
           (CLUB_ENTRIES - INFO_TAB) * ENTRY_SIZE);
    wr16(g_mem, tab + 2, 0);                        /* the default colour */
    win_put_str(tab + ENTRY_TEXT, "Edit Club", ENTRY_ID - ENTRY_TEXT);
    wr32(g_mem, tab + ENTRY_ID, rd32(g_mem, from + ENTRY_ID));     /* the id of the Squad tab: always a tab */
    wr32(g_mem, tab + ENTRY_DATA, EDIT_TAB);
    wr32(g_mem, cpu.esp + 4, entries);
    wr32(g_mem, cpu.esp + 8, CLUB_ENTRIES + 1);
    on_tab = selected == EDIT_TAB;
    o_005ed780();
    if (on_tab && !rt_unwinding) {
        cpu.esi = INFO_TAB;                         /* the build's switch runs case 5, 0x460590 */
        log_values(club);
    }
}

void o_00460590(void);

/* draw_club_information_screen, cdecl (top, bottom): the Edit Club tab in its place. */
RT_WRAP(00460590, club_information)
{
    if (!on_tab) {
        o_00460590();
        return;
    }
    on_tab = 0;
    draw(node_slot(0), (int)rt_arg(0), (int)rt_arg(1));
    rt_ret_cdecl(0);
}

void o_004671f0(void);

/* The input of the club screen (assistant.c's wrap of 0x4671f0 asks first). The input switches on slot 4 for the
 * widgets of each tab (0x467cbd, cases 1..5, any other value an error box at 0x4688c8), on every pass, a click or
 * not. On the Edit Club tab: its own buttons step a value; a click on a tab goes to the original, which moves to
 * that tab (0x467701..0x4677ac) before the switch, and a click on Edit Club itself does nothing; everything else
 * runs with slot 4 at Finances & Info, whose widgets the tab does not make, and slot 4 goes back to Edit Club
 * unless the input moved it. */
int club_editor_input(int16_t w)
{
    if (node_slot(4) != EDIT_TAB) return 0;
    if (w >= 0) {
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at), d = rd32(g_mem, at + 0x40);
        if (id == STEP_ID) {
            uint32_t club = node_slot(0);
            if (d / 4 < FIELDS && is_club(club)) step(club, (int)(d / 4), (int)(d % 4));
            rt_ret_cdecl(REBUILD);
            return 1;
        }
        if (id == TAB_ID) {
            if (d == EDIT_TAB) rt_ret_cdecl(0);
            else o_004671f0();
            return 1;
        }
    }
    set_slot(4, INFO_TAB);
    o_004671f0();
    if (!rt_unwinding && node_slot(4) == INFO_TAB) set_slot(4, EDIT_TAB);
    return 1;
}
