/* The original's "Compare two chosen players" screen with the polygon (#22, spec
 * 2026-10-15-features-0.4.0, slice 6; Q116). Facts: docs/research/2026-10-16-comparison-screen-inventory.md.
 *
 * The player chooses the two players with the prompts "Set as 1st/2nd player in comparison?" of the
 * profile's Action drop-down. The game keeps them per human manager in the scout manager [0xb74448]
 * (getter 0x7f7920). The side bar item 0x434 and the profile item "Compare two chosen players" push the
 * screen with the build 0x876520 and the input 0x889c30.
 *
 * With squad_views on, this wrap builds the screen in place of the original: the player's picture, a
 * heading with the two names, the side bar, the comparison of compare.c (variant B) and the Back bar. The
 * original's input stays: the screen has no widget with one of its ids, so a click goes to the side bar,
 * Back or a name link (0x7615d0, 0x7643d0), as on the original screen.
 *
 * The original's table calls the training manager 0x8b1750(player 1, 0), which writes training data and
 * draws random numbers. The wrap makes the same call in the same cases, so the game goes on the same way.
 * Check of 2026-10-16 (a count of the calls of the random function 0x90cfc0 during the build): one call
 * with the original build and one with this build.
 * In the cases where the original shows no table ("Need two players to compare." or no player data), the
 * original builds the screen. */
#include "win.h"
#include "views.h"
#include "options.h"

#include <stdio.h>
#include <string.h>

#define SCREENS 0xb74450u           /* the screen stack S */
#define SCOUTS 0xb74448u            /* the scout manager */
#define PLAYER_OF 0x7f7920u         /* thiscall on the scout manager (1 or 2): the staff record or 0 */
#define TRAINING 0xde1f44u          /* the training manager */
#define TRAIN 0x8b1750u             /* thiscall on the training manager (staff, 0), ret 8 */
#define PICTURE 0x41c790u           /* cdecl (staff, 0, 0): the staff's picture as the screen's picture */
#define HEADING 0x803f30u           /* thiscall on S (bar colour, text colour, text) */
#define SIDE_BAR 0x75db00u          /* cdecl (mode, arg) */
#define BACK_BAR 0x5edcc0u          /* cdecl (back, next) */
#define CANNOT_GO_BACK 0x7fec60u    /* thiscall on S (any): 0 when there is a screen to go back to */
#define AT_END 0x7febb0u            /* thiscall on S: 1 when no screen comes after the current one */
#define FULL_NAME 0x53dba0u         /* cdecl (staff, text, size, 1): the name as the original's heading */
#define RED 0xae31a8u               /* u16: the original's heading bar and player 2 colour (0x87678c) */
#define GREY 0xae3184u              /* u16: the original's heading text (0x876819) */
#define STAFF_COUNT 0xae2364u       /* the staff count; the 16 human managers are its last rows */
#define PLAYER_DATA 0x61u
#define JOB 0x3du

#define TEXT_SIZE 0x200u

void o_00876520(void);

static uint32_t text;               /* runtime memory, not the game's heap */

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

static uint32_t player(uint32_t n)
{
    uint32_t a[1] = {n};
    return call_this(PLAYER_OF, rd32(g_mem, SCOUTS), 1, a);
}

/* 1 when the original shows its attribute table (0x8790b5..0x8790f4, 0x8799c8): two different players,
 * player 1 not a human manager and with player data. The wrap also wants player data for player 2. */
static int has_table(uint32_t p1, uint32_t p2)
{
    if (!p1 || !p2 || p1 == p2) return 0;
    if (rd8(g_mem, p1 + JOB) == 2) return 0;
    int32_t id = (int32_t)rd32(g_mem, p1), count = (int32_t)rd32(g_mem, STAFF_COUNT);
    if (id >= count - 16 && id < count) return 0;
    return rd32(g_mem, p1 + PLAYER_DATA) && rd32(g_mem, p2 + PLAYER_DATA);
}

/* The build of the comparison screen: cdecl, no arguments. */
RT_WRAP(00876520, compare_screen_build)
{
    uint32_t p1 = player(1), p2 = player(2);
    if (!options_bool(OPT_SQUAD_VIEWS) || !has_table(p1, p2)) {
        compare_clear();            /* the old polygon must not draw on the original's screen */
        o_00876520();
        return;
    }
    if (!text) text = win_static_alloc(TEXT_SIZE);
    uint32_t t[2] = {p1, 0};
    call_this(TRAIN, rd32(g_mem, TRAINING), 2, t);

    uint32_t pic[3] = {p1, 0, 0};
    call(PICTURE, 3, pic);
    char names[2][0x100];
    for (int i = 0; i < 2; i++) {
        wr8(g_mem, text, 0);
        uint32_t a[4] = {i ? p2 : p1, text, 0xc8, 1};
        call(FULL_NAME, 4, a);
        snprintf(names[i], sizeof names[i], "%s", win_str(text));
    }
    char heading[0x200];
    /* The original's heading, with shorter names when the two do not fit the bar (Tousled12, 2026-10-21). */
    compare_heading(text, p1, p2, names[0], names[1], 100, 790, heading, sizeof heading);
    win_put_str(text, heading, TEXT_SIZE);
    uint32_t h[3] = {rd16(g_mem, RED), rd16(g_mem, GREY), text};
    call_this(HEADING, SCREENS, 3, h);
    uint32_t s[2] = {4, 0};
    call(SIDE_BAR, 2, s);

    compare_build(text, p1, p2, 100, 80, 790, 545, 0, 0, 0);

    uint32_t back_arg[1] = {0};
    uint32_t back = call_this(CANNOT_GO_BACK, SCREENS, 1, back_arg) == 0;
    uint32_t next = call_this(AT_END, SCREENS, 0, NULL) == 0;
    uint32_t bar[2] = {back, next};
    call(BACK_BAR, 2, bar);
    fprintf(stderr, "recomp: comparison screen built\n");
    rt_ret_cdecl(0);
}
