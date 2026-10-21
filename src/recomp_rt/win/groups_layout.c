/* Attribute groups (#43): the layout of the profile's attribute table (groups_layout.h). */
#include "groups_layout.h"

#include <stddef.h>
#include <string.h>

/* The groups of each column, as cells ended by -1. Inside a group the attributes are in alphabetical
 * order, the five rows of the original stay in its order after Physical (Q122, Q124). */
static const int outfield[GROUP_COLUMNS][GROUP_ROWS + 1] = {
    {CELL_HEAD_TECHNICAL, ATTR_CROSSING, ATTR_DRIBBLING, ATTR_FINISHING, ATTR_HEADING, ATTR_LONG_SHOTS,
     ATTR_MARKING, ATTR_PASSING, ATTR_SET_PIECES, ATTR_TACKLING, ATTR_TECHNIQUE, -1},
    {CELL_HEAD_MENTAL, ATTR_AGGRESSION, ATTR_ANTICIPATION, ATTR_BRAVERY, ATTR_CREATIVITY, ATTR_DECISIONS,
     ATTR_DETERMINATION, ATTR_FLAIR, ATTR_INFLUENCE, ATTR_OFF_THE_BALL, ATTR_POSITIONING, ATTR_TEAMWORK,
     ATTR_WORK_RATE, -1},
    {CELL_HEAD_PHYSICAL, ATTR_ACCELERATION, ATTR_AGILITY, ATTR_BALANCE, ATTR_JUMPING, ATTR_PACE, ATTR_STAMINA,
     ATTR_STRENGTH, CELL_FOOT, CELL_FORM, CELL_MORALE, CELL_CONDITION, CELL_ON_LOAN, -1},
};

/* A goalkeeper: Goalkeeping, then a short Technical group. Crossing, Dribbling, Finishing, Heading,
 * Long Shots, Marking and Tackling are not shown (Q129). One On Ones and Consistency come from the
 * player data, which the original does not show (Max, 2026-10-21). */
static const int keeper_first[GROUP_ROWS + 1] = {CELL_HEAD_GOALKEEPING, ATTR_HANDLING, ATTR_REFLEXES,
    CELL_ONE_ON_ONES, CELL_CONSISTENCY, ATTR_PASSING, CELL_HEAD_TECHNICAL, ATTR_SET_PIECES, ATTR_TECHNIQUE,
    -1};

const char *groups_heading(int cell)
{
    switch (cell) {
    case CELL_HEAD_GOALKEEPING: return "Goalkeeping";
    case CELL_HEAD_TECHNICAL: return "Technical";
    case CELL_HEAD_MENTAL: return "Mental";
    case CELL_HEAD_PHYSICAL: return "Physical";
    default: return NULL;
    }
}

const char *groups_hidden_name(int cell)
{
    switch (cell) {
    case CELL_ONE_ON_ONES: return "One On Ones";
    case CELL_CONSISTENCY: return "Consistency";
    default: return NULL;
    }
}

void groups_layout(int keeper, GroupLayout *out)
{
    memset(out, 0, sizeof *out);
    for (int c = 0; c < GROUP_COLUMNS; c++) {
        const int *src = keeper && c == 0 ? keeper_first : outfield[c];
        for (int r = 0; src[r] >= 0; r++) out->cell[c][r] = src[r];
    }
}
