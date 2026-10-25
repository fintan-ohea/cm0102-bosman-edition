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
 * (slice 3) the condition with Full, the morale (+0x45), and the injury with Cure while the player has one, and
 * (slice 4) the contract's wage and end date. The club's balance and transfer funds (slice 5) moved to the Edit
 * Club tab of the club screen (club_editor.c), which uses the funds adjustments kept here.
 * Each click changes the game's memory at once, so the box shows the game's own values. Save keeps the
 * changes and Cancel puts back the values that the player had when the box opened.
 *
 * An attribute is a byte that 0x543540 either returns as it is or puts through a formula with the current
 * ability (docs/research/2026-10-24-player-editor-attributes.md). The box does not copy the formula: a step
 * tries the next byte values one by one and asks 0x543540 for the result, until the shown value moves. A value
 * that no byte reaches stays as it is.
 *
 * Edited games: the first change marks the game as edited. The mark goes into the save as the part
 * bosman_editor.dat (format 1, length, edited 1, the count of funds adjustments, then 12 bytes for each: s32 club
 * id, s64 amount), added after the save, as season.c adds its part. The original reads only parts that end in .tmp, so it never reads this one. A load reads it, a new
 * game clears it, and Save Game suggests "<club> <date> (edited)" (savename.c).
 *
 * Same game (Q152): with the option off the item is not made and the box never opens. The item's text block
 * is made at the first profile build whatever the option, so on and off make the same heap blocks. */
#include "win.h"
#include "options.h"

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

/* The contract (slice 4): ci = int32 [[CONTRACT_INDEX] + id * 79], the record [[CONTRACTS]] + ci * 80. */
#define CONTRACT_INDEX 0xae2d0cu
#define CONTRACTS 0xae1050u         /* the contract manager: the this of get_wage 0x4d0c20 */
#define CONTRACT_WAGE 0x0cu         /* int32 (0x4d0c59) */
#define CONTRACT_END 0x2du          /* Date: s16 day of the year, s16 year, u32 leap flag (0x85e954) */
#define CONTRACT_TYPE 0x35u         /* low 6 bits: 1 rolling, 3 expired, 4 free (0x85e8f0) */
#define END_TEXT 0x85e8f0u          /* thiscall on the contract (text): the profile's "Expires" text */
#define DATE_SET 0x54a130u          /* Date::set, thiscall (day 1 to 31, month 0 to 11, year, weekday or -1) */
#define WAGE_STEP 100

/* Club money (slice 5): the balance, s64 at +0 of the club's finance record. */
#define FINANCES 0xae2a38u          /* the finance table: the this of get_club_finance */
#define CLUB_FINANCE 0x5a0590u      /* thiscall (club): the club's finance record */
#define STAFF_CLUB 0x39u            /* staff: the club, 0 for none */
#define FINANCE_CLUB 0x08u          /* finance record: the club id (0x598803) */
#define TRANSFER_BUDGET 0x598800u   /* thiscall on a finance record, 5 args, ret 0x14: s64 in edx:eax */
#define DISPLAY_AMOUNT 0x43f460u    /* Cash::get_display_amount, thiscall on an s64 (6, 6): rounded, as 0x463b2c */
#define FORMAT_MONEY 0x441c10u      /* cash_format_money, cdecl (text, size, s64): "£14,500,000" (0x463b3d) */
#define MAX_ADJUSTMENTS 256

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
#define FIELD_WAGE (FIELD_CONDITION + 3)
#define FIELD_END (FIELD_CONDITION + 4)
#define FIELDS (FIELD_CONDITION + 5)
#define ABILITY_MAX 200
#define VALUE_MAX 20
#define TEXT_SIZE 0x200u
#define NAME_AT 0x300u              /* scratch + this: the person's name and the attribute names */
#define OUT_AT 0x400u               /* scratch + this: the result of DAYS_SINCE, or an amount of money */
#define MONEY_AT 0x410u             /* scratch + this: the money text, 0x7d0 bytes as the finance screen gives */
#define MONEY_SIZE 0x7d0u
#define SCRATCH_SIZE (MONEY_AT + MONEY_SIZE)

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
static uint32_t old_contract, old_wage;
static uint8_t old_end[8];

/* Transfer funds adjustments: an amount per club id that the wrap of 0x598800 adds to the game's own figure.
 * They go into the save part, and a load or a new game replaces them. */
static struct { int32_t club; int64_t amount; } adjustments[MAX_ADJUSTMENTS];
static int adjustment_count;
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

/* The player's contract record, or 0 for none. */
static uint32_t contract(void)
{
    int32_t ci = (int32_t)rd32(g_mem, rd32(g_mem, CONTRACT_INDEX) + rd32(g_mem, staff) * 79u);
    return ci < 0 ? 0 : rd32(g_mem, rd32(g_mem, CONTRACTS)) + (uint32_t)ci * 80u;
}

/* A contract whose end date means something: not rolling, expired or free (0x85e8f0). */
static int has_end_date(uint32_t c)
{
    int type = (int)(rd8(g_mem, c + CONTRACT_TYPE) & 0x3f);
    return type != 1 && type != 3 && type != 4;
}

/* The finance record of a club (club_editor.c). */
uint32_t editor_finance(uint32_t club)
{
    if (!club) return 0;
    uint32_t a[1] = {club};
    return call_this(CLUB_FINANCE, rd32(g_mem, FINANCES), 1, a);
}

/* The finance record of the player's club, or 0 for a player without a club. */
static uint32_t finance(void) { return editor_finance(rd32(g_mem, staff + STAFF_CLUB)); }

static int64_t balance_of(uint32_t f) { return (int64_t)rd64(g_mem, f); }

int64_t editor_adjustment(int32_t club)
{
    for (int i = 0; i < adjustment_count; i++)
        if (adjustments[i].club == club) return adjustments[i].amount;
    return 0;
}

/* Sets the amount of a club; 0 removes it. 0 when the table is full. */
int editor_set_adjustment(int32_t club, int64_t amount)
{
    for (int i = 0; i < adjustment_count; i++) {
        if (adjustments[i].club != club) continue;
        if (amount) adjustments[i].amount = amount;
        else adjustments[i] = adjustments[--adjustment_count];
        return 1;
    }
    if (!amount) return 1;
    if (adjustment_count == MAX_ADJUSTMENTS) return 0;
    adjustments[adjustment_count].club = club;
    adjustments[adjustment_count++].amount = amount;
    return 1;
}

/* The transfer funds of the finance record f, as the club's finance screen asks for them (0x463aec: 0, 0, 0, 1,
 * 0), with the adjustment. */
/* A thiscall of the original that returns s64 in edx:eax. rt_callback keeps only eax: the same call, keeping
 * edx too. */
static int64_t call_this64(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    for (int i = n - 1; i >= 0; i--) {
        cpu.esp -= 4;
        wr32(g_mem, cpu.esp, args[i]);
    }
    cpu.esp -= 4;
    wr32(g_mem, cpu.esp, RT_RETURN_ADDR);
    rt_call(fn);
    uint32_t lo = cpu.eax, hi = cpu.edx;
    if (!rt_unwinding) cpu = saved;
    return (int64_t)(((uint64_t)hi << 32) | lo);
}

int64_t editor_funds(uint32_t f)
{
    static const uint32_t a[5] = {0, 0, 0, 1, 0};
    return call_this64(TRANSFER_BUDGET, f, 5, a);
}

/* An amount as the game writes money, in the shown currency and money scale (money.c); rounded as the finance
 * screen rounds the transfer funds when round is 1 (Cash::get_display_amount reads an int32). */
void editor_money_text(int64_t v, int round, char *out, size_t room)
{
    if (!scratch) scratch = rt_alloc_zero(SCRATCH_SIZE);
    if (round && v >= INT32_MIN && v <= INT32_MAX) {
        uint32_t at = scratch + OUT_AT;
        wr32(g_mem, at, (uint32_t)(int32_t)v);
        static const uint32_t a[2] = {6, 6};
        v = call_this64(DISPLAY_AMOUNT, at, 2, a);
    } else {
        v = money_shown(v);
    }
    uint32_t f[4] = {scratch + MONEY_AT, MONEY_SIZE, (uint32_t)v, (uint32_t)((uint64_t)v >> 32)};
    call(FORMAT_MONEY, 4, f);
    snprintf(out, room, "%s", win_str(scratch + MONEY_AT));
}

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* Day of the year from 0 into month from 0 and day from 1. */
static void month_day(int yday, int year, int *month, int *day)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11 && yday >= days[m] + (m == 1 && leap(year))) yday -= days[m] + (m == 1 && leap(year)), m++;
    *month = m;
    *day = yday + 1;
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
    fprintf(stderr, " condition %d morale %d injury %d %d", condition(), (int)rd8(g_mem, data + MORALE), status,
            injury_left(status));
    uint32_t c = contract();
    if (c) fprintf(stderr, " wage %d end %d %d", (int32_t)rd32(g_mem, c + CONTRACT_WAGE),
                   (int16_t)rd16(g_mem, c + CONTRACT_END + 2), (int16_t)rd16(g_mem, c + CONTRACT_END));
    else fprintf(stderr, " wage none");
    uint32_t f = finance();
    if (f) fprintf(stderr, " balance %lld funds %lld\n", (long long)balance_of(f), (long long)editor_funds(f));
    else fprintf(stderr, " balance none\n");
}

/* ---- The changes ---- */

void editor_mark_edited(void)
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
            editor_mark_edited();
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
    editor_mark_edited();
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
    editor_mark_edited();
}

static void step_morale(int by)
{
    int m0 = (int)rd8(g_mem, data + MORALE), m = m0 + by;
    if (m < 1) m = 1;
    if (m > VALUE_MAX) m = VALUE_MAX;
    if (m == m0) return;
    wr8(g_mem, data + MORALE, (uint32_t)m);
    fprintf(stderr, "recomp: player editor: staff %d morale %d -> %d\n", (int32_t)rd32(g_mem, staff), m0, m);
    editor_mark_edited();
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
    editor_mark_edited();
}

/* The wage: - and + by WAGE_STEP, the outer buttons by 10%. */
static void step_wage(int op)
{
    uint32_t c = contract();
    if (!c) return;
    int32_t w0 = (int32_t)rd32(g_mem, c + CONTRACT_WAGE), w = w0;
    int64_t step = money_internal(WAGE_STEP) > 0 ? money_internal(WAGE_STEP) : 1;   /* WAGE_STEP as shown */
    int64_t tenth = (int64_t)w0 / 10 > step ? (int64_t)w0 / 10 : step;
    int64_t by = op == 0 ? -step : op == 1 ? step : op == 2 ? -tenth : tenth;
    int64_t v = (int64_t)w0 + by;
    w = v < 0 ? 0 : v > 0x7fffffff ? 0x7fffffff : (int32_t)v;
    if (w == w0) return;
    wr32(g_mem, c + CONTRACT_WAGE, (uint32_t)w);
    fprintf(stderr, "recomp: player editor: staff %d wage %d -> %d\n", (int32_t)rd32(g_mem, staff), w0, w);
    editor_mark_edited();
}

/* The end date: a year earlier or later, through Date::set so the leap flag fits. 29 February becomes 28. The
 * date stays after today. */
static void step_end(int by)
{
    uint32_t c = contract();
    if (!c || !has_end_date(c)) return;
    uint32_t at = c + CONTRACT_END;
    int yday = (int16_t)rd16(g_mem, at), year = (int16_t)rd16(g_mem, at + 2), month, day;
    month_day(yday, year, &month, &day);
    int to = year + by;
    if (month == 1 && day == 29 && !leap(to)) day = 28;
    int today_year = (int16_t)rd16(g_mem, TODAY + 2), today_yday = (int16_t)rd16(g_mem, TODAY);
    if (to < today_year || (to == today_year && yday <= today_yday)) return;
    uint32_t a[4] = {(uint32_t)day, (uint32_t)month, (uint32_t)to, 0xffffffffu};
    call_this(DATE_SET, at, 4, a);
    fprintf(stderr, "recomp: player editor: staff %d contract end %d.%d.%d -> %d.%d.%d\n", (int32_t)rd32(g_mem, staff),
            day, month + 1, year, day, month + 1, (int16_t)rd16(g_mem, at + 2));
    editor_mark_edited();
}

static void step(int field, int op)
{
    static const int by[4] = {-1, 1, -10, 10};
    if (field == 0) step_ability(ABILITY, by[op]);
    else if (field == 1) step_ability(POTENTIAL, by[op]);
    else if (field == FIELD_CONDITION) step_condition(op == 3 ? 0 : by[op] * CONDITION_STEP);
    else if (field == FIELD_MORALE && op < 2) step_morale(by[op]);
    else if (field == FIELD_INJURY && op == 3) cure();
    else if (field == FIELD_WAGE) step_wage(op);
    else if (field == FIELD_END && op < 2) step_end(by[op]);
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
    old_contract = contract();
    if (old_contract) {
        old_wage = rd32(g_mem, old_contract + CONTRACT_WAGE);
        memcpy(old_end, g_mem + old_contract + CONTRACT_END, sizeof old_end);
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
    if (old_contract) {
        wr32(g_mem, old_contract + CONTRACT_WAGE, old_wage);
        memcpy(g_mem + old_contract + CONTRACT_END, old_end, sizeof old_end);
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
/* The contract row: wage (name, -10%, -, value, +, +10%) and end date (name, -1y, value, +1y). */
static const uint8_t weights_contract[] = {14, 9, 5, 16, 5, 9, 3, 18, 8, 22, 8};
/* An attribute row: name, -, value, +. */
static const uint8_t weights_attribute[] = {50, 10, 12, 10};
#define ROW_X1 160
#define ROW_X2 729
#define COLUMN_W 190
#define ROWS_PER_COLUMN 11
#define ROW_Y 214
#define ROW_STEP 24

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

    /* The contract: wage and end date, or "No contract". */
    int k = area(ROW_X1, 182, ROW_X2, 208, 11, weights_contract, 1, 0);
    uint32_t con = contract();
    plain(k, 0, "Wage", light);
    if (!con) {
        value_text(k, 3, "No contract");
    } else {
        button(k, 1, "-10%", FIELD_WAGE, 2, green);
        button(k, 2, "-", FIELD_WAGE, 0, green);
        char money[64];
        editor_money_text((int32_t)rd32(g_mem, con + CONTRACT_WAGE), 0, money, sizeof money);
        snprintf(s, sizeof s, "%s/w", money);
        value_text(k, 3, s);
        button(k, 4, "+", FIELD_WAGE, 1, green);
        button(k, 5, "+10%", FIELD_WAGE, 3, green);
        plain(k, 7, "Contract ends", light);
        uint32_t t[1] = {scratch + NAME_AT};
        call_this(END_TEXT, con, 1, t);
        char end[64];
        snprintf(end, sizeof end, "%s", win_str(scratch + NAME_AT));
        if (has_end_date(con)) button(k, 8, "-1y", FIELD_END, 0, green);
        value_text(k, 9, end);
        if (has_end_date(con)) button(k, 10, "+1y", FIELD_END, 1, green);
    }

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

void o_00598800(void);

/* Finance::get_transfer_budget, thiscall on a finance record (5 args): the game's figure plus the club's
 * adjustment. A game without adjustments gets the original's result unchanged. */
RT_WRAP(00598800, transfer_budget)
{
    uint32_t f = cpu.ecx;
    o_00598800();
    if (!adjustment_count || rt_unwinding) return;
    int64_t extra = editor_adjustment((int32_t)rd32(g_mem, f + FINANCE_CLUB));
    if (!extra) return;
    int64_t v = (int64_t)(((uint64_t)cpu.edx << 32) | cpu.eax) + extra;
    if (v < 0) v = 0;
    cpu.eax = (uint32_t)v;
    cpu.edx = (uint32_t)((uint64_t)v >> 32);
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

/* ---- The mark in the save (savepart.c) ---- */

#define PART_NAME "bosman_editor.dat"
#define PART_SIZE 16u               /* format, length, edited, count of adjustments */
#define ENTRY_SIZE 12u              /* an adjustment: s32 club id, s64 amount */
#define PART_MAX (PART_SIZE + MAX_ADJUSTMENTS * ENTRY_SIZE)

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static int add_part(const char *host)
{
    static uint8_t part[PART_MAX];
    uint32_t size = PART_SIZE + (uint32_t)adjustment_count * ENTRY_SIZE;
    memset(part, 0, size);
    put32(part, 1);
    put32(part + 4, size);
    put32(part + 8, 1);
    put32(part + 12, (uint32_t)adjustment_count);
    for (int i = 0; i < adjustment_count; i++) {
        uint8_t *e = part + PART_SIZE + i * ENTRY_SIZE;
        put32(e, (uint32_t)adjustments[i].club);
        memcpy(e + 4, &adjustments[i].amount, 8);
    }
    return savepart_write(host, PART_NAME, part, size);
}

static int loading_count;            /* the adjustments of the save being loaded */
static struct { int32_t club; int64_t amount; } loading[MAX_ADJUSTMENTS];

/* 1 when the save has the part with the edited mark, with its adjustments in loading. A part of slice 1 has 16
 * bytes and no adjustments. */
static int read_mark(const char *host)
{
    static uint8_t part[PART_MAX];
    loading_count = 0;
    int size = savepart_read(host, PART_NAME, part, sizeof part);
    if (size < (int)PART_SIZE || get32(part) < 1 || get32(part + 8) != 1) return 0;
    uint32_t n = get32(part + 12);
    if (n <= MAX_ADJUSTMENTS && PART_SIZE + n * ENTRY_SIZE <= (uint32_t)size) {
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t *e = part + PART_SIZE + i * ENTRY_SIZE;
            loading[i].club = (int32_t)get32(e);
            memcpy(&loading[i].amount, e + 4, 8);
        }
        loading_count = (int)n;
    }
    return 1;
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
    adjustment_count = 0;
    for (int i = 0; i < loading_count; i++) editor_set_adjustment(loading[i].club, loading[i].amount);
    if (edited) fprintf(stderr, "recomp: player editor: the save is an edited game, %d funds adjustments\n",
                        adjustment_count);
}

void editor_new_game(void)
{
    edited = 0;
    adjustment_count = 0;
}
