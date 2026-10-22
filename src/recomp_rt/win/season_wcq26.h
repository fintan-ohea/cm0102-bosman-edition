/* World Cup qualifying from the 2026 cycle (0.6.0 slice 29, season_wcq26.c). */
#ifndef RECOMP_SEASON_WCQ26_H
#define RECOMP_SEASON_WCQ26_H

#include <stdint.h>

/* 1 when a qualifying section plays for a World Cup of 48 teams in a season game. */
int season_wcq26(uint32_t self);

/* Called first by slice 28's wraps of wc_europe_league (season_nl_wc.c) at their entry state: the template
 * 0x9185c0, the play-off 0x919910 and get_asia_play_off_team 0x91def0. Each gives 1 when it completed the call
 * (a campaign for a World Cup of 48 teams), else 0 and changes nothing. */
int season_wcq26_europe_template(void);
int season_wcq26_europe_paths(void);
int season_wcq26_europe_no_asia(void);

/* The test log (RECOMP_SEASON_INTL_LOG=1) of the original's European pool at +0xee (count +0xf2) of a World Cup
 * qualifying section (0x919e30) or a European Championship qualifying (0x57f600): its size, Kosovo and every
 * nation in it with no continent or no players' club. what: the name of the line. */
void season_wcq26_log_pool(uint32_t self, const char *what);

#endif
