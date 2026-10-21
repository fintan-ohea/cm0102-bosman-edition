/* Shared parts of the Team views screen (views.c), its Best XI tab (best_xi.c) and the tactic list
 * (tactics.c). */
#ifndef RECOMP_VIEWS_H
#define RECOMP_VIEWS_H

#include <stddef.h>
#include <stdint.h>

#define VIEWS_TEXT_SIZE 0x80u       /* the text buffer that views.c gives best_xi_build */
#define VIEWS_ROW_PX 17             /* a text row, screen pixels */
#define VIEWS_FORMATION_X1 620      /* the Formation button: (620, 15)-(785, 35) */
#define VIEWS_TACTICS 64            /* the most tactics in the Formation list */

/* A tactic of the Formation list: its name as the game's Tactics drop-down shows it and the position
 * mask of each of its 11 slots. */
typedef struct {
    char name[64];
    uint16_t masks[11];
} Tactic;

/* The tactics of the manager's Tactics drop-down, the current tactic first. Returns the count. */
int tactics_list(uint32_t manager, Tactic *out, int max);

/* Tab 1: the Formation button and a card for each slot of the chosen formation on the pitch x1..y2 (classic
 * numbers, the pitch on its side). Returns the number of tactics in the list, 0 when there is none. */
int best_xi_build(uint32_t club, uint32_t team, uint32_t buf, int x1, int y1, int x2, int y2, uint32_t bar);

/* A click on a widget of id and data: 1 when it was a Formation item (rebuild the screen). */
int best_xi_input(uint32_t id, uint32_t data);

/* The heading "Name ~ Name" of the comparison screen made to fit the heading box x1..x2 (800x600 numbers).
 * text: a buffer of VIEWS_TEXT_SIZE bytes in game memory. */
void compare_heading(uint32_t text, uint32_t staff1, uint32_t staff2, const char *full1, const char *full2, int x1,
                     int x2, char *out, size_t size);

/* The comparison of two players (compare.c): variant B in the box x1..y2 (classic numbers). club not 0
 * makes each name box a drop-down with the club's squad (the Compare tab), at page page1 and page2. text: VIEWS_TEXT_SIZE bytes of
 * the game's memory. */
void compare_build(uint32_t text, uint32_t staff1, uint32_t staff2, int x1, int y1, int x2, int y2, uint32_t club,
                   int page1, int page2);
/* A click on a drop-down item: the side (1 or 2) with its new player in *value, or a page item of side 1
 * or 2 (3 or 4) with the page in *value, or 0. Each drop-down shows its squad in pages that fit the screen. */
int compare_input(uint32_t club, uint32_t id, uint32_t data, uint32_t *value);
/* The first two players of the club's squad, 0 when there are none. */
void compare_first_two(uint32_t club, uint32_t *a, uint32_t *b);
/* Forgets the comparison built last: call it when a screen or tab is built that is not a comparison. */
void compare_clear(void);
/* After the game draws a widget: the polygon and the frames of the comparison built last. */
void compare_widget_drawn(uint32_t widget);
/* A widget of a Best XI card whose stars the runtime draws after it: halves 0 to 10 (Q420). */
void views_stars(int widget, int halves);

/* The position text of a slot's mask: GK, SW, then D, DM, M, AM or F with L, C or R. */
void views_position_text(uint16_t mask, char *out, size_t size);

#endif
