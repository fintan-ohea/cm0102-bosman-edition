/* Value range (fork of fintan-ohea; docs/specs/2026-10-30-value-range-spec.md).
 *
 * With value_range = on, the Value row of the search Filters screen (build 0x8133a0, input 0x818100) has From and
 * To drop-downs after the original's Range drop-down. A search then lists the players whose value lies from the
 * From amount to the To amount.
 *
 * The original (docs/research/2026-10-30-value-range.md): the filter block (0x99 bytes; node slot 1 of the Filters
 * screen holds the copy that Ok applies) has the value flag 0x8000000 in its mask at +0 and the value band at
 * +0x23, a byte from -1 to 7 (SearchFilters::set_value_band 0x807310). player_matches_filter 0x807450 tests the
 * band only for -1 (affordable, 0x8079d8), 0 to 6 (value at most the band's amount, 0x807a27) and 7 (more than band
 * 6, 0x807a6e); the value is staff +0x52. Any other byte gives no value test. So a range is kept in the same byte,
 * 8 and up, one number for each pair of From and To: a saved search keeps it, and the original's own tests and the
 * text of its drop-down (0x81680e..0x8168c9) leave it alone.
 *
 * The screen: the Range drop-down is a cell button at column 4 of the Value row (0x8167bc, call site
 * filters_value_band), a popup area (0x8167fb) and items with id 0x11 and the band as data (0x816958); the input
 * gives that data to set_value_band (0x8189b6). The wrap of the build adds, in the same row, the range as text in
 * column 3 where the band text goes, and the From and To drop-downs in columns 5 and 6, made as the Range one.
 * Picking a band of the original replaces the range.
 *
 * Same game (Q152): with the option off nothing is made, the input and the test go on unchanged, and the text
 * block is taken at the first Filters build with the option on or off. */
#include "win.h"
#include "options.h"

#include <stdio.h>

extern const uint32_t rt_site_filters_value_band;

#define SCREENS 0xb74450u           /* the screen stack S */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x55f990u     /* thiscall on a Display, 18 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define DISPLAY_AMOUNT 0x43f460u    /* Cash::get_display_amount, thiscall on an amount (5, 5), as 0x81683e */
#define FORMAT_MONEY 0x441c10u      /* cash_format_money, cdecl (text, size, s64): "£100K" (0x81685a) */
#define DISPLAYS 0xb74c4cu
#define PLAYER_SLOT 0xb77c7au
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu
#define TEXT_YELLOW 0xaebdecu       /* the u16 colours of the Range drop-down (0x8167a2, 0x8167c1, 0x8168e2) */
#define TEXT_BUTTON 0xaebdeau
#define BUTTON_TEXT 0xae3184u
#define POPUP_COLOUR 0xae31aau

#define VALUE_FLAG 0x8000000u       /* filter mask +0: the Value row is on Range */
#define BAND 0x23u                  /* filter block: the value band */
#define STAFF_TABLE 0xae23bcu       /* the staff table, 0x6e bytes a person */
#define STAFF_SIZE 0x6eu
#define RESULT_STAFF 0x0cu          /* a search result: the staff id (0x8075be) */
#define STAFF_VALUE 0x52u           /* staff: the value (0x807a48) */

#define FROM_ID 0x7d61u             /* ids that the screen does not use; data: the index of the amount */
#define TO_ID 0x7d62u
#define REBUILD 0xfffffff5u
#define FIRST 8                     /* the byte of the first range; the original uses -1 to 7 */

/* The amounts as shown (money scale included); To has one more, no limit. */
static const int64_t amounts[] = {0, 100000, 250000, 500000, 1000000, 2500000, 5000000, 10000000, 25000000,
                                  50000000, 100000000, 200000000};
#define AMOUNTS ((int)(sizeof amounts / sizeof amounts[0]))
#define NO_LIMIT AMOUNTS            /* To index: no upper limit */

static uint32_t scratch;            /* the texts and the amount for DISPLAY_AMOUNT */
#define TEXT_AT 0x10u
#define TEXT_SIZE 0xc8u
static int value_area = -1, value_row;

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

/* The pair (from 0..AMOUNTS-1, to from+1..NO_LIMIT) of a band byte, 1 when the byte is a range. */
static int decode(int band, int *from, int *to)
{
    int n = band - FIRST;
    if (n < 0) return 0;
    for (int f = 0; f < AMOUNTS; f++)
        for (int t = f + 1; t <= NO_LIMIT; t++)
            if (n-- == 0) {
                *from = f;
                *to = t;
                return 1;
            }
    return 0;
}

static int encode(int from, int to)
{
    int n = 0;
    for (int f = 0; f < AMOUNTS; f++)
        for (int t = f + 1; t <= NO_LIMIT; t++, n++)
            if (f == from && t == to) return FIRST + n;
    return FIRST;
}

/* The range of a filter block, or 0. */
static int range_of(uint32_t filters, int *from, int *to)
{
    return filters && (rd32(g_mem, filters) & VALUE_FLAG) && decode((uint8_t)rd8(g_mem, filters + BAND), from, to);
}

/* An amount as the band texts write it (0x816830..0x81685a); 0 as "0" (the "0 - £100K" of band 0). */
static void amount_text(int i, char *out, size_t room)
{
    if (i == 0) {
        snprintf(out, room, "0");
        return;
    }
    wr32(g_mem, scratch, (uint32_t)(int32_t)money_internal(amounts[i]));
    static const uint32_t a[2] = {5, 5};
    Cpu saved = cpu;
    cpu.ecx = scratch;
    for (int k = 1; k >= 0; k--) {
        cpu.esp -= 4;
        wr32(g_mem, cpu.esp, a[k]);
    }
    cpu.esp -= 4;
    wr32(g_mem, cpu.esp, RT_RETURN_ADDR);
    rt_call(DISPLAY_AMOUNT);
    uint32_t lo = cpu.eax, hi = cpu.edx;
    if (!rt_unwinding) cpu = saved;
    uint32_t f[4] = {scratch + TEXT_AT, TEXT_SIZE, lo, hi};
    call(FORMAT_MONEY, 4, f);
    snprintf(out, room, "%s", win_str(scratch + TEXT_AT));
}

static uint32_t text(const char *s)
{
    win_put_str(scratch + TEXT_AT, s, TEXT_SIZE);
    return scratch + TEXT_AT;
}

/* A drop-down in column c of the Value row, made as the Range one (0x8167bc, 0x8167fb, 0x816958). */
static void drop_down(int c, const char *label, uint32_t id, int first, int last, int chosen)
{
    uint32_t b[11] = {(uint32_t)value_area, (uint32_t)c, (uint32_t)value_row, 0x30, rd16(g_mem, TEXT_BUTTON), 0xc,
                      1, rd16(g_mem, BUTTON_TEXT), text(label), 0, 0};
    uint32_t button = (uint32_t)(int32_t)(int16_t)call_this(CELL_BUTTON, SCREENS, 11, b);
    uint32_t a[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0x500030, rd16(g_mem, POPUP_COLOUR), button};
    uint32_t popup = (uint32_t)(int32_t)(int16_t)call_this(AREA_CREATE, display(), 11, a);
    for (int i = first; i <= last; i++) {
        char s[64];
        if (i == NO_LIMIT) snprintf(s, sizeof s, "No limit");
        else amount_text(i, s, sizeof s);
        uint32_t w[18] = {2, 0, 0, 0, 0, 0, 0, 0x10, rd16(g_mem, POPUP_COLOUR), rd16(g_mem, TEXT_YELLOW), 1, 1,
                          i == chosen ? rd16(g_mem, TEXT_YELLOW) : rd16(g_mem, TEXT_BUTTON), text(s), 0, id,
                          (uint32_t)i, popup};
        call_this(WIDGET_CREATE, display(), 18, w);
    }
}

/* The cell button create (assistant.c): the Range drop-down gives the area and row of the Value row. */
void value_range_cell_button(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_filters_value_band) return;
    value_area = (int)(int16_t)rt_arg(0);
    value_row = (int)rt_arg(2);
}

void o_008133a0(void);

/* The Filters build: the range text and the From and To drop-downs, when the Value row is on Range. */
RT_WRAP(008133a0, filters_build)
{
    if (!scratch) scratch = rt_alloc_zero(TEXT_AT + TEXT_SIZE);    /* on and off: the same heap blocks */
    value_area = -1;
    o_008133a0();
    if (rt_unwinding || value_area < 0 || !options_bool(OPT_VALUE_RANGE)) return;
    uint32_t eax = cpu.eax, filters = node_slot(1);
    int from = 0, to = NO_LIMIT, ranged = range_of(filters, &from, &to);
    if (ranged) {
        char a[64], b[64], s[140];
        amount_text(from, a, sizeof a);
        if (to == NO_LIMIT) snprintf(s, sizeof s, "%s +", a);
        else amount_text(to, b, sizeof b), snprintf(s, sizeof s, "%s - %s", a, b);
        uint32_t l[9] = {(uint32_t)value_area, 3, (uint32_t)value_row, 1, 0, 1, 2, rd16(g_mem, TEXT_YELLOW), text(s)};
        call_this(CELL_LABEL, SCREENS, 9, l);
    }
    drop_down(5, "From", FROM_ID, 0, AMOUNTS - 1, ranged ? from : -1);
    drop_down(6, "To", TO_ID, 1, NO_LIMIT, ranged ? to : -1);
    cpu.eax = eax;
}

void o_00818100(void);

/* The Filters input, cdecl (s16 widget): From and To set the range in the screen's copy of the filters. */
RT_WRAP(00818100, filters_input)
{
    int16_t w = (int16_t)rt_arg(0);
    if (w < 0 || !options_bool(OPT_VALUE_RANGE)) {
        o_00818100();
        return;
    }
    uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
    uint32_t id = (uint16_t)rd32(g_mem, at), i = rd32(g_mem, at + 0x40), filters = node_slot(1);
    if ((id != FROM_ID && id != TO_ID) || !filters || i > NO_LIMIT) {
        o_00818100();
        return;
    }
    int from = 0, to = NO_LIMIT;
    range_of(filters, &from, &to);
    if (id == FROM_ID) {
        from = (int)i < AMOUNTS ? (int)i : AMOUNTS - 1;
        if (to <= from) to = NO_LIMIT;
    } else {
        to = (int)i > 0 ? (int)i : 1;
        if (from >= to) from = 0;
    }
    wr8(g_mem, filters + BAND, (uint32_t)encode(from, to));
    fprintf(stderr, "recomp: value range: %lld to %lld\n", (long long)amounts[from],
            to == NO_LIMIT ? -1LL : (long long)amounts[to]);
    rt_ret_cdecl(REBUILD);
}

void o_00807450(void);

/* SearchFilters::player_matches_filter, thiscall (candidate), ret 4: a match of the original must also lie in the
 * range. */
RT_WRAP(00807450, filter_match)
{
    uint32_t filters = cpu.ecx, candidate = rt_arg(0);
    o_00807450();
    int from, to;
    if (rt_unwinding || !cpu.eax || !options_bool(OPT_VALUE_RANGE) || !candidate || !range_of(filters, &from, &to))
        return;
    uint32_t staff = rd32(g_mem, STAFF_TABLE) + rd32(g_mem, candidate + RESULT_STAFF) * STAFF_SIZE;
    int64_t v = (int32_t)rd32(g_mem, staff + STAFF_VALUE);
    if (v < money_internal(amounts[from]) || (to != NO_LIMIT && v > money_internal(amounts[to]))) cpu.eax = 0;
}
