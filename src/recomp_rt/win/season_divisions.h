/* New divisions (0.6.0 slice 20, spec docs/specs/2026-10-16-seasons-0.6.0-spec.md, Q290, Q291, Q331, Q363).
 * Research and evidence: docs/research/2026-10-20-new-divisions.md.
 *
 * A new division that replaces a division of the original lives in the object of that division (Q363: it takes
 * the extra division flag of the division it replaces). Only a season game (season_game_changes) gets any of
 * it. The base game and RECOMP_SEASON_CODE=off keep the original. RECOMP_NEW_DIVISIONS=off turns it off in a
 * season game (a test switch).
 *
 * Germany (20a): from 2008/09 the German Regional object (GerRegional, made only with the extra division flag,
 * create_german_comps 0x8343f0 at 0x8344d5) holds the 3. Liga: one table of 20 clubs, no South group. */
#ifndef RECOMP_SEASON_DIVISIONS_H
#define RECOMP_SEASON_DIVISIONS_H

#include <stdint.h>

/* The club count that GerRegional::create_table stores (0x5dd801, 0x5dd809; rt.h declares it for the
 * translated code). */
uint32_t rt_ger_regional_clubs(uint32_t obj, uint32_t original);

/* After the set_up_rules of a league (season_sizes_rules): the places towards a new division. */
void season_divisions_rules(uint32_t obj);
/* After League::load: a 3. Liga object gets its cleared stage slot again (season_load.c, load sweep). */
void season_divisions_loaded(uint32_t obj);

/* Club moves at a new game (season_clubs.c). The count of a database division in a season, or -1 for the
 * class count of season_clubs.c (0 empties the division: its clubs go to no division, Q290). */
int season_divisions_class_size(const char *division, int season);
/* Test switch RECOMP_TEST_EXTRA=spain,portugal (names: germany, italy, spain, portugal; the nation ids at 0x9cf314, 0x9cf360, 0x9cf488 and 0x9cf434): sets the extra
 * division flag (nation +0x11c bit 0x04) of those nations at a new game, before the club moves and the
 * competitions. The Select League(s) screen shows the box only for the first 16 rows, so the rules report runs of
 * Spain and Portugal use this. A test switch only: the player sets the flag on the screen. */
void season_divisions_test_extra(void);
/* 1 for the name of a record that the module adds (a division with no club in the database). */
int season_divisions_new_record(const char *division);
/* The record of the second Greek group (20d): a new record of slice 26's module. */
#define SEASON_DIV_GRE_RECORD "Greek Second Division Group 2"
/* 1 when the club list of a division in a season applies (the 3. Liga list only with the extra division flag). */
int season_divisions_list_on(const char *division, int season);
/* The division that a club of database division db_division goes to when the club moves of a new game leave it out of
 * its division (a group division of a tier in its new form: the tier's division of relegated clubs), or 0 for none. */
uint32_t season_divisions_leaver_division(uint32_t db_division, int season);

#include "season_change.h"
extern const SeasonRuleKind season_kind_divisions;

#endif
