/* The Hidden tab (fork of fintan-ohea; docs/specs/2026-10-30-hidden-tab-spec.md).
 *
 * With hidden_tab = on, the profile of a player (build 0x870700) has a sixth tab, Hidden, after History. It shows
 * the hidden values (glossary): current and potential ability and the attributes that no screen of the original
 * shows (docs/research/2026-10-30-hidden-attributes.md).
 *
 * The tab: the build hands the tab bar 0x5ed780 five entries of 0xd4 bytes at 0x874cc1 (call site
 * profile_tab_bar), the tab number 1..5 at entry +0xd0, and the tab of node slot 1 as the selected one. A click on
 * a tab is widget id 0x11 with the entry's data, which the input puts into slot 1 (0x88954e..0x8895d0). After the
 * bar the build draws the tab's page by its copy of the tab (0x875a29..0x875a46, cases 1..5; any other value an
 * error box at 0x876363), and makes the tab's own widgets by the byte copy of slot 1 (0x875132.., 0x875ebd..,
 * tests for 1, 2 and 5). So the wrap passes six entries, Hidden with data 6 last, and when slot 1 is 6 it sets both
 * copies to 3 after the bar: the build draws the Contract tab, which has no widgets of its own, and the wrap of the
 * Contract tab's draw 0x87cd90 (cdecl (top, bottom)) draws the Hidden tab in its place.
 *
 * Same game (Q152): with the option off the bar gets the original's entries and the tab is never made. The entry
 * block is taken at the first profile build with the option on or off. */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <string.h>

extern const uint32_t rt_site_profile_tab_bar;

#define SCREENS 0xb74450u           /* the screen stack S */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define DISPLAYS 0xb74c4cu
#define PLAYER_SLOT 0xb77c7au
#define TEXT_YELLOW 0xaebdecu
#define TEXT_LIGHT 0xae3154u

#define ENTRY_SIZE 0xd4u            /* a tab bar entry: s16 font, u16 colour, text, +0xd0 data (club_editor.c) */
#define ENTRY_TEXT 0x04u
#define ENTRY_DATA 0xd0u
#define PROFILE_TABS 5
#define HIDDEN_TAB 6
#define STAND_IN_TAB 3              /* Contract: the build makes no widgets of its own for it */
#define FRAME_TAB 0x6cu             /* the build's frame (esp before the bar's six arguments): its tab (0x874cba) */
#define FRAME_TAB_BYTE 0x1fu        /* and the byte copy of slot 1 (0x8707d6) */

#define RATING 0x544380u           /* player_attribute_rating, cdecl (player data, s8 byte): 1 to 20 */
#define TAB_ID 0x11u                /* the widget id of a tab (0x88954e) */
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu
#define PLAYER_DATA 0x61u           /* staff: the player data, 0 for no player */
#define ABILITY 0x05u               /* player data: s16 current ability */
#define POTENTIAL 0x07u             /* player data: s16 potential ability (0x8e8345) */

/* A hidden value: in the staff (s), in the player data (p), or a byte of the player data that the game rates with
 * the current ability (r, player_attribute_rating 0x544380, as the shown technical attributes). */
typedef struct { const char *name; char where; uint8_t at; } Hidden;

/* The research note gives the code for each. */
static const Hidden mental[] = {
    {"Adaptability", 's', 0x56}, {"Ambition", 's', 0x57}, {"Loyalty", 's', 0x59}, {"Pressure", 's', 0x5a},
    {"Professionalism", 's', 0x5b}, {"Sportsmanship", 's', 0x5c}, {"Temperament", 's', 0x5d},
};
static const Hidden playing[] = {
    {"Consistency", 'p', 0x21}, {"Dirtiness", 'p', 0x25}, {"Important Matches", 'p', 0x2c},
    {"Injury Proneness", 'p', 0x2d}, {"Natural Fitness", 'p', 0x34}, {"Versatility", 'p', 0x42},
};
static const Hidden skills[] = {
    {"Corners", 'r', 0x22}, {"Penalties", 'r', 0x38}, {"Throw Ins", 'r', 0x41}, {"One On Ones", 'r', 0x35},
    {"Left Foot", 'p', 0x30}, {"Right Foot", 'p', 0x3b},
};
#define COUNT(a) ((int)(sizeof a / sizeof a[0]))

static uint32_t entries, scratch;
static int on_tab;                  /* the build that runs shows the Hidden tab */
#define TEXT_SIZE 0x100u

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
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

static int is_player(uint32_t staff) { return staff && rd32(g_mem, staff + PLAYER_DATA); }

/* The bar's entries are the five tabs, 1..5 in order. */
static int tabs_as_expected(uint32_t from)
{
    for (uint32_t k = 0; k < PROFILE_TABS; k++)
        if (rd32(g_mem, from + k * ENTRY_SIZE + ENTRY_DATA) != k + 1) return 0;
    return 1;
}

/* The build's two copies of its tab, after the bar returns: frame = esp before the six arguments. */
static void set_frame_tab(uint32_t frame, uint32_t tab)
{
    wr32(g_mem, frame + FRAME_TAB, tab);
    wr8(g_mem, frame + FRAME_TAB_BYTE, tab);
}

void o_005ed780(void);

/* The tab bar (club_editor.c's wrap asks first), cdecl (entries, count, selected, int *y_top, int *y_bottom,
 * split). Returns 1 when it handled the call. */
int hidden_tab_bar(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_profile_tab_bar) return 0;
    on_tab = 0;
    if (!entries) entries = rt_alloc_zero((PROFILE_TABS + 1) * ENTRY_SIZE);    /* on and off: the same blocks */
    uint32_t frame = cpu.esp + 4 + 6 * 4, from = rt_arg(0), selected = rt_arg(2);
    if (!options_bool(OPT_HIDDEN_TAB) || !is_player(node_slot(0)) || rt_arg(1) != PROFILE_TABS ||
        !tabs_as_expected(from)) {
        if (selected == HIDDEN_TAB) {               /* the option went off on the tab: back to Profile */
            set_slot(1, 1);
            wr32(g_mem, cpu.esp + 12, 1);
            o_005ed780();
            if (!rt_unwinding) set_frame_tab(frame, 1);
            return 1;
        }
        o_005ed780();
        return 1;
    }
    uint32_t tab = entries + PROFILE_TABS * ENTRY_SIZE;
    memcpy(g_mem + entries, g_mem + from, PROFILE_TABS * ENTRY_SIZE);
    memcpy(g_mem + tab, g_mem + from + (PROFILE_TABS - 1) * ENTRY_SIZE, ENTRY_SIZE);
    win_put_str(tab + ENTRY_TEXT, "Hidden", 0x40);
    wr32(g_mem, tab + ENTRY_DATA, HIDDEN_TAB);
    wr32(g_mem, cpu.esp + 4, entries);
    wr32(g_mem, cpu.esp + 8, PROFILE_TABS + 1);
    o_005ed780();
    if (selected == HIDDEN_TAB && !rt_unwinding) {
        on_tab = 1;
        set_frame_tab(frame, STAND_IN_TAB);         /* the build's switch runs case 3, 0x87cd90 */
    }
    return 1;
}

/* ---- The page ---- */

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, TEXT_SIZE);
    return scratch;
}

static int area(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights, uint32_t flags)
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

static void label(int a, int column, const char *s, uint32_t colour)
{
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 1, 2, colour, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

/* Rows as the Profile tab's attributes: three columns from x 115, 340 and 565, a row every 13.5 pixels. */
static const uint8_t weights_row[] = {78, 22};
#define ROW_H 13.5
#define COLUMN_W 224
#define FIRST_X 115

static void row(int x, double y, const char *name, int v, uint32_t name_colour)
{
    char s[16];
    snprintf(s, sizeof s, "%d", v);
    int a = area(x, (int)y, x + COLUMN_W - 24, (int)(y + ROW_H), 2, weights_row, 1);
    label(a, 0, name, name_colour);
    label(a, 1, s, rd16(g_mem, TEXT_YELLOW));
}

static void heading(int x, double y, const char *s)
{
    int a = area(x, (int)y, x + COLUMN_W - 24, (int)(y + ROW_H), 1, NULL, 1);
    label(a, 0, s, rd16(g_mem, TEXT_YELLOW));
}

static int value_of(uint32_t staff, const Hidden *h)
{
    uint32_t data = rd32(g_mem, staff + PLAYER_DATA);
    if (h->where == 'r') {
        uint32_t a[2] = {data, (uint32_t)(int32_t)(int8_t)rd8(g_mem, data + h->at)};
        Cpu saved = cpu;
        int v = (int8_t)rt_callback(RATING, 2, a);
        cpu = saved;
        return v;
    }
    return (int8_t)rd8(g_mem, (h->where == 's' ? staff : data) + h->at);
}

static void group(int x, double y, const char *title, uint32_t staff, const Hidden *list, int n)
{
    heading(x, y, title);
    for (int i = 0; i < n; i++) row(x, y + (i + 1) * ROW_H, list[i].name, value_of(staff, &list[i]),
                                    rd16(g_mem, TEXT_LIGHT));
}

static void draw(uint32_t staff, int top)
{
    if (!scratch) scratch = rt_alloc_zero(TEXT_SIZE + 16);
    uint32_t data = rd32(g_mem, staff + PLAYER_DATA);
    double y = top + 6;                             /* as the Profile tab's headings (Technical at y 167) */
    int x = FIRST_X;
    group(x, y, "Mental", staff, mental, COUNT(mental));
    group(x + COLUMN_W, y, "Playing", staff, playing, COUNT(playing));
    group(x + 2 * COLUMN_W, y, "Set pieces and feet", staff, skills, COUNT(skills));
    double a = y + (COUNT(playing) + 1) * ROW_H + 6;
    heading(x + COLUMN_W, a, "Ability");
    row(x + COLUMN_W, a + ROW_H, "Current Ability", (int16_t)rd16(g_mem, data + ABILITY), rd16(g_mem, TEXT_LIGHT));
    row(x + COLUMN_W, a + 2 * ROW_H, "Potential Ability", (int16_t)rd16(g_mem, data + POTENTIAL),
        rd16(g_mem, TEXT_LIGHT));
    fprintf(stderr, "recomp: hidden tab: staff %d ca %d pa %d", (int32_t)rd32(g_mem, staff),
            (int16_t)rd16(g_mem, data + ABILITY), (int16_t)rd16(g_mem, data + POTENTIAL));
    const Hidden *lists[3] = {mental, playing, skills};
    int counts[3] = {COUNT(mental), COUNT(playing), COUNT(skills)};
    for (int g = 0; g < 3; g++)
        for (int i = 0; i < counts[g]; i++) fprintf(stderr, " %d", value_of(staff, &lists[g][i]));
    fprintf(stderr, "\n");
}

void o_00888260(void);

/* The profile's input (editor.c's wrap asks first). It switches on slot 1 for the widgets of each tab
 * (0x889a19..0x889a28, cases 1..5, any other value an error box at 0x889aa8). On the Hidden tab a click on a tab
 * goes to the original, which moves to that tab (0x88954e..0x8895d0); everything else runs with slot 1 at Contract,
 * whose widgets the Hidden tab does not make, and slot 1 goes back to Hidden unless the input moved it. Returns 1
 * when it handled the call. */
int hidden_profile_input(void)
{
    if (node_slot(1) != HIDDEN_TAB) return 0;
    int16_t w = (int16_t)rt_arg(0);
    if (w >= 0 && (uint16_t)rd32(g_mem, display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE) == TAB_ID) {
        o_00888260();
        return 1;
    }
    set_slot(1, STAND_IN_TAB);
    o_00888260();
    if (!rt_unwinding && node_slot(1) == STAND_IN_TAB) set_slot(1, HIDDEN_TAB);
    return 1;
}

void o_0087cd90(void);

/* staff_screens_draw_contract_tab, cdecl (top, bottom): the Hidden tab in its place. */
RT_WRAP(0087cd90, contract_tab)
{
    if (!on_tab) {
        o_0087cd90();
        return;
    }
    on_tab = 0;
    uint32_t staff = node_slot(0);
    if (is_player(staff)) draw(staff, (int)rt_arg(0));
    rt_ret_cdecl(0);
}
