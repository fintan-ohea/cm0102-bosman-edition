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
 * Ability (hidden_tab = on, which shows hidden values): the Attributes page of a player search (page 2, 0x813bec; not the staff search,
 * list type 3, 0x813c18) lays out the 31 attributes in a grid of two columns of 16 rows (0x813f9d..), so the right
 * column's last row is free. The wrap of the build puts a row of its own there, Ability and Potential with - and +,
 * the least current and potential ability a player must have, in steps of 10 (0 for none). They are kept in the
 * last bytes of the filter block, which no code of the search, the session or the filters reads (+0x80..+0x98 in
 * 0x7ae000..0x7b4000, 0x804000..0x808500, 0x8133a0 and 0x818100): a mark at +0x96 and the two minimums at +0x97
 * and +0x98. Clear (SearchFilters::reset_attribute_filters 0x8065d0) clears them too. The test cannot sit in
 * player_matches_filter: PLAYER_SEARCH::get_result_list 0x7af160 does not call it while the filter mask is the
 * default one (0x7af3f0..0x7af40a), and the minimums do not change the mask. The loop leaves out an entry whose byte
 * +0x20 has bit 0 (0x7af3ea), so in a call of 0x7af160 the wrap of purge_invalid_results 0x7b1520 (its last change
 * of the entries before the loop, 0x7af3c5) sets that bit on the players below the minimums, and the wrap of 0x7af160
 * puts the bits back after the call. These are hidden values
 * (glossary): the fork's decision of 2026-10-30 (docs/specs/2026-10-30-hidden-tab-spec.md).
 *
 * Same game (Q152): with the options off nothing is made, the input and the test go on unchanged, and the text
 * block is taken at the first Filters build with the options on or off. */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define STAFF_VALUE 0x52u
#define RESULT_SIZE 0x2du           /* a search result entry (0x7af452) */
#define SEARCH_FILTERS 0x1fau       /* PLAYER_SEARCH: its filter block (0x7af3f6) */
#define SEARCH_COUNT 0x1f6u         /* PLAYER_SEARCH: the count of entries (0x7af3ca) */
#define SEARCH_TABLE 0x1eeu         /* PLAYER_SEARCH: the entries (0x7af3e0) */
#define RESULT_FLAGS 0x20u          /* bit 0: left out of the list (0x7af3ea) */           /* staff: the value (0x807a48) */

#define PAGE_SLOT 2                 /* the Filters screen: 2 the Attributes page (0x813be8) */
#define ATTRIBUTES_PAGE 2
#define SEARCHES 1u                 /* the session's ten PLAYER_SEARCH records, 0x2b0 bytes (0x813bf4..0x813c0d) */
#define SEARCH_SIZE 0x2b0u
#define SEARCH_TYPE 0x0au
#define STAFF_SEARCH 3              /* the list type of Staff Search: the non-player attributes (0x813c18) */
#define ABILITY_MARK 0x96u          /* filter block: 0xca when the two bytes after it are ability minimums */
#define ABILITY_MIN 0x97u
#define POTENTIAL_MIN 0x98u
#define MARK 0xcau
#define PLAYER_DATA 0x61u           /* staff: the player data, 0 for no player */
#define ABILITY 0x05u               /* player data: s16 current ability */
#define POTENTIAL 0x07u             /* player data: s16 potential ability */
#define ABILITY_STEP 10
#define ABILITY_MAX 200
#define ABILITY_ID 0x7d63u          /* data: 0 Ability -, 1 Ability +, 2 Potential -, 3 Potential + */
#define BUTTON_FACE 0xae3150u       /* the u16 colours of an attribute's - and + (0x814077) */
#define TEXT_LIGHT 0xae3154u
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

/* The minimums of a filter block: 1 when it has them. */
static int ability_of(uint32_t filters, int *ca, int *pa)
{
    *ca = *pa = 0;
    if (!filters || rd8(g_mem, filters + ABILITY_MARK) != MARK) return 0;
    *ca = (uint8_t)rd8(g_mem, filters + ABILITY_MIN);
    *pa = (uint8_t)rd8(g_mem, filters + POTENTIAL_MIN);
    return *ca || *pa;
}

/* The Attributes page of a player search: the session's current search is not a staff search. */
static int player_attributes_page(void)
{
    uint32_t session = node_slot(0);
    if (!session || (int)node_slot(PAGE_SLOT) != ATTRIBUTES_PAGE) return 0;
    uint32_t search = session + SEARCHES + (uint32_t)(int8_t)rd8(g_mem, session) * SEARCH_SIZE;
    return rd8(g_mem, search + SEARCH_TYPE) != STAFF_SEARCH;
}

/* The row in the free place of the grid: the right column's last row (classic pixels, the row of Work Rate). */
static const uint8_t weights_ability[] = {20, 9, 10, 9, 3, 24, 9, 10, 9};
static void ability_row(uint32_t filters)
{
    int ca, pa;
    ability_of(filters, &ca, &pa);
    uint32_t a[11] = {462, 465, 756, 484, sizeof weights_ability, scratch + TEXT_AT + TEXT_SIZE, 1, 0, 1, 0,
                      0xffffffffu};
    memcpy(g_mem + scratch + TEXT_AT + TEXT_SIZE, weights_ability, sizeof weights_ability);
    uint32_t area = (uint32_t)(int32_t)(int16_t)call_this(AREA_CREATE, display(), 11, a);
    for (int k = 0; k < 2; k++) {
        int c = k * 5, v = k ? pa : ca;
        char s[16];
        uint32_t l[9] = {area, (uint32_t)c, 0, 1, 0, 1, 2, rd16(g_mem, v ? TEXT_YELLOW : TEXT_LIGHT),
                         text(k ? "Potential" : "Ability")};
        call_this(CELL_LABEL, SCREENS, 9, l);
        for (int up = 0; up < 2; up++) {
            uint32_t b[11] = {area, (uint32_t)(c + 1 + up * 2), 0, 0x30, rd16(g_mem, BUTTON_FACE), 0xc, 1,
                              rd16(g_mem, BUTTON_TEXT), text(up ? "+" : "-"), ABILITY_ID, (uint32_t)(k * 2 + up)};
            call_this(CELL_BUTTON, SCREENS, 11, b);
        }
        if (v) snprintf(s, sizeof s, "%d", v);
        else snprintf(s, sizeof s, "-");
        uint32_t t[9] = {area, (uint32_t)(c + 2), 0, 1, 0, 1, 2, rd16(g_mem, TEXT_YELLOW), text(s)};
        call_this(CELL_LABEL, SCREENS, 9, t);
    }
}

void o_008133a0(void);

/* The Filters build: the range text and the From and To drop-downs, when the Value row is on Range. */
RT_WRAP(008133a0, filters_build)
{
    if (!scratch) scratch = rt_alloc_zero(TEXT_AT + TEXT_SIZE + 16);    /* on and off: the same heap blocks */
    value_area = -1;
    o_008133a0();
    if (rt_unwinding) return;
    uint32_t eax = cpu.eax, filters = node_slot(1);
    if (options_bool(OPT_HIDDEN_TAB) && filters && player_attributes_page()) ability_row(filters);
    cpu.eax = eax;
    if (value_area < 0 || !options_bool(OPT_VALUE_RANGE)) return;
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
    if (w < 0 || (!options_bool(OPT_VALUE_RANGE) && !options_bool(OPT_HIDDEN_TAB))) {
        o_00818100();
        return;
    }
    uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
    uint32_t id = (uint16_t)rd32(g_mem, at), i = rd32(g_mem, at + 0x40), filters = node_slot(1);
    if (id == ABILITY_ID && filters && i < 4 && options_bool(OPT_HIDDEN_TAB)) {
        int ca, pa;
        ability_of(filters, &ca, &pa);
        int *v = i < 2 ? &ca : &pa;
        *v += i % 2 ? ABILITY_STEP : -ABILITY_STEP;
        if (*v < 0) *v = 0;
        if (*v > ABILITY_MAX) *v = ABILITY_MAX;
        wr8(g_mem, filters + ABILITY_MARK, MARK);
        wr8(g_mem, filters + ABILITY_MIN, (uint32_t)ca);
        wr8(g_mem, filters + POTENTIAL_MIN, (uint32_t)pa);
        fprintf(stderr, "recomp: ability filter: ability %d potential %d\n", ca, pa);
        rt_ret_cdecl(REBUILD);
        return;
    }
    if ((id != FROM_ID && id != TO_ID) || !filters || i > NO_LIMIT || !options_bool(OPT_VALUE_RANGE)) {
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
 * value range. */
RT_WRAP(00807450, filter_match)
{
    uint32_t filters = cpu.ecx, candidate = rt_arg(0);
    o_00807450();
    if (rt_unwinding || !cpu.eax || !candidate) return;
    uint32_t staff = rd32(g_mem, STAFF_TABLE) + rd32(g_mem, candidate + RESULT_STAFF) * STAFF_SIZE;
    int from, to;
    if (options_bool(OPT_VALUE_RANGE) && range_of(filters, &from, &to)) {
        int64_t v = (int32_t)rd32(g_mem, staff + STAFF_VALUE);
        if (v < money_internal(amounts[from]) || (to != NO_LIMIT && v > money_internal(amounts[to]))) cpu.eax = 0;
    }
}

static uint32_t pending;            /* the PLAYER_SEARCH whose get_result_list runs with ability minimums */
static int pending_ca, pending_pa, marked_count, marked_room;
static uint32_t *marked;

void o_007b1520(void);

/* PLAYER_SEARCH::purge_invalid_results, thiscall (relink), ret 4: the last change of the entries before the loop of
 * get_result_list (0x7af3c5). In that call, the players below the minimums get bit 0 of +0x20. */
RT_WRAP(007b1520, purge_results)
{
    uint32_t search = cpu.ecx;
    o_007b1520();
    if (rt_unwinding || search != pending) return;
    int n = (int32_t)rd32(g_mem, search + SEARCH_COUNT);
    uint32_t table = rd32(g_mem, search + SEARCH_TABLE), staff_table = rd32(g_mem, STAFF_TABLE);
    if (n > marked_room) {
        uint32_t *grown = realloc(marked, (size_t)n * sizeof *marked);
        if (!grown) return;
        marked = grown;
        marked_room = n;
    }
    for (int i = 0; i < n; i++) {
        uint32_t e = table + (uint32_t)i * RESULT_SIZE;
        if (rd8(g_mem, e + RESULT_FLAGS) & 1) continue;
        uint32_t data = rd32(g_mem, staff_table + rd32(g_mem, e + RESULT_STAFF) * STAFF_SIZE + PLAYER_DATA);
        if (data && (int16_t)rd16(g_mem, data + ABILITY) >= pending_ca &&
            (int16_t)rd16(g_mem, data + POTENTIAL) >= pending_pa)
            continue;
        wr8(g_mem, e + RESULT_FLAGS, rd8(g_mem, e + RESULT_FLAGS) | 1);
        marked[marked_count++] = e;
    }
}

void o_007af160(void);

/* PLAYER_SEARCH::get_result_list, thiscall (LLIST *out, int *other, compare), ret 0xc: the players below the
 * ability minimums of the search's filters (+0x1fa, 0x7af3f6) are left out of this list, and their bits go back
 * after it. The entries: count at +0x1f6, table at +0x1ee, 0x2d bytes each (0x7af3ca..0x7af3e8). */
RT_WRAP(007af160, result_list)
{
    uint32_t search = cpu.ecx;
    if (!options_bool(OPT_HIDDEN_TAB) || !ability_of(search + SEARCH_FILTERS, &pending_ca, &pending_pa)) {
        o_007af160();
        return;
    }
    pending = search;
    marked_count = 0;
    o_007af160();
    pending = 0;
    for (int i = 0; i < marked_count; i++)
        wr8(g_mem, marked[i] + RESULT_FLAGS, rd8(g_mem, marked[i] + RESULT_FLAGS) & ~1u);
    marked_count = 0;
}

void o_008065d0(void);

/* SearchFilters::reset_attribute_filters, thiscall: Clear of the Attributes page clears the minimums too. */
RT_WRAP(008065d0, reset_attributes)
{
    uint32_t filters = cpu.ecx;
    o_008065d0();
    if (rt_unwinding || !options_bool(OPT_HIDDEN_TAB) || rd8(g_mem, filters + ABILITY_MARK) != MARK) return;
    wr8(g_mem, filters + ABILITY_MIN, 0);
    wr8(g_mem, filters + POTENTIAL_MIN, 0);
}
