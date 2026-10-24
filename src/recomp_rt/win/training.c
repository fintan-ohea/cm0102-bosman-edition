/* The training assistant (fork of fintan-ohea; docs/specs/2026-10-24-training-assistant-spec.md, slices 1 to 4).
 *
 * With training_assistant = on, the Assistant drop-down of the player's own squad screen ends with "Training
 * setup" (assistant.c), which opens this box for the club. The box is a screen of the runtime on the game's
 * screen stack, built as the Options screen (options_screen.c): the frame of Game Settings (0x823950), pushed
 * with flag 1, with three tabs.
 *
 * Schedules (slices 1 and 4): two schedules, Outfield and Keepers, with their levels, their workload by the
 * game's own formula (TRAINING_SCHEDULE::get_training_load 0x8b4f60) and the first team players who follow each.
 * Apply writes them into the manager's two "Unused" schedule slots (8 and 9, type 7) and puts each first team
 * player on one of them by position. A slot that holds a schedule of the player's own is left alone. The switch
 * Weekly cycle (option training_cycle) makes Outfield train Intensive in four categories from Sunday to Thursday
 * and Medium on Friday and Saturday (training_turn, each human turn). Like a change on the Training screen, a
 * change of levels does not bring players up to date first (TRAINING_EDIT_SESSION::apply_changes 0x8b0b40).
 *
 * Explain (slice 2): the manager's nine schedules with levels, workload and players, and the attributes that each
 * category trains (TRAINING_MANAGER::apply_training_progress 0x8b2be0).
 *
 * Coaches (slice 3): each coach's rating in each category by the game's formulas (update_coach_assignments
 * 0x8b2880), the game's coaching factor now (get_coaching_factor 0x8b41d0) and that of a suggested assignment.
 * The suggestion tries every assignment with up to two categories per coach and keeps the one that gives the most
 * first team player-categories a training quality above 100, where a player can improve (0x8b1ef2), then at
 * least 110, then the highest factors weighted by players, then the fewest assignments. Apply writes it
 * into the coach table and lets the game work out the contributions (update_coach_assignments).
 *
 * The box works on the training manager directly and not from the Training screen, because that screen edits a
 * copy that its Ok writes back (apply_changes 0x8b0b40). The data: docs/research/2026-10-24-training.md.
 *
 * Same game (Q152): the box makes its scratch block when it opens, after a click of the player. The item's text
 * is made with the other Assistant texts whatever the option (assistant.c). The cycle changes only a schedule
 * named Outfield of type 8 in slot 8, which only the box makes. */
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
#define PERSON_NAME 0x53dba0u       /* cdecl (staff, buffer, size, 1): the full name (0x8766bb) */

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
#define STAFF_CLUB 0x39u            /* staff: the club */
#define COACH_DATA 0x69u            /* staff: the non-player (coaching) data, 0 for none */
#define SQUAD 0xd7u                 /* club: 50 staff pointers, the first team */
#define MANAGER 0xcfu               /* club: the manager's staff record */
#define PLAYER_DATA 0x61u           /* staff: the player data, 0 for no player */
#define GOALKEEPER 0x0fu            /* player data: the goalkeeper position rating, 1 to 20 */
#define NATURAL 15                  /* the game's "natural" rating (0x5b7d20, picker.c) */
#define GAME_DATE 0xae2c90u         /* s16 day of the year from 0, s16 year */

#define TRAINING 0xde1f44u          /* the training manager */
#define PLAYER_ENTRIES 0x651du      /* manager + this: 6 bytes per player id, byte 0 the schedule slot */
#define COACH_TABLES 0x652du        /* manager + this + h * 54: six records of 9 bytes (0x8b4cd0) */
#define COACHES 6
#define COACH_RECORD 9
#define SET_PLAYER_SCHEDULE 0x8b2250u  /* thiscall on the manager (staff, slot), ret 8 */
#define UPDATE_COACHES 0x8b2880u    /* thiscall on the manager (club, manager or 0), ret 8 */
#define COACHING_FACTOR 0x8b41d0u   /* thiscall on the manager (club, category), ret 8 */
#define COACH_RATING 0x544410u      /* non_player_attribute_rating, cdecl (coaching data, stored value) */
#define SET_LEVEL 0x8b4f30u         /* TRAINING_SCHEDULE::set_category_level, thiscall (category, level), ret 8 */
#define GET_LOAD 0x8b4f60u          /* TRAINING_SCHEDULE::get_training_load, thiscall (): the workload */
#define SCHEDULE_SIZE 0x9bu
#define SCHEDULE_NAME 0x09u         /* 11 characters and the end (0x8b51f4) */
#define SCHEDULE_TYPE 0x15u         /* 7 an unused slot, 8 a schedule of the manager's own */
#define TYPE_UNUSED 7
#define TYPE_OWN 8
#define CATEGORIES 5                /* Fitness, Tactics, Shooting, Skills, Goalkeeping: +0x04 to +0x08 */
#define SLOTS 10                    /* a manager's schedules; slot 0 is the Training screen's Overview */

#define APPLY_ID 0x7d44u            /* ids that the original does not use */
#define CLOSE_ID 0x7d45u
#define TAB_ID 0x7d46u              /* data: the tab, 1 to 3 */
#define CYCLE_ID 0x7d47u            /* data: 1 on, 0 off */
#define SUGGEST_ID 0x7d48u
#define REBUILD 0xfffffff5u         /* -0xb: build the box again */
#define CLOSE 0xfffffff7u           /* -9: close the box, as Ok and Cancel of Game Settings (0x8259a0) */
#define TEXT_SIZE 0x200u
#define NAME_AT 0x300u              /* scratch + this: a person's name */
#define SCRATCH_SIZE 0x400u

enum { TAB_SCHEDULES = 1, TAB_EXPLAIN, TAB_COACHES };

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
/* The weekly cycle (slice 4, Shephard 4.2 in the forum topic): Intensive or Medium in the four outfield
 * categories. */
static const uint8_t cycle_intensive[CATEGORIES] = {50, 50, 50, 50, 0};
static const uint8_t cycle_medium[CATEGORIES] = {25, 25, 25, 25, 0};

static const char *const category_names[CATEGORIES] = {"Fitness", "Tactics", "Shooting", "Skills", "Keeping"};
/* What each category trains (research note, 0x8b2be0): the attributes a screen shows, and the count of bytes
 * no screen shows. */
static const char *const category_trains[CATEGORIES] = {
    "Acceleration, Agility, Jumping, Pace, Reflexes, Stamina, Strength (and 1 hidden)",
    "Decisions, Marking, Off The Ball, Positioning, Teamwork",
    "Finishing, Long Shots (and 1 hidden)",
    "Crossing, Dribbling, Set Pieces, Heading, Passing, Tackling, Technique (and 2 hidden)",
    "Handling (and 1 hidden)",
};
static const char *const day_names[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                                         "Saturday"};

/* The divisors of update_coach_assignments (one coach in n categories, 0x8b2892) and get_coaching_factor (n
 * coaches in one category, 0x8b41e0): floats in the original. */
static const float per_coach[6] = {1.0f, 1.0f, 1.2f, 1.4f, 1.6f, 2.0f};
static const float per_category[7] = {1.0f, 1.0f, 1.45f, 1.7f, 1.9f, 2.05f, 2.2f};

static uint32_t scratch, club, manager;
static int human;                   /* the manager's place among the 16 human staff */
static int tab = TAB_SCHEDULES;
static char result[160];            /* what the last Apply did */
static uint32_t handled_press = 0xffffffffu;

/* The coaches tab: the table's coaches, their ratings and the suggestion. */
static int coach_count;
static int32_t coach_id[COACHES];
static int rating[COACHES][CATEGORIES];
static uint8_t suggestion[COACHES];  /* a category mask per coach */
static int suggested_factor[CATEGORIES], coaches_ready;
static int level_count[CATEGORIES][3];  /* first team players training each category Light, Medium, Intensive */
static int above_now[2], above_suggested[2];  /* player-categories with quality above 100 and at least 110 */

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

/* Manager h's schedule slot (TRAINING_MANAGER::get_manager_schedule 0x8b3930). */
static uint32_t schedule_of(int h, int slot)
{
    return rd32(g_mem, TRAINING) + (uint32_t)(h * 10 + slot + 7) * SCHEDULE_SIZE;
}

static uint32_t schedule(int slot) { return schedule_of(human, slot); }

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

/* The assistant's own schedule of plan p in manager h's slot: type 8 with its name. */
static int is_ours(int h, const Plan *p)
{
    uint32_t s = schedule_of(h, p->slot);
    return (int8_t)rd8(g_mem, s + SCHEDULE_TYPE) == TYPE_OWN && !strncmp(win_str(s + SCHEDULE_NAME), p->name, 11);
}

/* A slot the assistant may write: unused, or already one of its own schedules. */
static int slot_free(const Plan *p)
{
    return (int8_t)rd8(g_mem, schedule(p->slot) + SCHEDULE_TYPE) == TYPE_UNUSED || is_ours(human, p);
}

static int is_keeper(uint32_t staff)
{
    return (int)rd8(g_mem, rd32(g_mem, staff + PLAYER_DATA) + GOALKEEPER) >= NATURAL;
}

/* The slot of a player in the training manager's entries. */
static int slot_of(uint32_t staff)
{
    uint32_t entries = rd32(g_mem, rd32(g_mem, TRAINING) + PLAYER_ENTRIES);
    return (int)rd8(g_mem, entries + rd32(g_mem, rd32(g_mem, staff + PLAYER_DATA)) * 6);
}

/* The first team players on slot. */
static int players_on(int slot)
{
    int n = 0;
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i);
        if (staff && rd32(g_mem, staff + PLAYER_DATA) && slot_of(staff) == slot) n++;
    }
    return n;
}

static void set_levels(uint32_t s, const uint8_t *level)
{
    for (int c = 0; c < CATEGORIES; c++) {
        if (rd8(g_mem, s + 4 + (uint32_t)c) == level[c]) continue;
        uint32_t a[2] = {(uint32_t)c, level[c]};
        call_this(SET_LEVEL, s, 2, a);
    }
}

/* ---- The weekly cycle (slice 4) ---- */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* The day of the week of the game date, 0 Sunday (Sakamoto's method). */
static int weekday(void)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2), m = 0;
    while (m < 11 && d >= days[m] + (m == 1 && leap(y))) d -= days[m] + (m == 1 && leap(y)), m++;
    int yy = m < 2 ? y - 1 : y;
    return (yy + yy / 4 - yy / 100 + yy / 400 + t[m] + d + 1) % 7;
}

static const uint8_t *cycle_levels(int day) { return day <= 4 ? cycle_intensive : cycle_medium; }

/* Sets manager h's Outfield schedule for today, or back to its plan when the cycle is off. */
static void cycle_set(int h, int on)
{
    if (!is_ours(h, &plans[0])) return;
    uint32_t s = schedule_of(h, plans[0].slot);
    int day = weekday();
    const uint8_t *level = on ? cycle_levels(day) : plans[0].level;
    if (!memcmp(g_mem + s + 4, level, CATEGORIES)) return;
    set_levels(s, level);
    fprintf(stderr, "recomp: training cycle: manager %d %s, Outfield %s\n", h, day_names[day],
            on ? (day <= 4 ? "Intensive" : "Medium") : "back to its plan");
}

void training_turn(void)
{
    if (!options_bool(OPT_TRAINING_CYCLE) || !rd32(g_mem, TRAINING)) return;
    uint32_t count = rd32(g_mem, STAFF_COUNT), table = rd32(g_mem, STAFF_TABLE);
    for (int h = 0; h < 16 && (uint32_t)h < count; h++) {
        uint32_t staff = table + (count - 1 - (uint32_t)h) * STAFF_SIZE;
        uint32_t c = rd32(g_mem, staff + STAFF_CLUB);
        if (c && rd32(g_mem, c + MANAGER) == staff) cycle_set(h, 1);
    }
}

static void set_cycle(int on)
{
    if (options_bool(OPT_TRAINING_CYCLE) == on) return;
    int ok = options_set(OPT_TRAINING_CYCLE, on ? "on" : "off");
    fprintf(stderr, "recomp: option training_cycle = %s (%d)\n", on ? "on" : "off", ok);
    cycle_set(human, on);
}

/* ---- Slice 1: Apply ---- */

static void write_schedule(const Plan *p)
{
    uint32_t s = schedule(p->slot);
    wr8(g_mem, s + SCHEDULE_TYPE, TYPE_OWN);
    win_put_str(s + SCHEDULE_NAME, p->name, 12);
    set_levels(s, p->level);
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
    if (written[0] && options_bool(OPT_TRAINING_CYCLE)) cycle_set(human, 1);
}

/* ---- Slice 3: coaches ---- */

static uint32_t coach_table(void) { return rd32(g_mem, TRAINING) + COACH_TABLES + (uint32_t)human * 54u; }

static uint32_t staff_of(int32_t id) { return rd32(g_mem, STAFF_TABLE) + (uint32_t)id * STAFF_SIZE; }

static int coach_value(uint32_t data, uint32_t at)
{
    uint32_t a[2] = {data, rd8(g_mem, data + at)};
    return (int)(int8_t)(call(COACH_RATING, 2, a) & 0xff);
}

/* A coach's rating in each category before the divisor for several categories (0x8b2aac..0x8b2b5b). */
static void coach_ratings(uint32_t data, int *r)
{
    int coaching = coach_value(data, 0x10), technique = (int8_t)rd8(g_mem, data + 0x12);
    r[0] = (coaching - technique + 21) / 2;
    r[1] = (coach_value(data, 0x21) + coaching + technique) / 3;
    r[2] = (coaching + technique + (int8_t)rd8(g_mem, data + 0x0e)) / 3;
    r[3] = (coaching + technique) / 2;
    r[4] = coach_value(data, 0x11);
}

/* A coach's contribution in one of n categories (0x8b2b5e..0x8b2b83). */
static int contribution(int r, int n)
{
    int v = (int)((double)r / (double)per_coach[n]);
    return v < 1 ? 1 : v > 20 ? 20 : v;
}

/* The coaching factor of a category from its contributions (0x8b4270..0x8b42a8). */
static int factor(int sum, int coaches)
{
    if (coaches) sum = (int)((double)sum / (double)per_category[coaches]);
    int v = (int)((double)sum * 5.714285714285714);
    return v < 50 ? 50 : v;
}

static int bits(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

/* The factors of an assignment (a category mask per coach). */
static void factors_of(const uint8_t *mask, int *out)
{
    for (int k = 0; k < CATEGORIES; k++) {
        int sum = 0, n = 0;
        for (int c = 0; c < coach_count; c++) {
            if (!(mask[c] & (1 << k))) continue;
            sum += contribution(rating[c][k], bits(mask[c]));
            n++;
        }
        out[k] = factor(sum, n);
    }
}

static const int level_values[3] = {10, 25, 50};

/* The first team players training each category at each level (their schedule's level now), and from them the
 * weight of each category (at least 1). */
static void count_levels(int *w)
{
    memset(level_count, 0, sizeof level_count);
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i);
        if (!staff || !rd32(g_mem, staff + PLAYER_DATA)) continue;
        int slot = slot_of(staff);
        if (slot < 1 || slot >= SLOTS) continue;
        for (int k = 0; k < CATEGORIES; k++) {
            int v = rd8(g_mem, schedule(slot) + 4 + (uint32_t)k);
            for (int l = 0; l < 3; l++) if (v == level_values[l]) level_count[k][l]++;
        }
    }
    for (int k = 0; k < CATEGORIES; k++) w[k] = level_count[k][0] + level_count[k][1] + level_count[k][2] ?
        level_count[k][0] + level_count[k][1] + level_count[k][2] : 1;
}

/* Training quality of a level with a coaching factor (update_player_training 0x8b1da1..0x8b1e0b). */
static int quality(int level, int f)
{
    int q = 2 * level + 60 + (f - 100) / 5;
    return q < 0 ? 0 : q > 200 ? 200 : q;
}

/* The player-categories with quality above 100 (the positive branch, 0x8b1ef2) and at least 110 (out of the
 * negative branch, 0x8b1f36) with the factors f. */
static void count_above(const int *f, int *above)
{
    above[0] = above[1] = 0;
    for (int k = 0; k < CATEGORIES; k++)
        for (int l = 0; l < 3; l++) {
            int q = quality(level_values[l], f[k]);
            if (q > 100) above[0] += level_count[k][l];
            if (q >= 110) above[1] += level_count[k][l];
        }
}

/* The best assignment with up to two categories per coach: every combination; the most player-categories with
 * quality above 100, then at least 110, then the highest sum of factors weighted by players, then the fewest
 * assignments. */
static void suggest(void)
{
    uint8_t masks[16];
    int nm = 0;
    masks[nm++] = 0;
    for (int a = 0; a < CATEGORIES; a++) {
        masks[nm++] = (uint8_t)(1 << a);
        for (int b = a + 1; b < CATEGORIES; b++) masks[nm++] = (uint8_t)((1 << a) | (1 << b));
    }
    int w[CATEGORIES], idx[COACHES] = {0}, f[CATEGORIES], above[2], best_above[2] = {-1, -1};
    count_levels(w);
    long best = -1;
    int best_count = 99;
    uint8_t m[COACHES] = {0};
    memset(suggestion, 0, sizeof suggestion);
    for (;;) {
        for (int c = 0; c < coach_count; c++) m[c] = masks[idx[c]];
        factors_of(m, f);
        long score = 0;
        int count = 0;
        for (int k = 0; k < CATEGORIES; k++) score += (long)w[k] * f[k];
        for (int c = 0; c < coach_count; c++) count += bits(m[c]);
        count_above(f, above);
        int better = above[0] != best_above[0] ? above[0] > best_above[0]
                   : above[1] != best_above[1] ? above[1] > best_above[1]
                   : score != best ? score > best : count < best_count;
        if (better) {
            memcpy(best_above, above, sizeof best_above);
            best = score;
            best_count = count;
            memcpy(suggestion, m, sizeof suggestion);
        }
        int c = 0;
        while (c < coach_count && ++idx[c] == nm) idx[c++] = 0;
        if (c == coach_count) break;
    }
    factors_of(suggestion, suggested_factor);
    count_above(suggested_factor, above_suggested);
}

static int game_factor(int k)
{
    uint32_t a[2] = {club, (uint32_t)k};
    return (int16_t)call_this(COACHING_FACTOR, rd32(g_mem, TRAINING), 2, a);
}

/* The mask of a coach in the table now. */
static uint8_t current_mask(int32_t id)
{
    uint32_t t = coach_table();
    for (int i = 0; i < COACHES; i++) {
        uint32_t r = t + (uint32_t)i * COACH_RECORD;
        if ((int32_t)rd32(g_mem, r) != id) continue;
        uint8_t m = 0;
        for (int k = 0; k < CATEGORIES; k++) if (rd8(g_mem, r + 4 + (uint32_t)k)) m |= (uint8_t)(1 << k);
        return m;
    }
    return 0;
}

static void load_coaches(void)
{
    uint32_t a[2] = {club, 0};
    call_this(UPDATE_COACHES, rd32(g_mem, TRAINING), 2, a);  /* the game's own refresh of the table */
    coach_count = 0;
    uint32_t t = coach_table();
    for (int i = 0; i < COACHES; i++) {
        int32_t id = (int32_t)rd32(g_mem, t + (uint32_t)i * COACH_RECORD);
        uint32_t data = id >= 0 ? rd32(g_mem, staff_of(id) + COACH_DATA) : 0;
        if (!data) continue;
        coach_id[coach_count] = id;
        coach_ratings(data, rating[coach_count]);
        coach_count++;
    }
    /* A check of the formulas: the factors of the table now, worked out here and by the game. */
    uint8_t now[COACHES];
    int f[CATEGORIES];
    for (int c = 0; c < coach_count; c++) now[c] = current_mask(coach_id[c]);
    factors_of(now, f);
    fprintf(stderr, "recomp: training assistant coaches %d, factors now", coach_count);
    for (int k = 0; k < CATEGORIES; k++) fprintf(stderr, " %d/%d", f[k], game_factor(k));
    fprintf(stderr, " (worked out/game)\n");
    suggest();
    for (int k = 0; k < CATEGORIES; k++) f[k] = game_factor(k);
    count_above(f, above_now);
    fprintf(stderr, "recomp: training assistant above 100: now %d, suggested %d; at least 110: now %d, suggested %d\n",
            above_now[0], above_suggested[0], above_now[1], above_suggested[1]);
    coaches_ready = 1;
}

static void apply_suggestion(void)
{
    uint32_t t = coach_table();
    for (int i = 0; i < COACHES; i++) {
        uint32_t r = t + (uint32_t)i * COACH_RECORD;
        int32_t id = (int32_t)rd32(g_mem, r);
        for (int c = 0; c < coach_count; c++) {
            if (coach_id[c] != id) continue;
            for (int k = 0; k < CATEGORIES; k++) wr8(g_mem, r + 4 + (uint32_t)k, (suggestion[c] >> k) & 1u);
        }
    }
    uint32_t a[2] = {club, 0};
    call_this(UPDATE_COACHES, rd32(g_mem, TRAINING), 2, a);  /* the contributions, as the game works them out */
    fprintf(stderr, "recomp: training assistant coaches applied: factors");
    for (int k = 0; k < CATEGORIES; k++) fprintf(stderr, " %d/%d", suggested_factor[k], game_factor(k));
    fprintf(stderr, " (suggested/game)\n");
    snprintf(result, sizeof result, "Coaches assigned as suggested.");
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

static void choice(int a, int column, const char *s, uint32_t id, uint32_t data, uint32_t green)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 0xc, 2, green, text(s), id, data};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

static void wide_button(int a, int column, const char *s, uint32_t id)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                      text(s), id, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

/* A schedule row: name, the five levels, workload, players. */
static const uint8_t weights_row[] = {22, 13, 13, 13, 13, 13, 12, 11};
/* A coach row: name, the five categories. */
static const uint8_t weights_coach[] = {34, 13, 13, 13, 13, 13};
/* The cycle row: name, On, Off, a gap. */
static const uint8_t weights_switch[] = {38, 17, 17, 48};
#define ROW_X1 160
#define ROW_X2 729

static void schedule_row(int y, const char *name, const uint8_t *level, int load, int players, uint32_t light)
{
    int r = area(ROW_X1, y, ROW_X2, y + 22, 8, weights_row, 1, 0);
    plain(r, 0, name, 2, light);
    for (int k = 0; k < CATEGORIES; k++) value_text(r, k + 1, level_name(level[k]));
    char s[16];
    snprintf(s, sizeof s, "%d", load);
    value_text(r, 6, s);
    snprintf(s, sizeof s, "%d", players);
    value_text(r, 7, s);
}

static void header_row(int y, const char *first, int with_counts, uint32_t light)
{
    int h = area(ROW_X1, y, ROW_X2, y + 20, with_counts ? 8 : 6, with_counts ? weights_row : weights_coach, 1, 0);
    plain(h, 0, first, 2, light);
    for (int k = 0; k < CATEGORIES; k++) plain(h, k + 1, category_names[k], 2, light);
    if (with_counts) {
        plain(h, 6, "Workload", 2, light);
        plain(h, 7, "Players", 2, light);
    }
}

static void build_schedules(uint32_t light, uint32_t yellow, uint32_t green)
{
    box_text(ROW_X1, 132, ROW_X2, 150, 1, 0, 1, 1, light,
             "Two schedules for your first team, with the workload by the game's own formula.");
    header_row(156, "Schedule", 1, light);
    for (int p = 0; p < 2; p++)
        schedule_row(180 + p * 26, plans[p].name, plans[p].level, workload(plans[p].level), players_on(plans[p].slot),
                     light);
    int y = 238;
    for (int p = 0; p < 2; p++, y += 20) {
        char s[120];
        uint32_t sc = schedule(plans[p].slot);
        int type = (int8_t)rd8(g_mem, sc + SCHEDULE_TYPE);
        if (type == TYPE_UNUSED) snprintf(s, sizeof s, "Slot %d is unused: Apply sets up %s there.", plans[p].slot,
                                          plans[p].name);
        else if (slot_free(&plans[p])) snprintf(s, sizeof s, "Slot %d holds %s: Apply sets it again.", plans[p].slot,
                                                plans[p].name);
        else snprintf(s, sizeof s, "Slot %d holds your schedule %.11s: Apply leaves it and its players.",
                      plans[p].slot, win_str(sc + SCHEDULE_NAME));
        box_text(ROW_X1, y, ROW_X2, y + 18, 1, 0, 1, 1, light, s);
    }
    box_text(ROW_X1, y, ROW_X2, y + 18, 1, 0, 1, 1, light,
             "Goalkeepers (position rating 15 or more) go on Keepers, every other player on Outfield.");

    /* The weekly cycle (slice 4). */
    int on = options_bool(OPT_TRAINING_CYCLE);
    int c = area(ROW_X1, 312, ROW_X2, 338, 4, weights_switch, 1, 0);
    plain(c, 0, "Weekly cycle", 3, light);
    if (on) {
        value_text(c, 1, "On");
        choice(c, 2, "Off", CYCLE_ID, 0, green);
    } else {
        choice(c, 1, "On", CYCLE_ID, 1, green);
        value_text(c, 2, "Off");
    }
    int day = weekday();
    char s[120];
    snprintf(s, sizeof s, "Today is %s: %s", day_names[day],
             on ? (is_ours(human, &plans[0]) ? (day <= 4 ? "Outfield trains Intensive." : "Outfield trains Medium.")
                                            : "the cycle starts once Outfield is set up.")
                : "the cycle is off.");
    box_text(ROW_X1, 342, ROW_X2, 360, 1, 0, 1, 1, light, s);
    box_text(ROW_X1, 360, ROW_X2, 378, 1, 0, 1, 1, light,
             "On: Outfield trains Intensive in Fitness, Tactics, Shooting and Skills from Sunday to Thursday,");
    box_text(ROW_X1, 378, ROW_X2, 396, 1, 0, 1, 1, light,
             "and Medium on Friday and Saturday, every week. Keepers stay as they are. Off: Outfield as above.");
    snprintf(s, sizeof s, "Workload: Intensive %d, Medium %d (at most 100).", workload(cycle_intensive),
             workload(cycle_medium));
    box_text(ROW_X1, 396, ROW_X2, 414, 1, 0, 1, 1, light, s);
    if (result[0]) box_text(ROW_X1, 440, ROW_X2, 462, 1, 0, 1, 3, yellow, result);
}

static void build_explain(uint32_t light)
{
    header_row(132, "Your schedule", 1, light);
    for (int slot = 1; slot < SLOTS; slot++) {
        uint32_t s = schedule(slot);
        char name[16];
        snprintf(name, sizeof name, "%.11s", win_str(s + SCHEDULE_NAME));
        schedule_row(132 + 22 * slot, name, g_mem + s + 4, (int16_t)call_this(GET_LOAD, s, 0, NULL), players_on(slot),
                     light);
    }
    int y = 358;
    for (int k = 0; k < CATEGORIES; k++, y += 18) {
        char s[160];
        snprintf(s, sizeof s, "%s: %s", category_names[k], category_trains[k]);
        box_text(ROW_X1, y, ROW_X2, y + 18, 1, 0, 1, 1, light, s);
    }
    box_text(ROW_X1, y + 4, ROW_X2, y + 22, 1, 0, 1, 1, light,
             "Quality = 60 + 2 x level + (coaching - 100) / 5, with None 0, Light 10, Medium 25, Intensive 50.");
    box_text(ROW_X1, y + 22, ROW_X2, y + 40, 1, 0, 1, 1, light,
             "A player improves only where quality is above 100. Injured players train nothing.");
}

static void build_coaches(uint32_t light, uint32_t yellow)
{
    if (!coaches_ready) load_coaches();
    header_row(132, "Coach (rating)", 0, light);
    for (int c = 0; c < coach_count; c++) {
        int r = area(ROW_X1, 156 + c * 24, ROW_X2, 178 + c * 24, 6, weights_coach, 1, 0);
        uint32_t n[4] = {staff_of(coach_id[c]), scratch + NAME_AT, 0x100, 1};
        call(PERSON_NAME, 4, n);
        char name[64];
        snprintf(name, sizeof name, "%.40s", win_str(scratch + NAME_AT));
        plain(r, 0, name, 2, light);
        uint8_t now = current_mask(coach_id[c]);
        for (int k = 0; k < CATEGORIES; k++) {
            char s[8];
            snprintf(s, sizeof s, "%d", rating[c][k]);
            if (now & (1 << k)) value_text(r, k + 1, s);
            else plain(r, k + 1, s, 2, light);
        }
    }
    if (!coach_count) box_text(ROW_X1, 156, ROW_X2, 178, 1, 0, 1, 2, light, "Your club has no coaches.");
    int y = 156 + COACHES * 24 + 6;
    int f = area(ROW_X1, y, ROW_X2, y + 22, 6, weights_coach, 1, 0);
    plain(f, 0, "Coaching now", 2, light);
    for (int k = 0; k < CATEGORIES; k++) {
        char s[8];
        snprintf(s, sizeof s, "%d", game_factor(k));
        value_text(f, k + 1, s);
    }
    f = area(ROW_X1, y + 26, ROW_X2, y + 48, 6, weights_coach, 1, 0);
    plain(f, 0, "Suggested", 2, light);
    for (int k = 0; k < CATEGORIES; k++) {
        char s[8];
        snprintf(s, sizeof s, "%d", suggested_factor[k]);
        value_text(f, k + 1, s);
    }
    int ty = y + 56;
    for (int c = 0; c < coach_count && ty < 452; c++, ty += 16) {
        char s[160], cats[80] = "";
        for (int k = 0; k < CATEGORIES; k++)
            if (suggestion[c] & (1 << k)) {
                if (cats[0]) strncat(cats, ", ", sizeof cats - strlen(cats) - 1);
                strncat(cats, category_names[k], sizeof cats - strlen(cats) - 1);
            }
        uint32_t n[4] = {staff_of(coach_id[c]), scratch + NAME_AT, 0x100, 1};
        call(PERSON_NAME, 4, n);
        snprintf(s, sizeof s, "Suggested: %.40s on %s", win_str(scratch + NAME_AT), cats[0] ? cats : "nothing");
        box_text(ROW_X1, ty, ROW_X2, ty + 16, 1, 0, 1, 1, light, s);
    }
    char line[160];
    snprintf(line, sizeof line, "Player-categories where a player can improve (quality above 100): now %d, suggested %d.",
             above_now[0], above_suggested[0]);
    box_text(ROW_X1, 452, ROW_X2, 470, 1, 0, 1, 1, yellow, line);
    box_text(ROW_X1, 470, ROW_X2, 486, 1, 0, 1, 1, light,
             "Yellow: the categories a coach takes now. Coaching is the game's factor in each category.");
    if (result[0]) box_text(ROW_X1, 488, ROW_X2, 506, 1, 0, 1, 1, yellow, result);
}

static void build(void)
{
    area(0, 0, 0x31f, 0x257, 1, NULL, 2, 0);                    /* the dark cover, 0x823a92 */
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t g[4] = {0, 0xc0, 0, 0};
    uint32_t green = call(COLOUR, 4, g) & 0xffff;               /* the value buttons, 0x823979 */
    uint32_t light = colour16(TEXT_LIGHT), yellow = colour16(TEXT_YELLOW);
    area(145, 50, 744, 549, 1, NULL, 0x30, box_colour);
    box_text(155, 58, 734, 92, 0x30, colour16(TEXT_TITLE), 0xc, 4, colour16(BUTTON_TEXT), "Training assistant");

    int t = area(165, 100, 724, 124, 3, NULL, 1, 0);
    static const char *const tab_names[3] = {"Schedules", "Explain", "Coaches"};
    for (int i = 1; i <= 3; i++) {
        if (i == tab) value_text(t, i - 1, tab_names[i - 1]);
        else choice(t, i - 1, tab_names[i - 1], TAB_ID, (uint32_t)i, green);
    }
    if (tab == TAB_SCHEDULES) build_schedules(light, yellow, green);
    else if (tab == TAB_EXPLAIN) build_explain(light);
    else build_coaches(light, yellow);

    int b = area(155, 509, 734, 539, 2, NULL, 1, 0);
    if (tab == TAB_SCHEDULES) wide_button(b, 0, "Apply", APPLY_ID);
    if (tab == TAB_COACHES && coach_count) wide_button(b, 0, "Apply suggestion", SUGGEST_ID);
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
        uint32_t id = (uint16_t)rd32(g_mem, at), data = rd32(g_mem, at + 0x40);
        if (id == CLOSE_ID) {
            r = CLOSE;
        } else if (id == APPLY_ID) {
            apply();
            r = REBUILD;
        } else if (id == TAB_ID && data >= 1 && data <= 3) {
            tab = (int)data;
            result[0] = 0;
            r = REBUILD;
        } else if (id == CYCLE_ID) {
            set_cycle(data == 1);
            r = REBUILD;
        } else if (id == SUGGEST_ID) {
            apply_suggestion();
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
    if (!scratch) scratch = rt_alloc_zero(SCRATCH_SIZE);
    club = the_club;
    manager = m;
    human = h;
    tab = TAB_SCHEDULES;
    result[0] = 0;
    coaches_ready = 0;
    handled_press = win_press_count();              /* the press that opened the box does not click in it */
    for (int s = 1; s < SLOTS; s++) {
        uint32_t sc = schedule(s);
        fprintf(stderr, "recomp: training assistant slot %d type %d levels %d %d %d %d %d name %.11s\n", s,
                (int8_t)rd8(g_mem, sc + SCHEDULE_TYPE), rd8(g_mem, sc + 4), rd8(g_mem, sc + 5), rd8(g_mem, sc + 6),
                rd8(g_mem, sc + 7), rd8(g_mem, sc + 8), win_str(sc + SCHEDULE_NAME));
    }
    fprintf(stderr, "recomp: training assistant players on slot 8 %d, slot 9 %d\n", players_on(8), players_on(9));
    return 1;
}
