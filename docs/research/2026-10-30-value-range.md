# The value filter of the search Filters screen (2026-10-30)

Research for the value range of the fork (docs/specs/2026-10-30-value-range-spec.md). Addresses are those of the
original 3.9.68.

## The filter block

- A search keeps its filters in a block of 0x99 bytes (SearchEditSession::set_current_filters 0x804c10 copies
  0x99 bytes). The mask at +0 has the value flag 0x8000000: the Value row is on Range (0x8166ef, 0x807a1e).
- The band is the byte +0x23. SearchFilters::set_value_band 0x807310 stores it when it lies from -1 to 7
  (0x80731a..0x807322) and shows an assert otherwise. Its only caller is the Filters input (0x8189ce).
- PLAYER_SEARCH::load_from_file 0x7b2bf0 and save_to_file 0x7b3660 read and write the block as it is; the load
  does not check the band.

## The test

SearchFilters::player_matches_filter 0x807450 (thiscall, the candidate, ret 4). The staff is the staff table
[0xae23bc] + the candidate's id (+0x0c) * 0x6e (0x8075be..0x8075dc). With the value flag set:
- band -1 (0x8079d8): affordable, through the contract's club and 0x544670;
- band 0 to 6 (0x807a27..0x807a5a): the value, staff +0x52, must not be more than the band's amount
  (Cash 0x43f620 with the band);
- band 7 (0x807a6e..0x807a9d): the value must be more than the amount of band 6.
Any other byte makes no value test.

## The screen

Filters: open 0x813210, build 0x8133a0, input 0x818100. Node slots: 0 the session, 1 the copy of the filters that
the screen changes, 2 the page (0x818105..0x818128).

The Value row (General page), with the flag set:
- a cell button in column 4 of the row area (0x8167bc), with no text: the drop-down arrow;
- a popup area for it (0x8167fb: area create with a zero rect, flags 0x500030, colour [0xae31aa], the button);
- a loop from band 7 down to -1 (0x81680a..0x816969): the text "0 - £X" (0x816830..0x81686f, amount by
  Cash::get_display_amount 0x43f460 (5, 5) and cash_format_money 0x441c10), "Over £X" for 7 (0x816871) and the
  affordable text for -1 (0x816812); for the current band a label in column 3 (0x816902); and an item, widget id
  0x11 with the band as data (0x816958).

The input reads the widget's id (0x468b40) and data (0x468b90); id 0x11 calls set_value_band with the data on node
slot 1 and returns -0xb, build again (0x8189b6..0x8189d6).
