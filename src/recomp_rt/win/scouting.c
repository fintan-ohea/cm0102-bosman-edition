/* Automatic scouting (fork of fintan-ohea; docs/specs/2026-10-24-auto-scouting-spec.md, slices 1 to 3).
 *
 * With auto_scouting = on, each human turn (0x5cde40, testrun.c):
 * - Duties (slice 1): every scout of a human manager's club whose search has no duty gets one through the game's
 *   own PLAYER_SEARCH::start_scout_assignment (0x7ae9c0): in turn the next opposition, youth players, and the 14
 *   regions. A duty the assistant gave that has run three weeks is cleared (clear_scout_assignment 0x7aee80) and
 *   the scout gets the next one; the game does not end duties on its own in that time (a run of 14 days). A duty
 *   the player gave is left alone. The game's scouting then does the work (SCOUT_MANAGER::daily_update 0x7f8750).
 * - Reports (slice 2): from the results of every scout search (`+0x1ee`, entries of 0x2d bytes with the person at
 *   `+0x0c` and the game's ability and potential stars at `+0x29`, `+0x2a`), up to REPORTS_A_DAY new reports a day,
 *   highest stars first, for players of other clubs. A report keeps each attribute as a range: the true value
 *   and SPAN more numbers around it, SPAN from the scout's Judging Player Ability (coaching data `+0x17`): 1 for 20,
 *   5 for 1. Where the range sits around the value comes from a hash of the scout, the player and the attribute,
 *   not from the game's random numbers. A report scores the player in his best role from the middle of his
 *   ranges, as the Assistant scores a player (picker.c: the key attributes of the role, times the fit, over 20),
 *   and compares that with the best two players of the squad in the role (their true attributes: the club's
 *   coaches know them). Sign, Consider or Not needed follows.
 * - The reports, the duties the assistant gave and its count go into the save as the part bosman_scouting.dat.
 *
 * The Scout reports box (slice 3) opens from the Assistant drop-down of the player's own squad screen
 * (assistant.c): the reports, Sign first, 14 a page; a click on a name opens its report.
 *
 * Same game (Q152): with the option off nothing runs. With it on the scouts work, so the game changes on purpose;
 * the reports themselves draw no random numbers of the game. The data: docs/research/2026-10-24-scouting.md. */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREENS 0xb74450u           /* the screen stack S */
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define BACKGROUND_OFF 0x41c0d0u    /* cdecl (0): as the Game Settings build (0x823a57) */
#define COLOUR 0x5e4800u            /* cdecl (0, green, 0, 0): an RGB565 colour (0x823ac0) */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define PERSON_NAME 0x53dba0u       /* cdecl (staff, buffer, size, 1): the full name (0x8766bb) */
#define CLUB_NAME 0x542100u         /* cdecl (club, team, buffer, size, mode, flag); mode 1: short (compare.c) */
#define ATTRIBUTE 0x543540u         /* cdecl (staff, player data, index 1..31): the shown value */
#define ATTRIBUTE_NAME 0x5428a0u    /* cdecl (index 1..31, text, size >= 100, 0): the long name */
#define COACH_RATING 0x544410u      /* non_player_attribute_rating, cdecl (coaching data, stored value) */

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
#define STAFF_TABLE 0xae23bcu       /* the staff table, 0x6e bytes a staff */
#define STAFF_SIZE 0x6eu
#define STAFF_AGE 0x18u             /* s16 (picker.c) */
#define STAFF_CLUB 0x39u
#define COACH_DATA 0x69u            /* staff: the coaching data */
#define PLAYER_DATA 0x61u
#define SQUAD 0xd7u                 /* club: 50 staff pointers, the first team */
#define MANAGER 0xcfu               /* club: the manager's staff record */
#define GAME_DATE 0xae2c90u         /* s16 day of the year from 0, s16 year */

#define SCOUTS 0xb74448u            /* SCOUT_MANAGER (get_search 0x7f6cd0) */
#define SEARCH_SIZE 0x2b0u
#define SEARCHES 10
#define START_DUTY 0x7ae9c0u        /* thiscall on a search (nation, region, competition, youth, opposition), ret 0x14 */
#define CLEAR_DUTY 0x7aee80u        /* thiscall on a search () */
#define GET_SCOUT 0x7ae950u         /* thiscall on a search (): the scout's staff record, 0 when he has left */
#define RESULT_SIZE 0x2du
#define REGIONS 14

/* The weekly news item (slice 4), as season_change.c makes its own (docs/research/2026-10-17-season-change.md):
 * NewsItem::NewsItem 0x77bbe0 (type, flag), set_slot 0x785790, News::send_to_club_manager 0x7861e0 (item,
 * club) on the news store. Type 0x1400 is in the range of the game's scout news (0x138e: 5000 to 5999,
 * NewsItem::get_category 0x785670); the original's types of a range go up to 0x2f. season_change.c's wraps
 * of the four NewsItem functions that read a type call scouting_news_item and scouting_news_write. */
#define NEWS_STORE 0xb7431cu
#define NEWS_ITEM_NEW 0x0077bbe0u
#define NEWS_SET_SLOT 0x00785790u
#define NEWS_SEND_CLUB 0x007861e0u
#define TEXT_CLEAR 0x004afa40u      /* Text::clear, thiscall () */
#define TEXT_APPEND 0x004ae770u     /* Text::append_string, thiscall (text) */
#define NEWS_ITEM_ROOM 0x100u
#define NEWS_TYPE 0x1400u
#define NEWS_MAGIC 0x54435342u      /* "BSCT" in slot 0 */
#define MONDAY 1

#define MAX_REPORTS 300
#define REPORTS_A_DAY 5
#define DUTY_DAYS 21
#define ATTRIBUTES 31

#define LIST_ID 0x7d49u             /* ids that the original does not use; data: the report */
#define PAGE_ID 0x7d4au             /* data: 0 back, 1 on */
#define BACK_ID 0x7d4bu
#define CLOSE_ID 0x7d4cu
#define REBUILD 0xfffffff5u
#define CLOSE 0xfffffff7u
#define TEXT_SIZE 0x200u
#define NAME_AT 0x300u
#define SCRATCH_SIZE 0x400u
#define ROWS 14

enum { ROLE_GK, ROLE_DC, ROLE_DLR, ROLE_DMC, ROLE_MC, ROLE_MLR, ROLE_AM, ROLE_FC, ROLES };
enum { REC_SIGN, REC_CONSIDER, REC_NOT };
static const char *const role_names[ROLES] = {"GK", "D C", "D L/R", "DM", "M C", "M L/R", "AM", "F C"};
static const char *const rec_names[3] = {"Sign", "Consider", "Not needed"};

/* The key attributes of each role, as the Assistant uses them (picker.c), ended by 0. */
static const uint8_t keys[ROLES][7] = {
    [ROLE_GK] = {14, 24, 3, 23, 17},
    [ROLE_DC] = {28, 19, 15, 23, 27, 17},
    [ROLE_DLR] = {28, 19, 21, 23, 8},
    [ROLE_DMC] = {28, 23, 22, 29, 31},
    [ROLE_MC] = {22, 9, 29, 31, 30},
    [ROLE_MLR] = {8, 11, 21, 22, 30},
    [ROLE_AM] = {22, 7, 9, 30, 13},
    [ROLE_FC] = {12, 20, 4, 30, 21},
};

typedef struct {
    int32_t player, scout;
    int16_t yday, year;
    uint8_t ability, potential, role, rec, judging, isnew, age, pad;
    uint16_t est, best, second, pad2;  /* role scores x 10; the record is 100 bytes, no padding of its own */
    int32_t best_player, second_player;
    uint8_t lo[ATTRIBUTES + 1], hi[ATTRIBUTES + 1];
} Report;
_Static_assert(sizeof(Report) == 100, "the save part keeps reports of 100 bytes");

typedef struct { int32_t started; uint8_t ours; } Duty;

static Report reports[MAX_REPORTS];
static int report_count, counter, news_week = -1;
static int last_day = -1, filed_today;  /* REPORTS_A_DAY counts a game day, not a human turn */
static Duty duties[16][SEARCHES];

/* ---- Game memory ---- */

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

static uint32_t staff_of(int32_t id) { return rd32(g_mem, STAFF_TABLE) + (uint32_t)id * STAFF_SIZE; }

static int today(void) { return (int16_t)rd16(g_mem, GAME_DATE + 2) * 372 + (int16_t)rd16(g_mem, GAME_DATE); }

/* Manager h's search slot (get_search 0x7f6cd0): h counts from the first human staff id. */
static uint32_t search_of(int h, int slot)
{
    return rd32(g_mem, rd32(g_mem, SCOUTS)) + (uint32_t)(h * 10 + slot) * SEARCH_SIZE;
}

static int duty_free(uint32_t s)
{
    return (int32_t)rd32(g_mem, s + 0x18c) == -1 && (int32_t)rd32(g_mem, s + 0x190) == -1 &&
           (int8_t)rd8(g_mem, s + 0x194) == -1 && (int32_t)rd32(g_mem, s + 0x195) == -1 &&
           rd32(g_mem, s + 0x199) == 0 && rd32(g_mem, s + 0x19d) == 0;
}

static int shown(uint32_t staff, int k)
{
    uint32_t a[3] = {staff, rd32(g_mem, staff + PLAYER_DATA), (uint32_t)k};
    int v = (int)(call(ATTRIBUTE, 3, a) & 0xff);
    return v < 1 ? 1 : v > 20 ? 20 : v;
}

static int judging(uint32_t scout)
{
    uint32_t c = rd32(g_mem, scout + COACH_DATA);
    if (!c) return 1;
    uint32_t a[2] = {c, rd8(g_mem, c + 0x17)};
    int v = (int)(int8_t)(call(COACH_RATING, 2, a) & 0xff);
    return v < 1 ? 1 : v > 20 ? 20 : v;
}

/* ---- Roles and scores (picker.c) ---- */

/* The fit of a player's data for a role: the lower of the position and the side rating, 1 to 20. */
static int fit(uint32_t data, int role)
{
    int pos, side = 20;
    int left = rd8(g_mem, data + 0x18), right = rd8(g_mem, data + 0x17), centre = rd8(g_mem, data + 0x19);
    int wide = left > right ? left : right;
    switch (role) {
    case ROLE_GK: return rd8(g_mem, data + 0x0f);
    case ROLE_DC: pos = rd8(g_mem, data + 0x11) > rd8(g_mem, data + 0x10) ? rd8(g_mem, data + 0x11) : rd8(g_mem, data + 0x10);
                  side = centre; break;
    case ROLE_DLR: pos = rd8(g_mem, data + 0x11) > rd8(g_mem, data + 0x16) ? rd8(g_mem, data + 0x11) : rd8(g_mem, data + 0x16);
                   side = wide; break;
    case ROLE_DMC: pos = rd8(g_mem, data + 0x12); side = centre; break;
    case ROLE_MC: pos = rd8(g_mem, data + 0x13); side = centre; break;
    case ROLE_MLR: pos = rd8(g_mem, data + 0x13); side = wide; break;
    case ROLE_AM: pos = rd8(g_mem, data + 0x14); side = wide > centre ? wide : centre; break;
    default: pos = rd8(g_mem, data + 0x15); side = centre > wide ? centre : wide; break;
    }
    return pos < side ? pos : side;
}

static int best_role(uint32_t data)
{
    int role = ROLE_FC, best = -1;
    for (int r = 0; r < ROLES; r++) {
        int f = fit(data, r);
        if (f > best) best = f, role = r;
    }
    return role;
}

/* A score x 10 from attribute values: the average of the role's keys times the fit over 20. */
static int score(const int *attr, int role, int f)
{
    int sum = 0, n = 0;
    for (const uint8_t *k = keys[role]; *k; k++) sum += attr[*k], n++;
    return n ? sum * f * 10 / (n * 20) : 0;
}

/* The best two players of the club's first team in a role (true attributes). */
static void squad_best(uint32_t club, int role, int *best, int32_t *best_id, int *second, int32_t *second_id)
{
    *best = *second = 0;
    *best_id = *second_id = -1;
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i);
        if (!staff || !rd32(g_mem, staff + PLAYER_DATA)) continue;
        int attr[ATTRIBUTES + 1];
        for (const uint8_t *k = keys[role]; *k; k++) attr[*k] = shown(staff, *k);
        int s = score(attr, role, fit(rd32(g_mem, staff + PLAYER_DATA), role));
        if (s > *best) {
            *second = *best, *second_id = *best_id;
            *best = s, *best_id = (int32_t)rd32(g_mem, staff);
        } else if (s > *second) {
            *second = s, *second_id = (int32_t)rd32(g_mem, staff);
        }
    }
}

/* ---- Reports (slice 2) ---- */

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static Report *find_report(int32_t player)
{
    for (int i = 0; i < report_count; i++) if (reports[i].player == player) return &reports[i];
    return NULL;
}

/* A slot for a new report: a free one, else the oldest. */
static Report *new_slot(void)
{
    if (report_count < MAX_REPORTS) return &reports[report_count++];
    Report *oldest = &reports[0];
    for (int i = 1; i < report_count; i++)
        if (reports[i].year * 372 + reports[i].yday < oldest->year * 372 + oldest->yday) oldest = &reports[i];
    return oldest;
}

static void write_report(uint32_t club, uint32_t staff, uint32_t scout, int ability, int potential)
{
    int32_t pid = (int32_t)rd32(g_mem, staff), sid = (int32_t)rd32(g_mem, scout);
    int j = judging(scout);
    Report *old = find_report(pid);
    if (old && old->judging >= j && today() - (old->year * 372 + old->yday) < 60) return;
    Report *r = old ? old : new_slot();
    memset(r, 0, sizeof *r);
    r->player = pid;
    r->scout = sid;
    r->yday = (int16_t)rd16(g_mem, GAME_DATE);
    r->year = (int16_t)rd16(g_mem, GAME_DATE + 2);
    r->ability = (uint8_t)ability;
    r->potential = (uint8_t)potential;
    r->judging = (uint8_t)j;
    r->isnew = 1;
    r->age = (uint8_t)(int16_t)rd16(g_mem, staff + STAFF_AGE);
    int span = 1 + (20 - j) * 4 / 19, mid[ATTRIBUTES + 1], inside = 0;
    for (int k = 1; k <= ATTRIBUTES; k++) {
        int v = shown(staff, k), below = (int)(mix((uint32_t)sid * 2654435761u ^ (uint32_t)pid * 40503u ^ (uint32_t)k) %
                                              (uint32_t)(span + 1));
        int lo = v - below, hi = lo + span;
        if (lo < 1) hi += 1 - lo, lo = 1;
        if (hi > 20) lo -= hi - 20, hi = 20;
        if (lo < 1) lo = 1;
        r->lo[k] = (uint8_t)lo;
        r->hi[k] = (uint8_t)hi;
        mid[k] = (lo + hi) / 2;
        inside += lo <= v && v <= hi;
    }
    uint32_t data = rd32(g_mem, staff + PLAYER_DATA);
    r->role = (uint8_t)best_role(data);
    r->est = (uint16_t)score(mid, r->role, fit(data, r->role));
    int best, second;
    squad_best(club, r->role, &best, &r->best_player, &second, &r->second_player);
    r->best = (uint16_t)best;
    r->second = (uint16_t)second;
    /* Sign: clearly better than the best of the squad in the role, or a young player of high potential who is
     * near the second best already. Consider: as good as the second best, or of promise. */
    int young = potential >= 4 && r->age <= 21;
    if (r->est > r->best + 5 || (young && r->est + 20 >= r->second)) r->rec = REC_SIGN;
    else if (r->est >= r->second || young || (potential >= 3 && r->age <= 23)) r->rec = REC_CONSIDER;
    else r->rec = REC_NOT;
    fprintf(stderr, "recomp: scouting report player %d by scout %d (judging %d, span %d, true value inside %d of %d): "
                    "stars %d/%d, %s est %d best %d second %d: %s\n", pid, sid, j, span, inside, ATTRIBUTES, ability,
            potential, role_names[r->role], r->est, r->best, r->second, rec_names[r->rec]);
}

/* The candidates of a manager's scout searches: entries with stars, of players of other clubs, best stars first. */
static void file_reports(int h, uint32_t club)
{
    struct { uint32_t staff, scout; int ability, potential; } c[REPORTS_A_DAY];
    int n = 0;
    if (today() != last_day) last_day = today(), filed_today = 0;
    if (filed_today >= REPORTS_A_DAY) return;
    for (int slot = 0; slot < SEARCHES; slot++) {
        uint32_t s = search_of(h, slot);
        if ((int32_t)rd32(g_mem, s + 0x180) < 0) continue;
        uint32_t scout = call_this(GET_SCOUT, s, 0, NULL);
        if (!scout) continue;
        uint32_t list = rd32(g_mem, s + 0x1ee);
        int count = (int)rd32(g_mem, s + 0x1f6);
        for (int i = 0; i < count && list; i++) {
            uint32_t e = list + (uint32_t)i * RESULT_SIZE;
            int ability = rd8(g_mem, e + 0x29), potential = rd8(g_mem, e + 0x2a);
            int32_t id = (int32_t)rd32(g_mem, e + 0x0c);
            if (!ability || id < 0 || (uint32_t)id >= rd32(g_mem, STAFF_COUNT)) continue;
            uint32_t staff = staff_of(id);
            if (!rd32(g_mem, staff + PLAYER_DATA) || rd32(g_mem, staff + STAFF_CLUB) == club) continue;
            Report *old = find_report(id);
            if (old && (old->judging >= judging(scout) || today() - (old->year * 372 + old->yday) < 60)) continue;
            /* Keep the best REPORTS_A_DAY by stars: a full list takes a candidate only in place of its last. */
            int k;
            if (n < REPORTS_A_DAY) k = n++;
            else if (ability + potential > c[n - 1].ability + c[n - 1].potential) k = n - 1;
            else continue;
            c[k].staff = staff, c[k].scout = scout, c[k].ability = ability, c[k].potential = potential;
            for (; k > 0 && c[k].ability + c[k].potential > c[k - 1].ability + c[k - 1].potential; k--) {
                __typeof__(c[0]) t = c[k]; c[k] = c[k - 1]; c[k - 1] = t;
            }
        }
    }
    for (int i = 0; i < n && filed_today < REPORTS_A_DAY; i++, filed_today++)
        write_report(club, c[i].staff, c[i].scout, c[i].ability, c[i].potential);
}

/* ---- The weekly news (slice 4) ---- */

static int weekday(void)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2), m = 0;
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    while (m < 11 && d >= days[m] + (m == 1 && leap)) d -= days[m] + (m == 1 && leap), m++;
    int yy = m < 2 ? y - 1 : y;
    return (yy + yy / 4 - yy / 100 + yy / 400 + t[m] + d + 1) % 7;
}

/* Each Monday: one item to the club's manager with the count of reports not read yet, when there are any. */
static void send_news(uint32_t club)
{
    int week = today() / 7;
    if (weekday() != MONDAY || week == news_week) return;
    news_week = week;
    int fresh = 0, sign = 0, consider = 0;
    for (int i = 0; i < report_count; i++)
        if (reports[i].isnew) fresh++, sign += reports[i].rec == REC_SIGN, consider += reports[i].rec == REC_CONSIDER;
    if (!fresh) return;
    static uint32_t item;
    if (!item) item = win_static_alloc(NEWS_ITEM_ROOM);
    uint32_t store = rd32(g_mem, NEWS_STORE);
    if (!store) return;
    uint32_t a[2] = {NEWS_TYPE, 0};
    call_this(NEWS_ITEM_NEW, item, 2, a);
    uint32_t slots[4][2] = {{0, NEWS_MAGIC}, {1, (uint32_t)fresh}, {2, (uint32_t)sign}, {3, (uint32_t)consider}};
    for (int i = 0; i < 4 && !rt_unwinding; i++) call_this(NEWS_SET_SLOT, item, 2, slots[i]);
    uint32_t b[2] = {item, club};
    if (!rt_unwinding) call_this(NEWS_SEND_CLUB, store, 2, b);
    fprintf(stderr, "recomp: scouting news: %d new reports, %d sign, %d consider\n", fresh, sign, consider);
}

int scouting_news_item(uint32_t item)
{
    return item && rd32(g_mem, item) == NEWS_TYPE && rd32(g_mem, item + 5) == NEWS_MAGIC;
}

static void text_put(uint32_t text, const char *s)
{
    static uint32_t buf;
    if (!buf) buf = win_static_alloc(0x200);
    win_put_str(buf, s, 0x200);
    uint32_t a[1] = {buf};
    call_this(TEXT_APPEND, text, 1, a);
}

int scouting_news_write(uint32_t item, uint32_t text, int full)
{
    if (!scouting_news_item(item)) return 0;
    int fresh = (int)rd32(g_mem, item + 9), sign = (int)rd32(g_mem, item + 13), consider = (int)rd32(g_mem, item + 17);
    char s[300];
    call_this(TEXT_CLEAR, text, 0, NULL);
    if (rt_unwinding) return 1;
    if (!full) snprintf(s, sizeof s, "Your scouts filed %d new report%s", fresh, fresh == 1 ? "" : "s");
    else snprintf(s, sizeof s, "Your scouts have filed %d new report%s. They advise you to sign %d of the players and "
                  "to consider %d more.\n\nThe reports are in Scout reports, in the Assistant menu of your squad "
                  "screen.", fresh, fresh == 1 ? "" : "s", sign, consider);
    text_put(text, s);
    return 1;
}

/* ---- Duties (slice 1) ---- */

static void give_duty(int h, int slot, uint32_t s)
{
    for (int tries = 0; tries < 3; tries++) {
        int k = counter++;
        uint32_t a[5] = {0xffffffffu, 0, 0xffffffffu, 0, 0};
        const char *what;
        if (k % 6 == 0) a[4] = 1, what = "next opposition";
        else if (k % 6 == 1) a[3] = 1, what = "youth";
        else a[1] = (uint32_t)(1 + k % REGIONS), what = "region";
        call_this(START_DUTY, s, 5, a);
        if (duty_free(s)) continue;
        duties[h][slot].started = today();
        duties[h][slot].ours = 1;
        fprintf(stderr, "recomp: scouting duty manager %d slot %d scout %d: %s, %.40s\n", h, slot,
                (int32_t)rd32(g_mem, s + 0x180), what, win_str(s + 0x0b));
        return;
    }
}

void scouting_turn(void)
{
    if (!options_bool(OPT_AUTO_SCOUTING) || !rd32(g_mem, SCOUTS)) return;
    uint32_t count = rd32(g_mem, STAFF_COUNT);
    for (int h = 0; h < 16 && (uint32_t)h < count; h++) {
        uint32_t staff = staff_of((int32_t)(count - 16 + (uint32_t)h)), club = rd32(g_mem, staff + STAFF_CLUB);
        if (!club || rd32(g_mem, club + MANAGER) != staff) continue;
        for (int slot = 0; slot < SEARCHES; slot++) {
            uint32_t s = search_of(h, slot);
            if ((int32_t)rd32(g_mem, s + 0x180) < 0 || rd8(g_mem, s + 0x0a) != 4) continue;
            if (!call_this(GET_SCOUT, s, 0, NULL)) continue;
            if (duty_free(s)) {
                give_duty(h, slot, s);
            } else if (duties[h][slot].ours && today() - duties[h][slot].started >= DUTY_DAYS) {
                call_this(CLEAR_DUTY, s, 0, NULL);
                give_duty(h, slot, s);
            } else if (!duties[h][slot].ours) {
                duties[h][slot].started = 0;
            }
        }
        file_reports(h, club);
        send_news(club);
    }
}

/* ---- The save part ---- */

#define PART_NAME "bosman_scouting.dat"
#define PART_HEAD 16u               /* format, length, report count, counter */

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

void scouting_saved(const char *path)
{
    if (!report_count && !counter) return;
    uint32_t size = PART_HEAD + sizeof duties + (uint32_t)report_count * sizeof(Report);
    uint8_t *part = malloc(size);
    if (!part) return;
    put32(part, 1);
    put32(part + 4, size);
    put32(part + 8, (uint32_t)report_count);
    put32(part + 12, (uint32_t)counter);
    memcpy(part + PART_HEAD, duties, sizeof duties);
    memcpy(part + PART_HEAD + sizeof duties, reports, (size_t)report_count * sizeof(Report));
    char host[2048];
    if (!win_existing_file(path, host, sizeof host) || savepart_write(host, PART_NAME, part, size))
        fprintf(stderr, "cm0102_recomp: the scout reports could not be added to the save %s.\n", path);
    free(part);
}

static uint8_t *loading;
static int loading_size;

void scouting_loading(const char *path)
{
    static uint8_t buf[PART_HEAD + sizeof duties + MAX_REPORTS * sizeof(Report)];
    char host[2048];
    loading = buf;
    loading_size = win_existing_file(path, host, sizeof host) ? savepart_read(host, PART_NAME, buf, sizeof buf) : 0;
}

void scouting_new_game(void)
{
    report_count = counter = 0;
    last_day = news_week = -1;
    memset(duties, 0, sizeof duties);
}

void scouting_loaded(int ok)
{
    if (!ok) return;
    scouting_new_game();
    if (loading_size < (int)(PART_HEAD + sizeof duties) || get32(loading) != 1) return;
    uint32_t n = get32(loading + 8);
    if (n > MAX_REPORTS || PART_HEAD + sizeof duties + n * sizeof(Report) > (uint32_t)loading_size) return;
    counter = (int)get32(loading + 12);
    memcpy(duties, loading + PART_HEAD, sizeof duties);
    memcpy(reports, loading + PART_HEAD + sizeof duties, n * sizeof(Report));
    report_count = (int)n;
    fprintf(stderr, "recomp: scouting: the save has %d reports\n", report_count);
}

/* ---- The box (slice 3) ---- */

static uint32_t scratch;
static int page, detail = -1, order[MAX_REPORTS];
static uint32_t handled_press = 0xffffffffu;

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

static int area(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights, uint32_t flags, uint32_t colour)
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

static void link(int a, int column, const char *s, uint32_t id, uint32_t data, uint32_t green)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 0xc, 2, green, text(s), id, data};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

static void wide_button(int a, int column, const char *s, uint32_t id, uint32_t data)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                      text(s), id, data};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

static void name_of(int32_t id, char *out, size_t room)
{
    if (id < 0) { snprintf(out, room, "-"); return; }
    uint32_t n[4] = {staff_of(id), scratch + NAME_AT, 0x100, 1};
    call(PERSON_NAME, 4, n);
    snprintf(out, room, "%s", win_str(scratch + NAME_AT));
}

static void club_of(int32_t id, char *out, size_t room)
{
    uint32_t club = rd32(g_mem, staff_of(id) + STAFF_CLUB);
    if (!club) { snprintf(out, room, "Free"); return; }
    uint32_t n[6] = {club, 0, scratch + NAME_AT, 0x100, 1, 1};
    call(CLUB_NAME, 6, n);
    snprintf(out, room, "%s", win_str(scratch + NAME_AT));
}

static int by_value(const void *a, const void *b)
{
    const Report *x = &reports[*(const int *)a], *y = &reports[*(const int *)b];
    if (x->rec != y->rec) return x->rec - y->rec;
    if (x->est != y->est) return y->est - x->est;
    return x->player - y->player;
}

static const uint8_t weights_list[] = {13, 30, 9, 6, 22, 9, 9, 14};
static const uint8_t weights_attr[] = {52, 20};
#define ROW_X1 160
#define ROW_X2 729

/* The stars of a report: 0 is a value the scout has not judged (the game's results), shown as "-". */
static void stars(char *out, size_t room, int a, int p)
{
    char x[4] = "-", y[4] = "-";
    if (a) snprintf(x, sizeof x, "%d", a);
    if (p) snprintf(y, sizeof y, "%d", p);
    snprintf(out, room, "%s / %s", x, y);
}

static void build_list(uint32_t light, uint32_t yellow, uint32_t green)
{
    for (int i = 0; i < report_count; i++) order[i] = i;
    qsort(order, (size_t)report_count, sizeof order[0], by_value);
    int fresh = 0;
    for (int i = 0; i < report_count; i++) fresh += reports[i].isnew;
    char s[160];
    snprintf(s, sizeof s, "%d reports, %d new. Stars: the scout's ability / potential. Score: the role's key "
             "attributes x fit.", report_count, fresh);
    box_text(ROW_X1, 98, ROW_X2, 116, 1, 0, 1, 1, light, s);
    int h = area(ROW_X1, 120, ROW_X2, 140, 8, weights_list, 1, 0);
    static const char *const heads[8] = {"Advice", "Player", "Role", "Age", "Club", "Stars", "Score", "Your best"};
    for (int c = 0; c < 8; c++) plain(h, c, heads[c], 2, light);
    int pages = (report_count + ROWS - 1) / ROWS;
    if (page >= pages) page = pages ? pages - 1 : 0;
    for (int row = 0; row < ROWS && page * ROWS + row < report_count; row++) {
        int i = order[page * ROWS + row];
        const Report *r = &reports[i];
        int a = area(ROW_X1, 144 + row * 24, ROW_X2, 166 + row * 24, 8, weights_list, 1, 0);
        if (r->rec == REC_SIGN) value_text(a, 0, rec_names[r->rec]);
        else plain(a, 0, rec_names[r->rec], 2, r->rec == REC_CONSIDER ? yellow : light);
        char name[64], club[64];
        name_of(r->player, name, sizeof name);
        link(a, 1, name, LIST_ID, (uint32_t)i, green);
        plain(a, 2, role_names[r->role], 2, light);
        snprintf(s, sizeof s, "%d", r->age);
        plain(a, 3, s, 2, light);
        club_of(r->player, club, sizeof club);
        plain(a, 4, club, 2, light);
        stars(s, sizeof s, r->ability, r->potential);
        plain(a, 5, s, 2, light);
        snprintf(s, sizeof s, "%d.%d", r->est / 10, r->est % 10);
        plain(a, 6, s, 2, light);
        snprintf(s, sizeof s, "%d.%d", r->best / 10, r->best % 10);
        plain(a, 7, s, 2, light);
    }
    if (!report_count) box_text(ROW_X1, 150, ROW_X2, 172, 1, 0, 1, 2, light,
                                "No reports yet: your scouts report as they find players.");
    snprintf(s, sizeof s, "Page %d of %d", pages ? page + 1 : 0, pages);
    box_text(ROW_X1, 482, 440, 502, 1, 0, 1, 2, light, s);
    int p = area(450, 480, ROW_X2, 504, 2, NULL, 1, 0);
    if (page > 0) link(p, 0, "Previous page", PAGE_ID, 0, green);
    if (page + 1 < pages) link(p, 1, "Next page", PAGE_ID, 1, green);
}

static void build_detail(uint32_t light, uint32_t yellow)
{
    const Report *r = &reports[detail];
    char s[200], name[64], club[64], scout[64], best[64], second[64];
    name_of(r->player, name, sizeof name);
    club_of(r->player, club, sizeof club);
    name_of(r->scout, scout, sizeof scout);
    name_of(r->best_player, best, sizeof best);
    name_of(r->second_player, second, sizeof second);
    snprintf(s, sizeof s, "%s, %s, age %d, %s", name, club, r->age, role_names[r->role]);
    box_text(ROW_X1, 96, ROW_X2, 116, 1, 0, 0xc, 3, yellow, s);
    {   /* The date as day.month.year from the day of the year. */
        static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int d = r->yday, m = 0, y = r->year, leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        while (m < 11 && d >= days[m] + (m == 1 && leap)) d -= days[m] + (m == 1 && leap), m++;
            char st[16];
        stars(st, sizeof st, r->ability, r->potential);
        snprintf(s, sizeof s, "Scout %s (judging %d) on %d.%d.%02d. Stars (ability / potential): %s. Advice: %s.", scout,
                 r->judging, d + 1, m + 1, y % 100, st, rec_names[r->rec]);
    }
    box_text(ROW_X1, 118, ROW_X2, 136, 1, 0, 1, 1, light, s);
    snprintf(s, sizeof s, "Score as %s: %d.%d. Your best: %s %d.%d, then %s %d.%d.", role_names[r->role], r->est / 10,
             r->est % 10, best, r->best / 10, r->best % 10, second, r->second / 10, r->second % 10);
    box_text(ROW_X1, 136, ROW_X2, 154, 1, 0, 1, 1, light, s);
    for (int k = 1; k <= ATTRIBUTES; k++) {
        int col = (k - 1) / 11, row = (k - 1) % 11, x = ROW_X1 + col * 190, y = 162 + row * 26;
        int a = area(x, y, x + 184, y + 22, 2, weights_attr, 1, 0);
        uint32_t na[4] = {(uint32_t)k, scratch + NAME_AT, 100, 0};
        call(ATTRIBUTE_NAME, 4, na);
        char label[100];
        snprintf(label, sizeof label, "%s", win_str(scratch + NAME_AT));
        int key = 0;
        for (const uint8_t *q = keys[r->role]; *q; q++) key |= *q == k;
        plain(a, 0, label, 2, key ? yellow : light);
        if (r->lo[k] == r->hi[k]) snprintf(s, sizeof s, "%d", r->lo[k]);
        else snprintf(s, sizeof s, "%d-%d", r->lo[k], r->hi[k]);
        value_text(a, 1, s);
    }
    box_text(ROW_X1, 452, ROW_X2, 470, 1, 0, 1, 1, light,
             "Yellow names: the role's key attributes. The true value is inside each range; a better judge narrows it.");
}

static void build(void)
{
    area(0, 0, 0x31f, 0x257, 1, NULL, 2, 0);
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t g[4] = {0, 0xc0, 0, 0};
    uint32_t green = call(COLOUR, 4, g) & 0xffff;
    uint32_t light = colour16(TEXT_LIGHT), yellow = colour16(TEXT_YELLOW);
    area(145, 50, 744, 549, 1, NULL, 0x30, box_colour);
    box_text(155, 58, 734, 92, 0x30, colour16(TEXT_TITLE), 0xc, 4, colour16(BUTTON_TEXT), "Scout reports");
    if (detail >= 0 && detail < report_count) build_detail(light, yellow);
    else build_list(light, yellow, green);
    int b = area(155, 509, 734, 539, 2, NULL, 1, 0);
    if (detail >= 0) wide_button(b, 0, "Back to the list", BACK_ID, 0);
    wide_button(b, 1, "Close", CLOSE_ID, 0);
    rt_ret_cdecl(0);
}

static void input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    uint32_t r = 0;
    if (w >= 0 && win_press_count() == handled_press) w = -1;
    if (w >= 0) {
        handled_press = win_press_count();
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at), data = rd32(g_mem, at + 0x40);
        if (id == CLOSE_ID) {
            for (int i = 0; i < report_count; i++) reports[i].isnew = 0;
            r = CLOSE;
        } else if (id == LIST_ID && data < (uint32_t)report_count) {
            detail = (int)data;
            r = REBUILD;
        } else if (id == BACK_ID) {
            detail = -1;
            r = REBUILD;
        } else if (id == PAGE_ID) {
            page += data ? 1 : -1;
            if (page < 0) page = 0;
            r = REBUILD;
        }
    }
    rt_ret_cdecl(r);
}

int scouting_open(void)
{
    uint32_t p[5] = {rt_host_fn("scouting_build", build), rt_host_fn("scouting_input", input), 1, 0, 0};
    if (!call_this(PUSH, SCREENS, 5, p)) {
        fprintf(stderr, "recomp: scout reports not opened\n");
        return 0;
    }
    if (!scratch) scratch = rt_alloc_zero(SCRATCH_SIZE);
    page = 0;
    detail = -1;
    handled_press = win_press_count();
    fprintf(stderr, "recomp: scout reports opened: %d reports\n", report_count);
    return 1;
}
