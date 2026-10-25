# Shortlist All: the search screen, its result list and the shortlist

Research for `docs/specs/2026-10-25-shortlist-all-spec.md`. Original 3.9.68, addresses of the original program, read from the disassembly in the generated code (`build/recomp/gen`) and from runs of the fork's build (start.sav of `tests/fixtures`, Arsenal, 14.7.01).

## The screen

- Player & Staff Search: open 0x808540, build 0x808690, input 0x80eb00 (cdecl (s16 widget), ends with a plain `ret`), close 0x8131c0. The input reads node slot 0 into ebp (0x80eb28): the SearchEditSession. Its byte +0 is the current list (0x80eb91) and its ten PLAYER_SEARCH records of 0x2b0 bytes start at +1, the list type at +0xa of each (SearchEditSession::get_list_type 0x80eaa0; 1 is the Short List: add_player_to_short_list tests it at 0x8045b9).
- The row of Filters and Delete: an area of two columns from x 0x212 to 0x30c (classic pixels) at 0x80acea (0x55fba0), Filters in column 0 with id 0x13 at 0x80ad2b (0x8040a0), Delete in column 1 with id 0x16 after the tests at 0x80ad62..0x80ad7a. The Search and View drop-downs end near x 360, so x 395 to 525 of the row is free.
- The input takes a box answer first: box id `[0xdd6be0]` (0x19 at 0x80ec19) and the button `[0xdd6be4]`. Cancel with changes asks "Changes have been made to the player search. Cancel all changes ?".

## The result list

- SearchEditSession::get_current_results 0x804a40 (thiscall (LLIST **out, int *count, compare), ret 0xc) gives the LLIST at +0x361e and fills it once through PLAYER_SEARCH::get_result_list 0x7af160 while the cache flag +0x3634 is 0 (0x804a57, 0x804a8a).
- get_result_list adds only the results that pass the filters (player_matches_filter 0x807450 at 0x7af40d, skipped for the default mask 0x14140081 0x44aaa884) and that lack bit 1 of +0x20 (0x7af3ea), then sorts them and appends each 0x2d-byte entry to the list (0x68d0d0 at 0x7af547). So the list is every row of every page of the screen. The person is the staff id at +0x0c (the staff table `[0xae23bc]`, 0x6e bytes a person).
- LLIST (0x68d020): u16 count at +0, head +2, tail +6, cursor +0xa. A node of 0xe bytes: the data at +0, the next node at +6 (LLIST::get_next 0x68d1b0) and the previous at +0xa. The count is 16 bits, as `playersearch.c` notes.

## The shortlist

- SearchEditSession::is_player_on_short_list 0x804540 asks PLAYER_SEARCH::contains_player 0x7af600 of the Short List record.
- SearchEditSession::add_player_to_short_list 0x804590 copies the ten searches into the undo copy (0x7b7690 for each, 0x8045f9..0x80461c), adds the person with PLAYER_SEARCH::add_player 0x7af720 (which makes a result entry and calls add_result 0x7af870), sets the changed marks (+0x1b09, +0x3616, +0x361a) and clears the cache flag +0x3634.
- add_player stores a non-player too: in a run, Shortlist All on Staff Search without the test below put 2052 people into the Short List's results (+0x1f6 from 1672 to 3724), and the Short List showed only the 99 players among them.

## When the profile offers Add To Shortlist

staff_screens_build_profile_screen 0x870700, before the item at 0x8712d7 (Remove From Shortlist at 0x8711f6 when SCOUT_MANAGER::is_player_shortlisted 0x7f7490 says so):

- the staff id is below `[0xae2364]`, the people of the loaded leagues (0x8710cb);
- human_manager_current_controls_club 0x600c90 (club of the main contract, 0x4cf5a0 at 0x87114c; 0, 1) is 0 (0x871181);
- staff_is_not_board_member 0x4cb960 is not 0 (0x871194);
- the current human manager (`[0xb74c26 + slot * 0x300]`, 0x8709bd) exists and his byte +0x5f is not 5 (0x8711ad);
- staff +0x61, the player record, is not 0 (0x8711bb);
- player_regen_get_retirement_status 0x7a7eb0 is not 2 (0x8711d4).

The runtime tests the club with staff +0x39 in place of the main contract's club.
