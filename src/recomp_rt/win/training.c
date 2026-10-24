/* The training assistant (fork of fintan-ohea; docs/specs/2026-10-24-training-assistant-spec.md, slice 1).
 *
 * With training_assistant = on, the Assistant drop-down of the player's own squad screen ends with "Training
 * setup" (assistant.c), which opens this box for the club. The box is a screen of the runtime on the game's
 * screen stack, built as the Options screen (options_screen.c): the frame of Game Settings (0x823950), pushed
 * with flag 1.
 *
 * It shows two schedules, Outfield and Keepers, with their levels and their workload by the game's own formula
 * (TRAINING_SCHEDULE::get_training_load 0x8b4f60), and the first team players who follow each. Apply writes them
 * into the manager's two "Unused" schedule slots (8 and 9, type 7) and puts each first team player on one of
 * them by position. A slot that holds a schedule of the player's own is left alone, and the box says so. The
 * box works on the training manager directly and not from the Training screen, because that screen edits a copy
 * that its Ok writes back (TRAINING_EDIT_SESSION::apply_changes 0x8b0b40).
 *
 * The data: docs/research/2026-10-24-training.md.
 *
 * Same game (Q152): the box makes its scratch block when it opens, after a click of the player. The item's text
 * is made with the other Assistant texts whatever the option (assistant.c). */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <string.h>

#define SCREENS 0xb74450u           /* the screen stack S */
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define BACKGROUND_OFF 0x41c0d0u    /* cdecl (0): as the Game Settings build (0x823a57) */
#define COLOUR 0x5e4800u            /* cdecl (0, green, 0, 0): an RGB565 colour (0x823ac0) */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */

#define DISPLAYS 0xb74c4cu          /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu

#define TEXT_YELLOW 0xaebdecu       /* u16 colours of the Game Settings box */
#define TEXT_TITLE 0xaebdeau
#define TEXT_LIGHT 0xae3154u
#define BUTTON_COLOUR 0xae317eu
#define BUTTON_TEXT 0xae3184u

#define STAFF_COUNT 0xae2364u       /* the 16 human managers are the last staff ids */
#define SQUAD 0xd7u                 /* club: 50 staff pointers, the first team */
#define MANAGER 0xcfu               /* club: the manager's staff record */
#define PLAYER_DATA 0x61u           /* staff: the player data, 0 for no player */
#define GOALKEEPER 0x0fu            /* player data: the goalkeeper position rating, 1 to 20 */
#define NATURAL 15                  /* the game's "natural" rating (0x5b7d20, picker.c) */

#define TRAINING 0xde1f44u          /* the training manager */
#define PLAYER_ENTRIES 0x651du      /* manager + this: 6 bytes per player id, byte 0 the schedule slot */
#define SET_PLAYER_SCHEDULE 0x8b2250u  /* thiscall on the manager (staff, slot), ret 8 */
#define SET_LEVEL 0x8b4f30u         /* TRAINING_SCHEDULE::set_category_level, thiscall (category, level), ret 8 */
#define GET_LOAD 0x8b4f60u          /* TRAINING_SCHEDULE::get_training_load, thiscall (): the workload */
#define SCHEDULE_SIZE 0x9bu
#define SCHEDULE_NAME 0x09u         /* 11 characters and the end (0x8b51f4) */
#define SCHEDULE_TYPE 0x15u         /* 7 an unused slot, 8 a schedule of the manager's own */
#define TYPE_UNUSED 7
#define TYPE_OWN 8
#define CATEGORIES 5                /* Fitness, Tactics, Shooting, Skills, Goalkeeping: +0x04 to +0x08 */

#define APPLY_ID 0x7d44u            /* ids that the original does not use */
#define CLOSE_ID 0x7d45u
#define REBUILD 0xfffffff5u         /* -0xb: build the box again */
#define CLOSE 0xfffffff7u           /* -9: close the box, as Ok and Cancel of Game Settings (0x8259a0) */
#define TEXT_SIZE 0x200u

/* The two schedules of slice 1, from the forum topic (spec): the levels of the five categories. */
typedef struct {
    const char *name;
    int slot;
    uint8_t level[CATEGORIES];
} Plan;
static const Plan plans[2] = {
    {"Outfield", 8, {25, 50, 25, 50, 0}},
    {"Keepers", 9, {25, 25, 0, 25, 50}},
};
static const char *const category_names[CATEGORIES] = {"Fitness", "Tactics", "Shooting", "Skills", "Keeping"};

static uint32_t scratch, club, manager;
static int human;                   /* the manager's place among the 16 human staff */
static char result[160];            /* what the last Apply did */
static uint32_t handled_press = 0xffffffffu;

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

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static uint32_t colour16(uint32_t at) { return rd16(g_mem, at); }

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, TEXT_SIZE);
    return scratch;
}

/* ---- The training data ---- */

/* The manager's schedule slot (TRAINING_MANAGER::get_manager_schedule 0x8b3930). */
static uint32_t schedule(int slot)
{
    return rd32(g_mem, TRAINING) + (uint32_t)(human * 10 + slot + 7) * SCHEDULE_SIZE;
}

/* The workload of the levels, as get_training_load 0x8b4f60: 1.3 x Fitness + 0.5 x each other, at most 100. The
 * game cuts each term to a whole number (0x945640 sets the FPU to chop, 0x94564f), so Medium Fitness gives 32
 * and a Medium category 12. */
static int workload(const uint8_t *level)
{
    int w = (int)(level[0] * 1.3);
    for (int c = 1; c < CATEGORIES; c++) w += (int)(level[c] * 0.5);
    return w > 100 ? 100 : w;
}

static const char *level_name(int v)
{
    return v >= 50 ? "Intensive" : v >= 25 ? "Medium" : v >= 10 ? "Light" : "None";
}

/* A slot the assistant may write: unused, or already one of its own schedules. */
static int slot_free(const Plan *p)
{
    uint32_t s = schedule(p->slot);
    int type = (int8_t)rd8(g_mem, s + SCHEDULE_TYPE);
    return type == TYPE_UNUSED || (type == TYPE_OWN && !strncmp(win_str(s + SCHEDULE_NAME), p->name, 11));
}

static int is_keeper(uint32_t staff)
{
    return (int)rd8(g_mem, rd32(g_mem, staff + PLAYER_DATA) + GOALKEEPER) >= NATURAL;
}

/* The first team players on slot. */
static int players_on(int slot)
{
    int n = 0;
    uint32_t entries = rd32(g_mem, rd32(g_mem, TRAINING) + PLAYER_ENTRIES);
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i), data = staff ? rd32(g_mem, staff + PLAYER_DATA) : 0;
        if (data && (int)rd8(g_mem, entries + rd32(g_mem, data) * 6) == slot) n++;
    }
    return n;
}

static void write_schedule(const Plan *p)
{
    uint32_t s = schedule(p->slot);
    wr8(g_mem, s + SCHEDULE_TYPE, TYPE_OWN);
    win_put_str(s + SCHEDULE_NAME, p->name, 12);
    for (int c = 0; c < CATEGORIES; c++) {
        uint32_t a[2] = {(uint32_t)c, p->level[c]};
        call_this(SET_LEVEL, s, 2, a);
    }
}

static void apply(void)
{
    int written[2], players[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        written[k] = slot_free(&plans[k]);
        if (written[k]) write_schedule(&plans[k]);
    }
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i);
        if (!staff || !rd32(g_mem, staff + PLAYER_DATA)) continue;
        int k = is_keeper(staff);
        if (!written[k]) continue;
        uint32_t a[2] = {staff, (uint32_t)plans[k].slot};
        call_this(SET_PLAYER_SCHEDULE, rd32(g_mem, TRAINING), 2, a);
        players[k]++;
    }
    snprintf(result, sizeof result, "Applied: %d players on Outfield, %d on Keepers.", players[0], players[1]);
    if (!written[0] || !written[1]) snprintf(result, sizeof result, "Applied where the slot was free: %d players on "
                                             "Outfield, %d on Keepers.", players[0], players[1]);
    fprintf(stderr, "recomp: training assistant applied club %#x: outfield slot %d %s %d players, keepers slot %d "
                    "%s %d players\n", club, plans[0].slot, written[0] ? "written" : "kept", players[0],
            plans[1].slot, written[1] ? "written" : "kept", players[1]);
    for (int k = 0; k < 2; k++)
        fprintf(stderr, "recomp: training assistant %s workload box %d game %d\n", plans[k].name,
                workload(plans[k].level), (int16_t)call_this(GET_LOAD, schedule(plans[k].slot), 0, NULL));
}

/* ---- The box ---- */

static int area(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights, uint32_t flags,
                uint32_t colour)
{
    uint32_t w = 0;
    if (weights) {
        w = scratch + TEXT_SIZE;
        memcpy(g_mem + w, weights, (size_t)columns);
    }
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, (uint32_t)columns, w, 1, 0, flags,
                      colour, 0xffffffffu};
    return (int16_t)call_this(AREA_CREATE, display(), 11, a);
}

static void box_text(int x1, int y1, int x2, int y2, uint32_t style, uint32_t colour, uint32_t flags, uint32_t font,
                     uint32_t text_colour, const char *s)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, style, colour, flags, font,
                      text_colour, text(s), 1};
    call_this(BOX_TEXT, SCREENS, 11, a);
}

static void plain(int a, int column, const char *s, uint32_t font, uint32_t colour)
{
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 1, font, colour, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

static void value_text(int a, int column, const char *s)
{
    uint32_t y = colour16(TEXT_YELLOW);
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 0x221, y, 0xc, 2, y, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

static void wide_button(int a, int column, const char *s, uint32_t id)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                      text(s), id, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

/* A schedule row: name, the five levels, workload, players. */
static const uint8_t weights_row[] = {22, 13, 13, 13, 13, 13, 12, 11};
#define ROW_X1 160
#define ROW_X2 729

static void build(void)
{
    area(0, 0, 0x31f, 0x257, 1, NULL, 2, 0);                    /* the dark cover, 0x823a92 */
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t light = colour16(TEXT_LIGHT), yellow = colour16(TEXT_YELLOW);
    area(145, 50, 744, 549, 1, NULL, 0x30, box_colour);
    box_text(155, 58, 734, 92, 0x30, colour16(TEXT_TITLE), 0xc, 4, colour16(BUTTON_TEXT), "Training assistant");
    box_text(ROW_X1, 98, ROW_X2, 116, 1, 0, 1, 1, light,
             "Two schedules for your first team, with the workload by the game's own formula.");

    int h = area(ROW_X1, 124, ROW_X2, 146, 8, weights_row, 1, 0);
    plain(h, 0, "Schedule", 2, light);
    for (int k = 0; k < CATEGORIES; k++) plain(h, k + 1, category_names[k], 2, light);
    plain(h, 6, "Workload", 2, light);
    plain(h, 7, "Players", 2, light);
    for (int p = 0; p < 2; p++) {
        int r = area(ROW_X1, 152 + p * 30, ROW_X2, 178 + p * 30, 8, weights_row, 1, 0);
        plain(r, 0, plans[p].name, 3, light);
        for (int k = 0; k < CATEGORIES; k++) value_text(r, k + 1, level_name(plans[p].level[k]));
        char s[16];
        snprintf(s, sizeof s, "%d", workload(plans[p].level));
        value_text(r, 6, s);
        snprintf(s, sizeof s, "%d", players_on(plans[p].slot));
        value_text(r, 7, s);
    }

    int y = 226;
    for (int p = 0; p < 2; p++, y += 22) {
        char s[120];
        uint32_t sc = schedule(plans[p].slot);
        int type = (int8_t)rd8(g_mem, sc + SCHEDULE_TYPE);
        if (type == TYPE_UNUSED) snprintf(s, sizeof s, "Slot %d is unused: Apply sets up %s there.", plans[p].slot,
                                          plans[p].name);
        else if (slot_free(&plans[p])) snprintf(s, sizeof s, "Slot %d holds %s: Apply sets it again.", plans[p].slot,
                                                plans[p].name);
        else snprintf(s, sizeof s, "Slot %d holds your schedule %.11s: Apply leaves it and its players.",
                      plans[p].slot, win_str(sc + SCHEDULE_NAME));
        box_text(ROW_X1, y, ROW_X2, y + 20, 1, 0, 1, 2, light, s);
    }
    box_text(ROW_X1, 280, ROW_X2, 298, 1, 0, 1, 1, light,
             "Goalkeepers (position rating 15 or more) go on Keepers, every other player on Outfield.");
    box_text(ROW_X1, 300, ROW_X2, 318, 1, 0, 1, 1, light,
             "Workload = 1.3 x Fitness + 0.5 x each other category, at most 100.");
    if (result[0]) box_text(ROW_X1, 330, ROW_X2, 352, 1, 0, 1, 3, yellow, result);

    int b = area(155, 509, 734, 539, 2, NULL, 1, 0);
    wide_button(b, 0, "Apply", APPLY_ID);
    wide_button(b, 1, "Close", CLOSE_ID);
    rt_ret_cdecl(0);
}

/* The input of the box: cdecl (s16 widget), one click for each press of the mouse button (options_screen.c). */
static void input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    uint32_t r = 0;
    if (w >= 0 && win_press_count() == handled_press) w = -1;
    if (w >= 0) {
        handled_press = win_press_count();
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at);
        if (id == CLOSE_ID) {
            r = CLOSE;
        } else if (id == APPLY_ID) {
            apply();
            r = REBUILD;
        }
    }
    rt_ret_cdecl(r);
}

int training_open(uint32_t the_club)
{
    uint32_t m = the_club ? rd32(g_mem, the_club + MANAGER) : 0;
    int h = m ? (int)rd32(g_mem, STAFF_COUNT) - (int)rd32(g_mem, m) - 1 : -1;
    if (h < 0 || h >= 16) {
        fprintf(stderr, "recomp: training assistant: club %#x has no human manager\n", the_club);
        return 0;
    }
    uint32_t p[5] = {rt_host_fn("training_build", build), rt_host_fn("training_input", input), 1, 0, 0};
    if (!call_this(PUSH, SCREENS, 5, p)) {
        fprintf(stderr, "recomp: training assistant not opened\n");
        return 0;
    }
    if (!scratch) scratch = rt_alloc_zero(TEXT_SIZE + 16);
    club = the_club;
    manager = m;
    human = h;
    result[0] = 0;
    handled_press = win_press_count();              /* the press that opened the box does not click in it */
    for (int s = 1; s < 10; s++) {
        uint32_t sc = schedule(s);
        fprintf(stderr, "recomp: training assistant slot %d type %d levels %d %d %d %d %d name %.11s\n", s,
                (int8_t)rd8(g_mem, sc + SCHEDULE_TYPE), rd8(g_mem, sc + 4), rd8(g_mem, sc + 5), rd8(g_mem, sc + 6),
                rd8(g_mem, sc + 7), rd8(g_mem, sc + 8), win_str(sc + SCHEDULE_NAME));
    }
    fprintf(stderr, "recomp: training assistant players on slot 8 %d, slot 9 %d\n", players_on(8), players_on(9));
    return 1;
}
