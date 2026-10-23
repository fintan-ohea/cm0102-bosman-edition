/* Stage slots after a load (0.6.0 load sweep, season_load.c). */
#pragma once
#include <stdint.h>

/* After a League or Cup load that succeeds: the stage slots after +0x30 are set to 0 (a season game only). */
void season_load_clear_stages(uint32_t obj);
/* After a League load: the table of a Club World Championship parent of Form K (season_kind_club_world.c). */
void season_club_world_loaded(uint32_t obj);

