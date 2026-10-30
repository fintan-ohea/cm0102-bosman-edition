# Hidden attributes of a player (2026-10-30)

Research for the Hidden tab of the fork (docs/specs/2026-10-30-hidden-tab-spec.md). Addresses are those of the
original 3.9.68.

## Where they are

The profile shows 31 attributes through staff_get_player_attribute 0x5431e0 / 0x543540 (the bytes are listed in
docs/research/2026-10-24-player-editor-attributes.md). The player data (staff +0x61) keeps its attribute bytes
from +0x1b to +0x44 in alphabetical order of the attribute names: the 31 shown ones fill that order with these gaps,
which database_player_to_attribute_vector 0x53a610 reads with the others. The staff record keeps the personal
attributes from +0x56 to +0x5d, with Determination (+0x58, shown) among them, in the same order.

| Value | Where | Evidence |
|---|---|---|
| Current ability | player +0x05, s16 | picker, 0x543540 (editor research) |
| Potential ability | player +0x07, s16 | 0x8e8345 |
| Consistency | player +0x21 | read by staff_get_player_strength_flags 0x546ab0 at 0x546efb; order |
| Corners | player +0x22 | order |
| Dirtiness | player +0x25 | staff_get_player_weakness_flags 0x5470d0 at 0x5471f5; order |
| Important Matches | player +0x2c | order |
| Injury Proneness | player +0x2d | physio_assess_player_fitness 0x7a2650 at 0x7a26ba, 0x7a2746, 0x7a2848 |
| Left Foot | player +0x30 | order (the profile shows only Preferred Foot) |
| Natural Fitness | player +0x34 | Injury::init_squad_fitness 0x62a920 at 0x62aa03; 0x7a2898 |
| One On Ones | player +0x35 | order |
| Penalties | player +0x38 | Tactics_Screen::set_default_penalty_takers 0x8a9370 at 0x8a9441 |
| Right Foot | player +0x3b | order |
| Throw Ins | player +0x41 | order |
| Versatility | player +0x42 | order |
| Adaptability | staff +0x56 | order |
| Ambition | staff +0x57 | contract_manager_get_deserved_squad_status 0x4ca470 at 0x4ca4ee, 0x4ca644 |
| Loyalty | staff +0x59 | staff_fade_staff_relationships 0x547f90 at 0x547fb0 |
| Pressure | staff +0x5a | order |
| Professionalism | staff +0x5b | game_check_players_missing_training 0x5d6aa0 at 0x5d6b7e, 0x5d6c4f |
| Sportsmanship | staff +0x5c | order |
| Temperament | staff +0x5d | 0x547f90 at 0x548016; staff_get_player_weakness_flags at 0x5471de |

"Order" means the name follows from the alphabetical order of the record and is not read from code yet. The
strings of these names are not in the program (the game's text comes from its language files), so the code that
uses a byte is the only proof. A later check can read MATCH_PLAYER::setup and calc_match_attributes, which copy
many of them.

## Rated bytes

Corners, Penalties, Throw Ins and One On Ones are relative bytes, as the shown technical attributes: Ray Parlour
of start.sav has -30 for Penalties, -22 for Throw Ins and -62 for One On Ones. The tab rates them as the game rates
an attribute byte with the current ability, player_attribute_rating 0x544380 (player data, byte), which gives 1 to
20 (Ray Parlour: Corners 18, Penalties 12, Throw Ins 13, One On Ones 8). The other values are kept as 1 to 20 and
shown as they are.

## The profile's tabs

- The build 0x870700 reads the tab from node slot 1 (0x8707bf..0x8707d6, byte copy at its esp + 0x1f).
- It makes five tab bar entries of 0xd4 bytes, data 1 to 5 at +0xd0 (0x874ad5, 0x874b36, 0x874bb7, 0x874c27,
  0x874caf), and calls the tab bar 0x5ed780 at 0x874cc1 with (entries, 5, the tab, ...); it keeps the tab at its
  esp + 0x6c (0x874cba).
- A click on a tab is widget id 0x11 with the entry's data; the input 0x888260 stores the data in slot 1 and
  builds again (0x88954e..0x8895d5).
- The build draws the page by the tab (0x875a29..0x875a46, jump table 0x8764ec, cases 1..5: 0x87a510 Profile,
  Injuries & Bans, 0x87cd90 Contract, 0x87dd80 Transfer, 0x885820 History) and shows an error box for any other
  value (0x876363). Its own widgets test the byte copy for 1, 2 and 5 only (0x870955, 0x875132, 0x875443,
  0x87591a, 0x875c01, 0x875ebd), so Contract (3) makes none.
- The input 0x888260 also switches on slot 1 for each tab's widgets (0x889a19..0x889a28, jump table 0x889c14,
  cases 1..5), with an error box for any other value (0x889aa8, staff_screens.cpp line 13210).
- With six entries the bar draws two rows of three: Profile, Injuries & Bans and Contract above the page, Transfer,
  History and Hidden below it, as it draws the club screen's tabs and links.
