/* The score of a player in a slot, without game memory, so that tests/picker can test it (picker.c,
 * best_xi.c).
 *
 * A value is the sum of the key attributes the viewer sees, times the fit, times 60, divided by their
 * count: the score shown is value / 1200 (Q138 to Q140, Q156). An attribute that the game hides from the
 * viewer is 0 and does not count (Q111). With every key attribute hidden, the player has no score: the value
 * is PICK_UNSCORED, the screens show "-" in place of the number, and he ranks after every player with a
 * score (a score of 0 included). */
#ifndef RECOMP_PICK_SCORE_H
#define RECOMP_PICK_SCORE_H

#include <stdint.h>

#define PICK_UNSCORED (-2)

/* The value from attr[k] (0 hidden) of the keys (attribute numbers, ended by 0) at a fit. */
static inline int pick_value(const uint8_t *attr, const uint8_t *keys, int fit)
{
    int sum = 0, n = 0;
    for (const uint8_t *k = keys; *k; k++)
        if (attr[*k]) { sum += attr[*k]; n++; }
    return n ? sum * fit * 60 / n : PICK_UNSCORED;
}

/* The rank of a value for ordering: -1 for a player who cannot take the slot (value -1), 0 for no score,
 * the value + 1 otherwise. */
static inline int pick_rank(int v) { return v == PICK_UNSCORED ? 0 : v < 0 ? -1 : v + 1; }

/* The number on the screen for a value: 1 to 20 (rounded), or 0 for no score ("-"). */
static inline int pick_shown(int v)
{
    if (v == PICK_UNSCORED) return 0;
    int s = (v + 600) / 1200;
    return s < 1 ? 1 : s > 20 ? 20 : s;
}

#endif
