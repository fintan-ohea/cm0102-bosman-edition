/* The option full_player_search (0.5.0, #57).
 *
 * The player search list builder 0x7afa70 adds a player only when the human manager knows him: the score of
 * 0x5407e0 is 4000 or more (jge at 0x7afc4b), or a scout report exists, or he is from the same region
 * and one of 0x544520, 0x544590 and 0x76d9b0 is true (docs/research/2026-10-16-crash-fixes.md, 1c).
 * Changing the jge to a jmp lists every player. Here every original call still runs, so that the
 * game state is the same with the option on and off. tools/recomp/widescreen.py makes the three places that
 * reject a player ask rt_full_player_search instead. With the option off, the original's path runs.
 *
 * The limit (forum report of 2026-10-21). The screen counts the players it shows in 16 bits: 0x7af160 adds
 * each player that passes the filters to a list with the word count inc at 0x68d191, and the found text of
 * 0x808690 (0x80b6f6) reads that word as signed. Above 32767 players the word is negative and the screen
 * says "1 player found (N filtered out)" over an empty list. Two places add players after the extra ones:
 * the players the manager knows that come later in the staff order (the original's accept path), and
 * 0x7af870 (scout lists, 0x7affe2) and 0x7af720 (0x7af31b, at most 50). So:
 *   - extra players (the ones the original rejects) join only while the list holds fewer than LIST_MAX;
 *   - a known player that comes when the list is full takes the place of the last extra player
 *     (rt_full_player_search_slot), so the list keeps every player that the option off lists;
 *   - LIST_MAX leaves room for the later adds, and rt_full_player_search_shown caps the players that
 *     0x7af160 passes to the screen list at 32767 as a last guard.
 * All of this runs only with the option on. */
#include "options.h"
#include "../rt.h"

#define LIST_COUNT 0x1f6        /* [search screen + 0x1f6]: the players in the list */
#define LIST_MAX 30000          /* the extra players stop here: room for the later adds */
#define SHOWN_MAX 32767         /* the screen keeps its count in 16 bits (inc word at 0x68d191) */

static uint32_t extras[LIST_MAX];   /* the list rows that hold extra players, in the order they joined */
static uint32_t n_extras;
static int pending;                 /* the player under test joins as an extra player */
static int replaced;                /* the row of the player under test replaces an extra player */

/* 1 when a player that the original rejects joins the list: the option is on and the list has room. */
uint32_t rt_full_player_search(uint32_t screen)
{
    if (!options_bool(OPT_FULL_PLAYER_SEARCH)) return 0u;
    if (rd32(g_mem, screen + LIST_COUNT) >= LIST_MAX) return 0u;
    pending = 1;
    return 1u;
}

/* The builder empties the list (0x7afbb3). */
void rt_full_player_search_reset(void)
{
    n_extras = 0;
    pending = 0;
    replaced = 0;
}

/* The player under test does not join (0x7afe6b, the end of the loop body). */
void rt_full_player_search_skip(void)
{
    pending = 0;
}

/* The row for the player that joins (0x7afdda reads the count here). */
uint32_t rt_full_player_search_slot(uint32_t screen)
{
    uint32_t count = rd32(g_mem, screen + LIST_COUNT);
    replaced = 0;
    if (pending) {
        pending = 0;
        if (n_extras < LIST_MAX) extras[n_extras++] = count;
        return count;
    }
    if (n_extras && count >= LIST_MAX && options_bool(OPT_FULL_PLAYER_SEARCH)) {
        replaced = 1;
        return extras[--n_extras];
    }
    return count;
}

/* 1 when the row of the player replaced an extra player: the count stays (0x7afe5e). */
uint32_t rt_full_player_search_replaced(void)
{
    uint32_t r = (uint32_t)replaced;
    replaced = 0;
    return r;
}

/* The players that 0x7af160 adds to the screen list (ebx at 0x7af467). */
uint32_t rt_full_player_search_shown(uint32_t passed)
{
    if (passed > SHOWN_MAX && options_bool(OPT_FULL_PLAYER_SEARCH)) return SHOWN_MAX;
    return passed;
}
