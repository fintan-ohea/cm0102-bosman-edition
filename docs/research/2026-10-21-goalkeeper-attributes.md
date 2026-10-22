# Goalkeeper attributes from the player data (2026-10-21)

Max chose to show One On Ones and Consistency in the Goalkeeping group of the profile (attribute groups on, goalkeepers only). He also moved Set Pieces and Technique into that group, so a goalkeeper has no Technical heading. Source: `clean-3968/cm0102.exe` and `clean-3968/Editor/cm0102ed.exe`. Code: `src/recomp_rt/win/groups.c` (`hidden_cell`), `groups_layout.c`.

## The player record

Player table 10 (`player.dat`, 70 bytes a row). +0x5 is current ability (word). +0x0f..+0x1a are the positions and sides. +0x1b..+0x44 are 42 attribute bytes, one a byte.

- The 42 bytes are in alphabetical order of the editor's field names, `staff_pl_acceleration` to `staff_pl_work_rate` (strings of cm0102ed.exe): Acceleration, Aggression, Agility, Anticipation, Balance, Bravery, Consistency, Corners, Crossing, Decisions, Dirtiness, Dribbling, Finishing, Flair, Free Kicks, Handling, Heading, Important Matches, Injury Proneness, Jumping, Leadership, Left Foot, Long Shots, Marking, Movement, Natural Fitness, One On Ones, Pace, Passing, Penalties, Positioning, Reflexes, Right Foot, Stamina, Strength, Tackling, Teamwork, Technique, Throw Ins, Versatility, Vision, Work Rate.
- Check: each of the 30 shown attributes in the record is read at the byte that this order gives. The getter 0x543540 (jump table 0x543a94) reads, for example, Handling at +0x2a, Reflexes at +0x3a, Creativity (Vision) at +0x43, Influence (Leadership) at +0x2f, Off The Ball (Movement) at +0x33 and Set Pieces (Free Kicks) at +0x29. The names come from 0x5428a0 (jump table 0x543158). Determination is staff +0x58, not in this record.
- Not shown: +0x21 Consistency, +0x22 Corners, +0x25 Dirtiness, +0x2c Important Matches, +0x2d Injury Proneness, +0x30 Left Foot, +0x34 Natural Fitness, +0x35 One On Ones, +0x38 Penalties, +0x3b Right Foot, +0x41 Throw Ins, +0x42 Versatility.
- In `player.dat` the bytes are 0 to 20, and 0 means random (editor hint "0 (random) - 20 (best)"). The game fills and converts them when it starts.

## Conversion to 1 to 20

- Some bytes are stored relative to current ability. 0x538bc0 shifts cells 14..31 of the vector of 0x53a610 when a game starts (0x538d43..0x538d6c), and these cells hold +0x35 One On Ones, +0x38 Penalties and +0x41 Throw Ins among the hidden bytes. The profile converts such a byte with x = 10 + 0.05 (2r + CA), rating = trunc(clamp((x + 0.1 x²) / 3 + 0.5, 1, 20)) (0x544380; constants 0x967888 = 0.05, 0x9688b0 = 10, 0x967880 = 0.1, 0x968a80 = 1/3, 0x968968 = 0.5, clamp 0x967890 = 1, 0x968ea8 = 20; 0x945640 truncates). One On Ones uses 0x544380 itself.
- The other hidden bytes are stored as the rating. The profile shows such bytes as they are (for example Flair, +0x28, case 13 of 0x543540). Consistency is shown as it is.

## Fog of war

0x543440 (viewer, staff, attribute) returns 0xf9 when 0x543280 (viewer, staff, attribute) returns 0 (0x54349f..0x5434c6). 0x543280 uses the attribute number only in its pattern of known attributes for knowledge levels 1 and 2 (0x5432db..0x543432) and has no range check, so the runtime gives the two new cells the numbers 32 and 33. The profile passes the viewer at [esp+0x74] at 0x87a74e, one push below the frame that `rt_groups_cells` reads.

## Match engine

The match player keeps the player data at +0x6d (0x6eaeda).

- Consistency: read at 0x70d444 with match Handling (+0xa5), then a random roll against 20 (0x70d45c..0x70d465), the keeper's catch. Other reads: 0x6eb51b, 0x6ec64a, 0x6c0f97, 0x6c216c, 0x6c2631.
- One On Ones: written to match player +0xbd at 0x6ed40e, no read of +0xbd found. The raw byte is read for shoot-out takers (0x6be9dd) and side strength (0x6c10e9). No keeper use found.
- Throw Ins: throw-in taker (0x6ced50). It is the outfield throw-in, not a keeper's throw.
- Natural Fitness (0x6f5899) and Important Matches (0x6eb85a, 0x6ec525) apply to every player.

## Not in CM 01/02 data

Aerial Reach, Command of Area, Communication, Eccentricity, First Touch, Kicking, Punching, Rushing Out and Throwing (FM attributes) have no byte in the record.

## Same save

The two cells take the place of two blank cells of the first column, and the Technical heading becomes a blank cell, so the profile makes the same number of labels with the option on (39 cells, 13 rows). The runtime calls 0x543280 and 0x544380, which the profile build also calls for its own cells. 0x543280 reaches 0x5b02f0, which writes the cache global 0xae1080 with the same club lookup. Proof: the attribute groups check and the same-save sets on the branch (handover report).
