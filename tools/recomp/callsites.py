"""Calls of the original whose return address the runtime tests, so that a wrap acts on one call site only.

Each entry names a call instruction and the function it calls. The translation checks that the
instruction at the address is that call, and the build stops if it is not. table.c gets a constant
`rt_site_<name>` with the return address (the address of the call plus 5), for the runtime to compare
with the return address on the stack. The evidence for each address is in
docs/research/2026-10-14-columns-and-colours-inventory.md and
docs/research/2026-10-15-squad-dropdown-inventory.md, docs/research/2026-10-15-commentary-speed-inventory.md,
and for setup_menu in
docs/research/2026-10-13-screen-stack-inventory.md section 6a.
"""

# name: (address of the call, called function)
CALL_SITES = {
    # Attribute colours (#19): the label create 0x804100 of an attribute number.
    "profile_value": (0x87AC15, 0x804100),       # player profile, every value cell (0x87a510)
    "squad_attribute": (0x45C7CA, 0x804100),     # squad list, View Attributes (0x456900)
    # Attribute groups (#43): the attribute area and the name label of each cell of the player profile
    # 0x87a510 (docs/research/2026-10-15-attribute-groups.md).
    "profile_area": (0x87A705, 0x55FBA0),
    "profile_name": (0x87AA09, 0x804100),
    # Test saves (testrun.c): the write of 0x1cc stack bytes that the save 0x52dad0 never sets, into
    # index.dat (docs/research/2026-10-15-attribute-groups.md).
    "index_pad": (0x53142B, 0x932E10),
    # Match columns (#23): the player list area of the tactics screen 0x899ea0, 9 columns.
    "tactics_area_heading": (0x89C894, 0x55FBA0),  # with a heading row
    "tactics_area": (0x89C93C, 0x55FBA0),          # without a heading row
    # Match columns (#23): the status text of a row in the Condition and Match Rating views.
    "tactics_status": (0x89E01A, 0x8AADB0),
    # Match columns (#23): the Player Ratings area of the match screen 0x722ed0, 16 columns.
    "ratings_area": (0x722FA3, 0x55FBA0),
    # Data set choice (#39): the menu builder call of the Setup Game build 0x81bab0, with 9 items.
    "setup_menu": (0x81BCC5, 0x5ED300),
    # Setup Game menu B (0.6.0, Max's play test): the sub-heading "Setup Game" of the build 0x81bab0. The
    # season strip takes its place (database.c).
    "setup_subheading": (0x81BB5F, 0x803F80),
    # Assistant drop-down (#12): the Clear Squad button of the squad list 0x456900.
    "squad_clear": (0x458AD0, 0x8040A0),
    # Team views (#22): the Filter button of the squad list 0x456900, made after Clear Squad.
    "squad_filter": (0x458B10, 0x8040A0),
    # Holiday until next match (#30): the Go on Holiday item create of the left bar build 0x75db00
    # (docs/research/2026-10-15-holiday-next-match-inventory.md, 4a).
    "holiday_item": (0x75F1D4, 0x55F990),
    # Faster commentary (#31): the Very Fast item create of the speed popup on the match screen 0x718e90,
    # and the speed getter call whose result the build copies into the match (esi) at 0x719c4e.
    "speed_very_fast": (0x719861, 0x55F990),
    "speed_copy": (0x719C49, 0x5FCFF0),
    # No CD box: the message box "CD Not Required" (title 0xa81384, text 0xa81354) at the end of a new game
    # 0x828110 and of a load 0x82a9c0. Both callers drop the return value (0x82a990, 0x82e405).
    "cd_box_new": (0x82A98B, 0x5E8290),
    "cd_box_load": (0x82E400, 0x5E8290),
    # No box Database..cpp 7784: the error box of database_load_language_names 0x5339a0 when the counts in
    # the header of CM3_TEMP\<lang>.lng differ from the database (checks 0x533e6a..0x533ec5, the only
    # jumps to 0x53585d). The caller drops the return value (0x5358bc).
    "lng_count_box": (0x5358B7, 0x5E8290),
    # Foreign player colour (#47): the name of each row, the cell button of the squad list 0x456900 and the
    # widget create of the tactics screen 0x899ea0 (docs/research/2026-10-16-foreign-and-tactics-positions.md).
    "squad_name": (0x45BA84, 0x8040A0),
    "tactics_name": (0x89E3EB, 0x55F990),
    # Options box (#56): the Game Settings item create of the side bar drop-down "Game Options" in 0x75db00
    # (docs/research/2026-10-16-options-screen.md, 1a). The new item follows it.
    "options_item": (0x760602, 0x55F990),
    # Options box (#56): the input poll of the main loop 0x7fc7e0. Its result goes to the Game Settings hook
    # test at 0x7fcd25.
    "main_poll": (0x7FCC14, 0x55F620),
    # Work permits after Brexit (0.6.0 slice 17, season_kind_nation.c): the calls of player_needs_work_permit
    # 0x8c7560 where a signing is decided (docs/research/2026-10-17-season-rules-17.md, section 5): the two offer
    # checks of 0x8c7470 and the two of 0x8c7690, the completion 0x8c38b0 and its call of is_work_permit_granted
    # 0x8c7950.
    "permit_offer_a": (0x8C74A9, 0x8C7560),
    "permit_offer_b": (0x8C7539, 0x8C7560),
    "permit_check_a": (0x8C7704, 0x8C7560),
    "permit_check_b": (0x8C7872, 0x8C7560),
    "permit_complete": (0x8C3902, 0x8C7560),
    "permit_decide": (0x8C391B, 0x8C7950),
}

SITES = {addr: (name, target) for name, (addr, target) in CALL_SITES.items()}
