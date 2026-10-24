/* The player editor (fork of fintan-ohea; docs/specs/2026-10-24-player-editor-spec.md, slice 1).
 *
 * With player_editor = on, the Action drop-down of a person's profile (build 0x870700) ends with "Edit player"
 * when the person is a player (staff +0x61). The item follows "Add/Edit Manager Note", the item that the build
 * makes for every profile: its widget create at 0x874653 (call site profile_note) gets the Action popup as its
 * last argument. The profile input 0x888260 opens the box for the staff of node slot 0, the slot that the
 * build reads as its person (0x8707ba).
 *
 * The box is a screen of the runtime on the game's screen stack, built as the Options screen
 * (options_screen.c): the frame of Game Settings (0x823950), pushed with flag 1. It shows current ability
 * (player data +0x05), potential ability (+0x07) and the 31 attributes of 0x5431e0, with - and + buttons, and
 * (slice 3) the condition with Full, the morale (+0x45), and the injury with Cure while the player has one.
 * Each click changes the game's memory at once, so the box shows the game's own values. Save keeps the
 * changes and Cancel puts back the values that the player had when the box opened.
 *
 * An attribute is a byte that 0x543540 either returns as it is or puts through a formula with the current
 * ability (docs/research/2026-10-24-player-editor-attributes.md). The box does not copy the formula: a step
 * tries the next byte values one by one and asks 0x543540 for the result, until the shown value moves. A value
 * that no byte reaches stays as it is.
 *
 * Edited games: the first change marks the game as edited. The mark goes into the save as the part
 * bosman_editor.dat (16 bytes: format 1, length 16, edited 1, 0), added after the save, as season.c adds its
 * part. The original reads only parts that end in .tmp, so it never reads this one. A load reads it, a new
 * game clears it, and Save Game suggests "<club> <date> (edited)" (savename.c).
 *
 * Same game (Q152): with the option off the item is not made and the box never opens. The item's text block
 * is made at the first profile build whatever the option, so on and off make the same heap blocks. */
#include "win.h"
#include "options.h"
#include "../host.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint32_t rt_site_profile_note;

#define SCREENS 0xb74450u           /* the screen stack S */
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define BACKGROUND_OFF 0x41c0d0u    /* cdecl (0): as the Game Settings build (0x823a57) */
#define COLOUR 0x5e4800u            /* cdecl (0, green, 0, 0): an RGB565 colour (0x823ac0) */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x55f990u     /* thiscall on a Display, 18 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define PERSON_NAME 0x53dba0u       /* cdecl (staff, buffer, size, 1): the full name (0x8766bb) */
#define ATTRIBUTE 0x543540u         /* cdecl (staff, player data, index 1..31): the shown value */
#define ATTRIBUTE_NAME 0x5428a0u    /* cdecl (index 1..31, text, size >= 100, 0): the long name */

#define DISPLAYS 0xb74c4cu          /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu

#define TEXT_YELLOW 0xaebdecu       /* u16 colours of the Game Settings box */
#define TEXT_TITLE 0xaebdeau
#define TEXT_LIGHT 0xae3154u
#define BUTTON_COLOUR 0xae317eu
#define BUTTON_TEXT 0xae3184u

#define PLAYER_DATA 0x61u           /* staff: the player data, 0 for no player */
#define ABILITY 0x05u               /* player data: s16 current ability, 1 to 200 */
#define POTENTIAL 0x07u             /* player data: s16 potential ability (0x8e8345) */
#define DETERMINATION 0x58u         /* staff: attribute 10, the only one outside the player data (0x54376c) */
#define MORALE 0x45u                /* player data: morale, 1 to 20 (0x544710) */

/* Condition and injury (docs/research/2026-10-24-player-editor-attributes.md, slice 3). */
#define INJURIES 0xb63d28u          /* the injury object: the this of 0x62c400 and 0x62c590 */
#define GET_CONDITION 0x62c590u     /* thiscall (staff, 1): the condition in %, after the day's update */
#define INJURY_STATUS 0x62c400u     /* thiscall (staff): 0 fit, 1 recovering, 2 injured */
#define DAYS_SINCE 0x54cc30u        /* thiscall on TODAY (out, date low, date high): s16 days, u16 years */
#define TODAY 0xae2c90u
#define INJURY_ROW 31u              /* [obj] + id * 31: +0x08 fitness ceiling, +0x0a condition, +0x0e record */
#define INJURY_RECORD 48u           /* [obj + 4] + index * 48: +0x20 date, +0x28 recovery days, +0x2a injured days */
#define CONDITION_FULL 10000
#define CONDITION_STEP 500

#define ITEM_ID 0x7d40u             /* "Edit player" in the Action popup. Ids that the original does not use. */
#define STEP_ID 0x7d41u             /* data: field * 4 + step (0: -1, 1: +1, 2: -10, 3: +10) */
#define SAVE_ID 0x7d42u
#define CANCEL_ID 0x7d43u
#define NEW_SCREEN 0xfffffffcu      /* -4: build the screen on top, after a push */
#define REBUILD 0xfffffff5u         /* -0xb: build the box again */
#define CLOSE 0xfffffff7u           /* -9: close the box, as Ok and Cancel of Game Settings (0x8259a0) */

#define ATTRIBUTES 31
/* 0 current ability, 1 potential ability, 2.. attributes 1..31, then condition, morale and injury. */
#define FIELD_CONDITION (2 + ATTRIBUTES)
#define FIELD_MORALE (FIELD_CONDITION + 1)
#define FIELD_INJURY (FIELD_CONDITION + 2)
#define FIELDS (FIELD_CONDITION + 3)
#define ABILITY_MAX 200
#define VALUE_MAX 20
#define TEXT_SIZE 0x200u
#define NAME_AT 0x300u              /* scratch + this: the person's name and the attribute names */
#define OUT_AT 0x400u               /* scratch + this: the result of DAYS_SINCE */
#define SCRATCH_SIZE 0x410u

/* The byte of each attribute (index 1..31): player data + offset, or the staff for Determination. */
static const uint8_t attribute_byte[ATTRIBUTES + 1] = {
    0, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x43, 0x23, 0x24, DETERMINATION, 0x26, 0x27, 0x28, 0x2a, 0x2b, 0x2f,
    0x2e, 0x31, 0x32, 0x33, 0x36, 0x37, 0x39, 0x3a, 0x29, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x44};

static uint32_t scratch;
static uint32_t staff, data;        /* the player in the box */
static int16_t old_ability, old_potential;
static uint8_t old_bytes[ATTRIBUTES + 1], old_morale;
static int16_t old_ceiling, old_condition, old_recovery, old_injured;
static uint32_t old_record;         /* the injury record when the box opened, 0 for none */
static int edited, edited_before;   /* the game is edited; and before the box opened */
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

static uint32_t colour16(uint32_t at) { return rd16(g_mem, at); }

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, TEXT_SIZE);
    return scratch;
}

/* The address of the byte of attribute k. */
static uint32_t byte_at(int k) { return (k == 10 ? staff : data) + attribute_byte[k]; }

static int shown(int k)
{
    uint32_t a[3] = {staff, data, (uint32_t)k};
    return (int)(call(ATTRIBUTE, 3, a) & 0xff);
}

/* The player's row of the injury object. */
static uint32_t injury_row(void)
{
    return rd32(g_mem, rd32(g_mem, INJURIES)) + rd32(g_mem, staff) * INJURY_ROW;
}

/* The player's injury record, or 0. */
static uint32_t injury_record(void)
{
    int32_t index = (int32_t)rd32(g_mem, injury_row() + 0x0e);
    return index < 0 ? 0 : rd32(g_mem, rd32(g_mem, INJURIES) + 4) + (uint32_t)index * INJURY_RECORD;
}

/* The condition in %, as the profile shows it (0x87ab38). The call brings the row up to today first. */
static int condition(void)
{
    uint32_t a[2] = {staff, 1};
    return (int16_t)call_this(GET_CONDITION, rd32(g_mem, INJURIES), 2, a);
}

static int injury_status(void)
{
    uint32_t a[1] = {staff};
    return (int)(call_this(INJURY_STATUS, rd32(g_mem, INJURIES), 1, a) & 0xff);
}

/* The days since the injury of record, as 0x62c400 counts them. */
static int injury_days(uint32_t record)
{
    uint32_t a[3] = {scratch + OUT_AT, rd32(g_mem, record + 0x20), rd32(g_mem, record + 0x24)};
    call_this(DAYS_SINCE, TODAY, 3, a);
    return (int16_t)rd16(g_mem, scratch + OUT_AT) + (int)rd16(g_mem, scratch + OUT_AT + 2) * 365;
}

/* The days left of the injury (status 2) or of the recovery (status 1), else 0. */
static int injury_left(int status)
{
    uint32_t record = injury_record();
    if (!record || !status) return 0;
    return (int16_t)rd16(g_mem, record + (status == 2 ? 0x2a : 0x28)) - injury_days(record);
}

static const char *morale_word(int m)
{
    return m < 4 ? "Very Low" : m < 8 ? "Low" : m < 12 ? "Ok" : m < 15 ? "Good" : m < 18 ? "Very Good" : "Superb";
}

int editor_game_edited(void) { return edited; }

/* "recomp: player editor <what> staff <id> ca <n> pa <n> values <31 shown values>", for the test
 * tests/smoke/player_editor_run.py. */
static void log_values(const char *what)
{
    fprintf(stderr, "recomp: player editor %s staff %d ca %d pa %d values", what, (int32_t)rd32(g_mem, staff),
            (int16_t)rd16(g_mem, data + ABILITY), (int16_t)rd16(g_mem, data + POTENTIAL));
    for (int k = 1; k <= ATTRIBUTES; k++) fprintf(stderr, " %d", shown(k));
    int status = injury_status();
    fprintf(stderr, " condition %d morale %d injury %d %d\n", condition(), (int)rd8(g_mem, data + MORALE), status,
            injury_left(status));
}

/* ---- The changes ---- */

static void mark_edited(void)
{
    if (!edited) fprintf(stderr, "recomp: player editor: the game is now edited\n");
    edited = 1;
}

/* One step of attribute k up (dir 1) or down (-1): the next byte that moves the shown value, kept in 1..20. */
static void step_attribute(int k, int dir)
{
    uint32_t at = byte_at(k);
    int b0 = (int8_t)rd8(g_mem, at), v0 = shown(k);
    for (int b = b0 + dir; b >= -128 && b <= 127; b += dir) {
        wr8(g_mem, at, (uint32_t)b);
        int v = shown(k);
        if (v < 1 || v > VALUE_MAX) break;
        if (dir > 0 ? v > v0 : v < v0) {
            fprintf(stderr, "recomp: player editor: staff %d attribute %d %d -> %d (byte %d -> %d)\n",
                    (int32_t)rd32(g_mem, staff), k, v0, v, b0, b);
            mark_edited();
            return;
        }
    }
    wr8(g_mem, at, (uint32_t)b0);
}

/* Current ability stays at or below potential ability: a test game with current ability 165 and potential 160
 * had current ability 160 a day later (tests/smoke/player_editor_run.py). Inference: the game cuts current
 * ability to potential. So a step keeps the pair in order: current ability above potential raises potential,
 * potential below current ability lowers it. */
static void step_ability(uint32_t offset, int by)
{
    int v0 = (int16_t)rd16(g_mem, data + offset), v = v0 + by;
    if (v < 1) v = 1;
    if (v > ABILITY_MAX) v = ABILITY_MAX;
    if (v == v0) return;
    wr16(g_mem, data + offset, (uint32_t)v);
    int ca = (int16_t)rd16(g_mem, data + ABILITY), pa = (int16_t)rd16(g_mem, data + POTENTIAL);
    if (ca > pa) wr16(g_mem, data + (offset == ABILITY ? POTENTIAL : ABILITY), (uint32_t)v);
    fprintf(stderr, "recomp: player editor: staff %d %s %d -> %d (ca %d, pa %d)\n", (int32_t)rd32(g_mem, staff),
            offset == ABILITY ? "current ability" : "potential ability", v0, v,
            (int16_t)rd16(g_mem, data + ABILITY), (int16_t)rd16(g_mem, data + POTENTIAL));
    mark_edited();
}

/* Condition: by steps the stored condition (+0x0a) by CONDITION_STEP; Full (by 0) sets it and the fitness
 * ceiling (+0x08) to 10000, so the shown condition is 100% (or 90% while recovering, 0x62c603). */
static void step_condition(int by)
{
    uint32_t row = injury_row();
    int c0 = (int16_t)rd16(g_mem, row + 0x0a), v0 = condition();
    int c = by ? c0 + by : CONDITION_FULL;
    if (c < 100) c = 100;
    if (c > CONDITION_FULL) c = CONDITION_FULL;
    if (!by) wr16(g_mem, row + 0x08, CONDITION_FULL);
    wr16(g_mem, row + 0x0a, (uint32_t)c);
    fprintf(stderr, "recomp: player editor: staff %d condition %d%% -> %d%% (stored %d -> %d)\n",
            (int32_t)rd32(g_mem, staff), v0, condition(), c0, c);
    mark_edited();
}

static void step_morale(int by)
{
    int m0 = (int)rd8(g_mem, data + MORALE), m = m0 + by;
    if (m < 1) m = 1;
    if (m > VALUE_MAX) m = VALUE_MAX;
    if (m == m0) return;
    wr8(g_mem, data + MORALE, (uint32_t)m);
    fprintf(stderr, "recomp: player editor: staff %d morale %d -> %d\n", (int32_t)rd32(g_mem, staff), m0, m);
    mark_edited();
}

/* Cure: the injury and its recovery end today, as when their days have passed (0x62c400). */
static void cure(void)
{
    uint32_t record = injury_record();
    int status = injury_status();
    if (!record || !status) return;
    int days = injury_days(record);
    if (days < 0) days = 0;
    for (uint32_t at = 0x28; at <= 0x2a; at += 2)
        if ((int16_t)rd16(g_mem, record + at) > days) wr16(g_mem, record + at, (uint32_t)days);
    fprintf(stderr, "recomp: player editor: staff %d injury status %d -> %d after %d days\n",
            (int32_t)rd32(g_mem, staff), status, injury_status(), days);
    mark_edited();
}

static void step(int field, int op)
{
    static const int by[4] = {-1, 1, -10, 10};
    if (field == 0) step_ability(ABILITY, by[op]);
    else if (field == 1) step_ability(POTENTIAL, by[op]);
    else if (field == FIELD_CONDITION) step_condition(op == 3 ? 0 : by[op] * CONDITION_STEP);
    else if (field == FIELD_MORALE && op < 2) step_morale(by[op]);
    else if (field == FIELD_INJURY && op == 3) cure();
    else if (field < FIELD_CONDITION && op < 2) step_attribute(field - 1, by[op]);
}

static void snapshot(void)
{
    old_ability = (int16_t)rd16(g_mem, data + ABILITY);
    old_potential = (int16_t)rd16(g_mem, data + POTENTIAL);
    for (int k = 1; k <= ATTRIBUTES; k++) old_bytes[k] = (uint8_t)rd8(g_mem, byte_at(k));
    old_morale = (uint8_t)rd8(g_mem, data + MORALE);
    condition();                                    /* the row up to today, so Cancel restores today's values */
    uint32_t row = injury_row();
    old_ceiling = (int16_t)rd16(g_mem, row + 0x08);
    old_condition = (int16_t)rd16(g_mem, row + 0x0a);
    old_record = injury_record();
    if (old_record) {
        old_recovery = (int16_t)rd16(g_mem, old_record + 0x28);
        old_injured = (int16_t)rd16(g_mem, old_record + 0x2a);
    }
    edited_before = edited;
}

static void restore(void)
{
    wr16(g_mem, data + ABILITY, (uint32_t)old_ability);
    wr16(g_mem, data + POTENTIAL, (uint32_t)old_potential);
    for (int k = 1; k <= ATTRIBUTES; k++) wr8(g_mem, byte_at(k), old_bytes[k]);
    wr8(g_mem, data + MORALE, old_morale);
    uint32_t row = injury_row();
    wr16(g_mem, row + 0x08, (uint32_t)old_ceiling);
    wr16(g_mem, row + 0x0a, (uint32_t)old_condition);
    if (old_record) {
        wr16(g_mem, old_record + 0x28, (uint32_t)old_recovery);
        wr16(g_mem, old_record + 0x2a, (uint32_t)old_injured);
    }
    edited = edited_before;
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

/* A value: a yellow label with a frame, as Game Settings (0x82477b). */
static void value_text(int a, int column, const char *s)
{
    uint32_t y = colour16(TEXT_YELLOW);
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 0x221, y, 0xc, 2, y, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

static void value(int a, int column, int v)
{
    char s[8];
    snprintf(s, sizeof s, "%d", v);
    value_text(a, column, s);
}

/* A step button: green, as the value buttons of Game Settings (0x8247a4). */
static void button(int a, int column, const char *s, int field, int op, uint32_t green)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 0xc, 2, green, text(s), STEP_ID,
                      (uint32_t)(field * 4 + op)};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

static void plain(int a, int column, const char *s, uint32_t colour)
{
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 1, 2, colour, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

/* The ability row: name, -10, -, value, +, +10 for current and then potential ability. */
static const uint8_t weights_ability[] = {27, 8, 6, 9, 6, 8, 4, 27, 8, 6, 9, 6, 8};
/* The fitness row: condition (name, -, value, +, Full), morale (name, -, value, +) and injury (name, value,
 * Cure), with small gaps. */
static const uint8_t weights_fitness[] = {18, 5, 9, 5, 8, 3, 14, 5, 16, 5, 3, 13, 22, 8};
/* An attribute row: name, -, value, +. */
static const uint8_t weights_attribute[] = {50, 10, 12, 10};
#define ROW_X1 160
#define ROW_X2 729
#define COLUMN_W 190
#define ROWS_PER_COLUMN 11
#define ROW_Y 186
#define ROW_STEP 27

static void ability_cells(int a, int first, const char *label, int v, int field, uint32_t green, uint32_t light)
{
    plain(a, first, label, light);
    button(a, first + 1, "-10", field, 2, green);
    button(a, first + 2, "-", field, 0, green);
    value(a, first + 3, v);
    button(a, first + 4, "+", field, 1, green);
    button(a, first + 5, "+10", field, 3, green);
}

static void editor_build(void)
{
    area(0, 0, 0x31f, 0x257, 1, NULL, 2, 0);                    /* the dark cover, 0x823a92 */
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t g[4] = {0, 0xc0, 0, 0};
    uint32_t green = call(COLOUR, 4, g) & 0xffff;
    uint32_t light = colour16(TEXT_LIGHT);
    area(145, 50, 744, 549, 1, NULL, 0x30, box_colour);
    box_text(155, 58, 734, 92, 0x30, colour16(TEXT_TITLE), 0xc, 4, colour16(BUTTON_TEXT), "Player editor");
    uint32_t n[4] = {staff, scratch + NAME_AT, 0x100, 1};
    call(PERSON_NAME, 4, n);
    char name[0x100];
    snprintf(name, sizeof name, "%s", win_str(scratch + NAME_AT));
    box_text(160, 96, 729, 114, 1, 0, 0xc, 3, colour16(TEXT_YELLOW), name);

    int a = area(ROW_X1, 120, ROW_X2, 146, 13, weights_ability, 1, 0);
    ability_cells(a, 0, "Current ability", (int16_t)rd16(g_mem, data + ABILITY), 0, green, light);
    ability_cells(a, 7, "Potential ability", (int16_t)rd16(g_mem, data + POTENTIAL), 1, green, light);

    /* Condition, morale and injury: - and + steps, Full, and Cure while the player is injured or recovering. */
    int f = area(ROW_X1, 152, ROW_X2, 178, 14, weights_fitness, 1, 0);
    char s[32];
    plain(f, 0, "Condition", light);
    button(f, 1, "-", FIELD_CONDITION, 0, green);
    snprintf(s, sizeof s, "%d%%", condition());
    value_text(f, 2, s);
    button(f, 3, "+", FIELD_CONDITION, 1, green);
    button(f, 4, "Full", FIELD_CONDITION, 3, green);
    plain(f, 6, "Morale", light);
    button(f, 7, "-", FIELD_MORALE, 0, green);
    int m = (int)rd8(g_mem, data + MORALE);
    snprintf(s, sizeof s, "%s %d", morale_word(m), m);
    value_text(f, 8, s);
    button(f, 9, "+", FIELD_MORALE, 1, green);
    plain(f, 11, "Injury", light);
    int status = injury_status();
    if (status) snprintf(s, sizeof s, "%s %dd", status == 2 ? "Injured" : "Recov.", injury_left(status));
    else snprintf(s, sizeof s, "None");
    value_text(f, 12, s);
    if (status) button(f, 13, "Cure", FIELD_INJURY, 3, green);

    for (int k = 1; k <= ATTRIBUTES; k++) {
        int col = (k - 1) / ROWS_PER_COLUMN, row = (k - 1) % ROWS_PER_COLUMN;
        int x = ROW_X1 + col * COLUMN_W, y = ROW_Y + row * ROW_STEP;
        int r = area(x, y, x + COLUMN_W - 6, y + ROW_STEP - 3, 4, weights_attribute, 1, 0);
        uint32_t na[4] = {(uint32_t)k, scratch + NAME_AT, 100, 0};
        call(ATTRIBUTE_NAME, 4, na);
        char label[100];
        snprintf(label, sizeof label, "%s", win_str(scratch + NAME_AT));
        plain(r, 0, label, light);
        button(r, 1, "-", k + 1, 0, green);
        value(r, 2, shown(k));
        button(r, 3, "+", k + 1, 1, green);
    }
    box_text(ROW_X1, 485, ROW_X2, 504, 1, 0, 1, 1, light,
             "Changes show at once. Cancel puts back the values from when the box opened.");

    int b = area(155, 509, 734, 539, 2, NULL, 1, 0);
    uint32_t save[11] = {(uint32_t)b, 0, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                         text("Save"), SAVE_ID, 0};
    call_this(CELL_BUTTON, SCREENS, 11, save);
    uint32_t cancel[11] = {(uint32_t)b, 1, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                           text("Cancel"), CANCEL_ID, 0};
    call_this(CELL_BUTTON, SCREENS, 11, cancel);
    rt_ret_cdecl(0);
}

/* The input of the box: cdecl (s16 widget). One click for each press of the mouse button, as the Options
 * screen: a held button repeats its widget, and after a build that widget can be another button. */
static void editor_input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    uint32_t r = 0;
    if (w >= 0 && win_press_count() == handled_press) w = -1;
    if (w >= 0) {
        handled_press = win_press_count();
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at), d = rd32(g_mem, at + 0x40);
        if (id == SAVE_ID) {
            log_values("saved");
            r = CLOSE;
        } else if (id == CANCEL_ID) {
            restore();
            log_values("cancelled");
            r = CLOSE;
        } else if (id == STEP_ID && d / 4 < FIELDS) {
            step((int)(d / 4), (int)(d % 4));
            r = REBUILD;
        }
    }
    rt_ret_cdecl(r);
}

static int editor_open(uint32_t person)
{
    if (!person || !rd32(g_mem, person + PLAYER_DATA)) return 0;
    uint32_t p[5] = {rt_host_fn("editor_build", editor_build), rt_host_fn("editor_input", editor_input), 1, 0, 0};
    if (!call_this(PUSH, SCREENS, 5, p)) {
        fprintf(stderr, "recomp: player editor not opened\n");
        return 0;
    }
    if (!scratch) scratch = rt_alloc_zero(SCRATCH_SIZE);
    staff = person;
    data = rd32(g_mem, person + PLAYER_DATA);
    set_slot(0, staff);
    snapshot();
    handled_press = win_press_count();              /* the press that opened the box does not click in it */
    log_values("opened");
    return 1;
}

/* ---- The profile ---- */

static uint32_t item_args[18], item_display, item_label;

int editor_widget_before(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_profile_note) return 0;
    if (!item_label) item_label = rt_alloc_str("Edit player");      /* on and off: the same heap blocks */
    if (!options_bool(OPT_PLAYER_EDITOR)) return 0;
    for (int i = 0; i < 18; i++) item_args[i] = rt_arg(i);
    item_display = cpu.ecx;
    return 1;
}

/* After the Manager Note item: one more item in the same popup, for a player only. */
void editor_widget_after(void)
{
    uint32_t person = node_slot(0);
    if (!person || !rd32(g_mem, person + PLAYER_DATA)) return;
    uint32_t args[18];
    memcpy(args, item_args, sizeof args);
    args[13] = item_label;
    args[15] = ITEM_ID;
    args[16] = 0;
    uint32_t eax = cpu.eax;
    call_this(WIDGET_CREATE, item_display, 18, args);
    cpu.eax = eax;
}

void o_00888260(void);

/* The input of the profile, cdecl (s16 widget): the item opens the box. */
RT_WRAP(00888260, profile_input)
{
    int16_t w = (int16_t)rt_arg(0);
    if (w >= 0 && options_bool(OPT_PLAYER_EDITOR)) {
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        if ((uint16_t)rd32(g_mem, at) == ITEM_ID && editor_open(node_slot(0))) {
            rt_ret_cdecl(NEW_SCREEN);
            return;
        }
    }
    o_00888260();
}

/* ---- The mark in the save ---- */

#define PART_NAME "bosman_editor.dat"
#define PART_SIZE 16u
#define HEADER_ROWS 1000u
#define ROW_SIZE 0x10cu
#define ROW_NAME 8u
#define NAME_SIZE 260u
#define MAX_PARTS 999u              /* zopen 0x9326e0 refuses a new part at 999 (season.c) */

static int read_at(int fd, int64_t at, void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_read(fd, buf, n) == (long)n;
}

static int write_at(int fd, int64_t at, const void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_write(fd, buf, n) == (long)n;
}

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* The part's bytes as the container stores them: as they are (version 1 and 3), or with the byte run code of
 * zwrite 0x932e10 (version 2 and 4), which stores a byte above 0x80 or a run as a count and a byte. */
static uint32_t stored_form(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t version)
{
    if (version != 2 && version != 4) {
        memcpy(out, in, n);
        return n;
    }
    uint32_t o = 0;
    for (uint32_t i = 0; i < n;) {
        uint32_t j = i + 1;
        while (j < n && in[j] == in[i] && j - i < 0x7f) j++;
        if (j - i < 2 && in[i] < 0x81) out[o++] = in[i];
        else out[o++] = (uint8_t)(j - i + 0x80), out[o++] = in[i];
        i = j;
    }
    return o;
}

/* Finds the part's row: its index, or count when the file has none. -1 when the file is no container. */
static int find_row(int fd, uint32_t *version, uint32_t *count, uint8_t *row)
{
    uint8_t head[12];
    if (!read_at(fd, 0, head, 12)) return -1;
    *version = get32(head);
    *count = get32(head + 8);
    if (*version < 1 || *version > 4 || *count > HEADER_ROWS) return -1;
    for (uint32_t k = 0; k < *count; k++) {
        if (!read_at(fd, 12 + (int64_t)k * ROW_SIZE, row, ROW_SIZE)) return -1;
        if (!strncmp((const char *)row + ROW_NAME, PART_NAME, NAME_SIZE)) return (int)k;
    }
    return (int)*count;
}

static int add_part(const char *host)
{
    int fd = host_open(host, O_RDWR, 0);
    if (fd < 0) return 1;
    int bad = 1;
    uint32_t version, count;
    uint8_t row[ROW_SIZE], part[PART_SIZE] = {0}, stored[2 * PART_SIZE];
    int at = find_row(fd, &version, &count, row);
    if (at < 0 || ((uint32_t)at == count && count >= MAX_PARTS)) goto done;
    put32(part, 1);
    put32(part + 4, PART_SIZE);
    put32(part + 8, 1);
    uint32_t n = stored_form(part, PART_SIZE, stored, version);
    int64_t end = host_seek(fd, 0, SEEK_END);
    if (end < 12 + (int64_t)HEADER_ROWS * ROW_SIZE || end + n > 0xfffffffe) goto done;
    /* The bytes, then the row, then the count, as season.c: a write that stops leaves the old file whole. */
    if (!write_at(fd, end, stored, n)) goto done;
    memset(row, 0, sizeof row);
    put32(row, (uint32_t)end);
    put32(row + 4, PART_SIZE);
    memcpy(row + ROW_NAME, PART_NAME, sizeof PART_NAME);
    if (!write_at(fd, 12 + (int64_t)at * ROW_SIZE, row, ROW_SIZE)) goto done;
    if ((uint32_t)at == count) {
        uint8_t c[4];
        put32(c, count + 1);
        if (!write_at(fd, 8, c, 4)) goto done;
    }
    bad = 0;
done:
    host_close(fd);
    return bad;
}

/* The decoder of zread 0x932c30 (season.c): n bytes out of room stored bytes. 0 when the bytes end early. */
static int decode(const uint8_t *in, uint32_t room, uint8_t *out, uint32_t n)
{
    uint32_t o = 0, i = 0;
    while (o < n && i < room) {
        uint8_t b = in[i++];
        if (b < 0x81) { out[o++] = b; continue; }
        if (i >= room) return 0;
        for (uint32_t k = 0; k < (uint32_t)(b - 0x80) && o < n; k++) out[o++] = in[i];
        i++;
    }
    return o == n;
}

/* 1 when the save has the part with the edited mark. A stored part of 16 bytes takes at most 32 bytes. */
static int read_mark(const char *host)
{
    int fd = host_open(host, O_RDONLY, 0);
    if (fd < 0) return 0;
    uint32_t version, count;
    uint8_t row[ROW_SIZE], stored[2 * PART_SIZE], part[PART_SIZE];
    int at = find_row(fd, &version, &count, row), mark = 0;
    if (at >= 0 && (uint32_t)at < count && get32(row + 4) == PART_SIZE) {
        int64_t end = host_seek(fd, 0, SEEK_END), offset = get32(row);
        uint32_t room = end - offset < (int64_t)sizeof stored ? (uint32_t)(end - offset) : (uint32_t)sizeof stored;
        int ok = end > offset && read_at(fd, offset, stored, room);
        if (ok && (version == 2 || version == 4)) ok = decode(stored, room, part, PART_SIZE);
        else if (ok) ok = room >= PART_SIZE && (memcpy(part, stored, PART_SIZE), 1);
        mark = ok && get32(part) >= 1 && get32(part + 8) == 1;
    }
    host_close(fd);
    return mark;
}

/* After a save that wrote path (season.c). A failed mark only costs the name of later saves. */
void editor_saved(const char *path)
{
    if (!edited) return;
    char host[2048];
    if (win_existing_file(path, host, sizeof host) && !add_part(host)) return;
    fprintf(stderr, "cm0102_recomp: the edited mark could not be added to the save %s.\n", path);
}

static int loading_mark;

/* Before a load of path (season.c), and after it with its result. */
void editor_loading(const char *path)
{
    char host[2048];
    loading_mark = win_existing_file(path, host, sizeof host) && read_mark(host);
}

void editor_loaded(int ok)
{
    if (!ok) return;
    edited = loading_mark;
    if (edited) fprintf(stderr, "recomp: player editor: the save is an edited game\n");
}

void editor_new_game(void) { edited = 0; }
