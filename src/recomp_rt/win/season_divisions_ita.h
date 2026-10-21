/* Italy's single Serie C of three groups, 2014/15 on (0.6.0 slice 20b; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md
 * slice 20 and Q363; research docs/research/2026-10-20-ita-serie-c.md).
 *
 * The groups A, B and C live in the objects of Serie C1/A (ItaSerC1a), C1/B (ItaSerC1b) and C2/A (ItaSerC2a). The
 * rows of a season are the rows "Serie C group A..C" of the rules table (ita.tsv, comp_file of the object, Q520). Only a season game whose
 * rules season has these rows gets any of it (the "Serie C era"); the base game, RECOMP_SEASON_CODE=off and the
 * seasons before 2014/15 keep the original's code. */
#ifndef RECOMP_SEASON_DIVISIONS_ITA_H
#define RECOMP_SEASON_DIVISIONS_ITA_H

#include <stdint.h>

/* 1 when competition object obj plays a group of the Serie C in its season (its rules season has the rows). */
int season_ita_serie_c(uint32_t obj);

/* The group (0 A, 1 B, 2 C) of an object of the Serie C era, or -1. */
int season_ita_group(uint32_t obj);

/* The largest club count of a class of slice 18's size table for obj: 20 for C1/A and C1/B in the Serie C era,
 * else cap (season_sizes.c). */
int season_ita_cap(uint32_t obj, int cap);

/* The club count of a database division in the club moves of a new game in season (season_clubs.c): the count of
 * its group for C1/A, C1/B and C2/A, 0 for C2/B and C2/C (the division ends, Q290), 1 with the extra division flag
 * (their objects exist then and a league needs a club), in the Serie C era; -1 else. */
int season_ita_division_size(const char *name, int season);

/* The patched instructions (tools/recomp/new_divisions.py; rt.h declares them for the translated code). */
uint32_t rt_ita_table_bytes(uint32_t obj, uint32_t original);   /* push 0x426 of create_table */
uint32_t rt_ita_c2a_clubs(uint32_t obj, uint32_t original);     /* ItaSerC2a::create_table count store */
uint32_t rt_ita_c2_end_clubs(uint32_t obj, uint32_t original);  /* C2/B and C2/C create_table count stores */
uint32_t rt_ita_tier_c2(uint32_t c2a);                          /* 0x6418d5: the C2a object of the tier block */
uint32_t rt_ita_cup_c1b(uint32_t id, uint32_t c1b);             /* ItaCup: group C counts as C1 */
uint32_t rt_ita_cup_fill(uint32_t esp, uint32_t count, uint32_t position);  /* ItaCup: an end to its loop */
uint32_t rt_ita_start_c2(uint32_t c2a);                         /* 0x641986: the C2a object of the start block */

/* 1 when League::create_fixtures 0x6835c0 makes no fixtures for league: the group C object that slice 26's module
 * makes in a season change pass into the era gets its table and fixtures after the regroup (season_uefa_league.c
 * holds the one wrap of 0x6835c0). */
int season_ita_no_fixtures(uint32_t league);

#endif
