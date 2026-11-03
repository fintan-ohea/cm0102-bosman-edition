# Club editor: the club screen's tabs and the club's values

Research for `docs/specs/2026-10-25-club-editor-spec.md`. Original 3.9.68, addresses of the original program, read from the disassembly in the generated code (`build/recomp/gen`) and from runs of the fork's build (start.sav of `tests/fixtures`, Arsenal, 14.7.01).

## The tabs of the club screen

- The build is 0x454900 (pushed with the input 0x4671f0 and the leave 0x469da0 by open_club_squad_screen 0x453d60, 0x453e2c). It makes 0x19 tab bar entries of 0xd4 bytes on its stack (0x45494e) and fills ten: Squad, Transfers, Next Match, Fixtures, Finances & Info (data 1 to 5), then the links Tactics, Training, Last Match, the division and History. It calls the tab bar 0x5ed780 (entries, 10, the byte of node slot 4, &y_top, &y_bottom, 1) at 0x455243 (`docs/research/2026-10-15-team-views-frame-inventory.md`: with more than five entries and split 1, (count + 1) / 2 go on top).
- After the call it switches on slot 4 (esi, which it set before the call at 0x455205 and keeps across it): `dec esi; cmp esi, 4; ja 0x456381`, jump table 0x456724 (0x45525c, 0x455273, 0x456342, 0x456359, 0x45636d). Case 5 is only `draw_club_information_screen 0x460590 (y_top, y_bottom)`. Any other value shows the error box of club_screens.cpp line 0x505 (0x456381..0x4563dd).
- The input 0x4671f0: a click on widget id 2 (a tab) whose data differs from slot 4 sets slot 4 to the data and rebuilds (0x467701..0x4677ac). Later, on every pass, a click or not, it switches on slot 4 for the widgets of each tab (0x467cbd: cases 1 to 5; any other value shows the error box of line 0x1f75 at 0x4688c8).

## The values

- Club (draw_club_information_screen reads them): `+0x64` status, 1 Professional, 2 Semi-Professional, 3 Amateur (0x460ec6..0x460efd, texts 0x9904b0, 0x99049c, 0x990494); `+0x69` the stadium record, a pointer at run time (0x4610d4); `+0x6d` the club owns the ground (0x4610d1); `+0x7f` training facilities, above 15 "Top facilities plus youth academy" (0x461506); `+0x80` reputation, s16 (0x461525).
- Stadium record: `+0x3c` capacity, `+0x40` seated capacity (0x4612e6, 0x4612f1), `+0x4c` retractable roof, `+0x4d` under-soil heating (0x4612f6, 0x4612fd); `+0x44` expansion limit (`docs/research/2026-10-16-stadium-research.md`).
- Board confidence: MANAGER_MANAGER (`[0xb67a44]`) keeps a record of 0x49 bytes per club id at `[this]`; `+6` is the board's confidence, s16 (get_board_confidence_band 0x690b70: `[[this]] + id * 73 + 6`, bands at 3,000, 3,500, 4,275, 6,500, 8,500 and 9,500). The weekly update and the reactions keep it at most 10,000 (0x69a217, 0x69a2f9, 0x694f6d) and test it against 1 (0x69b120).
- Money: the finance record's balance and the transfer funds adjustment of the player editor (`docs/research/2026-10-24-player-editor-attributes.md`).
- National teams: 0x53b900 (club) is not 0 for a national team; the build greys the Transfers tab for one (0x454efa).
