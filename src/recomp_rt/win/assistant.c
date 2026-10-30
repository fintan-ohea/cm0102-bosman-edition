/* The Assistant drop-down of the squad screen (#12). Research:
 * docs/research/2026-10-15-squad-dropdown-inventory.md (the button and the drop-down) and
 * docs/research/2026-10-15-assistant-pick-inventory.md (the picker).
 *
 * On the squad tab of the player's own club (0x454900, node slot 4 = 1), the Clear Squad button
 * becomes an Assistant button with a drop-down in the style of Filter: Pick best team, Pick most
 * match-fit team, Pick under-21 team and Clear squad. The last item is the original button: same id
 * 0x25 and data, so the original handler clears the team and keeps the undo copy, and the item reads
 * "Undo clear" when the original button read "Undo Clear". The Penalty Takers view keeps its button.
 *
 * The three pick items call picker_pick (below) and then return -0xb, as the original handlers do, so
 * the screen loop builds the screen again with the new selection.
 *
 * With squad_views = on, the drop-down ends with Team views, which opens the Team views screen
 * (views.c). Another club's squad screen has no Clear Squad button (0x600c90 at 0x458a37), so it gets
 * an Assistant button with Team views only, in the same cell.
 *
 * With training_assistant = on (fork), the drop-down of the player's own squad ends with Training setup, which
 * opens the training assistant (training.c), and with auto_scouting = on by Scout reports (scouting.c). */
#include "win.h"
#include "options.h"

#include <stdio.h>

extern const uint32_t rt_site_squad_clear, rt_site_squad_filter;

#define SCREENS 0xb74450u        /* the screen stack: this of 0x8040a0 and 0x7ff010 */
#define NODE_SLOT 0x7ff010u      /* thiscall (slot): the value of a slot of the current screen node */
#define AREA_CREATE 0x55fba0u    /* thiscall on a Display, 11 args: the popup area, as for Filter */
#define WIDGET_CREATE 0x55f990u  /* thiscall on a Display, 18 args: one item of a popup */
#define DISPLAYS 0xb74c4cu       /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au    /* s16: the player slot that has the turn */
#define WIDGET_ID 0xba966u       /* Display + this + w * 0x18c: the id of widget w */
#define WIDGET_SIZE 0x18cu
#define POPUP_COLOUR 0xae31aau   /* u16 colour globals that the Filter items use (0x458b45, 0x458bb6) */
#define ITEM_HIGHLIGHT 0xaebdecu
#define ITEM_TEXT 0xaebdeau
#define CLEAR_ID 0x25u           /* the Clear Squad button and its handler (0x4680bd) */
#define PICK_ID 0x60u            /* the pick items; the squad tab does not use this id */
#define PENALTY_VIEW 0x800u      /* node slot 7: the Penalty Takers view */
#define REBUILD 0xfffffff5u      /* -0xb: the screen loop builds the screen again (0x7fd3a9) */
#define NEW_SCREEN 0xfffffffcu   /* -4: build the screen on top, after a push (as the start screen menu) */
#define VIEWS_KIND 4u            /* the data of the Team views item (views.c) */
#define TRAINING_KIND 5u         /* the data of the Training setup item (training.c, fork) */
#define SCOUTING_KIND 6u         /* the data of the Scout reports item (scouting.c, fork) */
#define CELL_BUTTON 0x8040a0u    /* the cell button create, for the Assistant button of another club */
#define BUTTON_COLOUR 0xae317eu  /* u16 colours of the Clear Squad button (0x458ad0) */
#define BUTTON_TEXT 0xae3184u


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

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

/* The texts live on the game's heap: the widget create copies them (0x458a96, 0x458ad5). All of them
 * are made at the first use, whatever the options: the game's results depend on the layout of its heap
 * (a save differs after one more block of 11 bytes), so squad_views on and off must make the same blocks. */
static uint32_t text(int i)
{
    static const char *const texts[] = {"Assistant", "Pick best team", "Pick most match-fit team",
                                        "Pick under-21 team", "Clear squad", "Undo clear", "Team views",
                                        "Training setup", "Scout reports"};
    enum { TEXTS = sizeof texts / sizeof texts[0] };
    static uint32_t at[TEXTS];
    if (!at[0])
        for (int k = 0; k < TEXTS; k++) at[k] = rt_alloc_str(texts[k]);
    return at[i];
}

void o_008040a0(void);
void o_004671f0(void);
void foreign_cell_button(void);

/* One item of a popup: an id and its data, as the Filter items (0x458b45, 0x458bb6). */
static void popup_item(uint32_t popup, uint32_t t, uint32_t id, uint32_t data)
{
    uint32_t w[18] = {2, 0, 0, 0, 0, 0, 0, 0x10, rd16(g_mem, POPUP_COLOUR), rd16(g_mem, ITEM_HIGHLIGHT), 1, 1,
                      rd16(g_mem, ITEM_TEXT), t, 0, id, data, popup};
    call_this(WIDGET_CREATE, display(), 18, w);
}

/* The popup of the button: an area as for Filter. */
static uint32_t popup_of(uint32_t button)
{
    uint32_t a[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0x130, rd16(g_mem, POPUP_COLOUR), button};
    return (uint32_t)(int32_t)(int16_t)call_this(AREA_CREATE, display(), 11, a);
}

static int clear_made;  /* the squad list made its Clear Squad button in this build */

/* The cell button create, thiscall on 0xb74450, 11 args: (area, column, row, style, colour, text_flags,
 * font, text_colour, text, id, data); ret 0x2c, the widget index in ax. Two calls change: Clear Squad
 * (0x458ad0) becomes the Assistant button, and before Filter (0x458b10) a squad screen without Clear
 * Squad (another club's) gets an Assistant button with Team views only. */
RT_WRAP(008040a0, cell_button)
{
    foreign_cell_button();      /* the squad list's names (foreign.c) */
    if (shortlist_all_cell_button()) return;   /* Shortlist All after Filters of Player & Staff Search */
    value_range_cell_button();      /* the Value row of the search Filters (value_range.c) */
    uint32_t site = rd32(g_mem, cpu.esp);
    if (site == rt_site_squad_filter) {
        int other = !clear_made;
        clear_made = 0;
        text(0);    /* the same heap blocks with squad_views on and off (see text) */
        if (other && options_bool(OPT_SQUAD_VIEWS) && !(node_slot(7) & PENALTY_VIEW)) {
            uint32_t b[11] = {rt_arg(0), 0, 0, 0x30, rd16(g_mem, BUTTON_COLOUR), 0xc, 1, rd16(g_mem, BUTTON_TEXT),
                              text(0), 0, 0};
            uint32_t button = (uint32_t)(int32_t)(int16_t)call_this(CELL_BUTTON, SCREENS, 11, b);
            popup_item(popup_of(button), text(6), PICK_ID, VIEWS_KIND);
        }
        o_008040a0();
        return;
    }
    if (site != rt_site_squad_clear || rt_arg(9) != CLEAR_ID || (node_slot(7) & PENALTY_VIEW)) {
        if (site == rt_site_squad_clear) clear_made = 1;
        o_008040a0();
        return;
    }
    clear_made = 1;
    uint32_t undo = rt_arg(10);
    wr32(g_mem, cpu.esp + 4 + 4 * 8, text(0));
    wr32(g_mem, cpu.esp + 4 + 4 * 9, 0);         /* id 0: the button only opens its popup (0x560490) */
    o_008040a0();
    uint32_t button = (uint32_t)(int32_t)(int16_t)cpu.eax;
    uint32_t popup = popup_of(button);
    for (int i = 1; i <= 3; i++) popup_item(popup, text(i), PICK_ID, (uint32_t)i);
    popup_item(popup, text(undo ? 5 : 4), CLEAR_ID, undo);
    if (options_bool(OPT_SQUAD_VIEWS)) popup_item(popup, text(6), PICK_ID, VIEWS_KIND);
    if (options_bool(OPT_TRAINING_ASSISTANT)) popup_item(popup, text(7), PICK_ID, TRAINING_KIND);
    if (options_bool(OPT_AUTO_SCOUTING)) popup_item(popup, text(8), PICK_ID, SCOUTING_KIND);
    cpu.eax = button;
}

/* The input of the club screens, cdecl (s16 widget). A pick item runs the picker. */
RT_WRAP(004671f0, squad_input)
{
    int16_t w = (int16_t)rt_arg(0);
    if (club_editor_input(w)) return;              /* the Edit Club tab (club_editor.c) */
    if (w < 0 || node_slot(4) != 1) {
        o_004671f0();
        return;
    }
    uint32_t at = display() + WIDGET_ID + (uint32_t)w * WIDGET_SIZE;
    if ((int16_t)rd32(g_mem, at) != (int16_t)PICK_ID) {
        o_004671f0();
        return;
    }
    int kind = (int)rd32(g_mem, at + 0x40);
    uint32_t club = node_slot(0), team = node_slot(1);
    if (kind == VIEWS_KIND) {
        views_open(club, team);
        rt_ret_cdecl(NEW_SCREEN);
        return;
    }
    if (kind == SCOUTING_KIND) {
        if (scouting_open()) rt_ret_cdecl(NEW_SCREEN);
        else rt_ret_cdecl(REBUILD);
        return;
    }
    if (kind == TRAINING_KIND) {
        if (training_open(club)) rt_ret_cdecl(NEW_SCREEN);
        else rt_ret_cdecl(REBUILD);
        return;
    }
    int n = picker_pick(club, team, kind);
    fprintf(stderr, "recomp: assistant picked %d players (kind %d) for club %#x team %u\n", n, kind, club, team);
    rt_ret_cdecl(REBUILD);
}
