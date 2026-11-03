# Money: how the game shows and reads amounts

Research for `docs/specs/2026-10-24-money-scale-spec.md`. Original 3.9.68, addresses of the original program, read from the disassembly in the generated code (`build/recomp/gen`) and from runs of the fork's build (start.sav of `tests/fixtures`, Arsenal, 14.7.01).

## The amounts of 2001

- A person's value is the int32 at staff `+0x52` (the value search_screens_compare_staff_by_value 0x819f70 sorts by, 0x819fa6). In the 2026 data update preview save the highest is 25,891,466 and the 99th percentile 3,247,522; in start.sav (3.9.68 data) the highest is 25,381,814. The data updates change players, not the money.

## The currency rate

- cash_set_currency (0x43f7f0) sets two doubles for the chosen currency: g_currency_exchange_rate (0xad9cb8, 1.0 for pounds, 1.49 for dollars and more) and g_currency_preset_amount_scale (0xad9da0, 1.0, 1.5 or 2.5). Config::Config (0x5d76ce) and Config::apply_and_save (0x5d85e0) call it; no other code writes the two doubles.
- The rate is read only by the Cash functions: Cash::get_display_amount (0x43f460: the int32 at `this` times the rate, then rounded by cash_get_rounding_step 0x4426e0 unless the flag 0x20), Cash::step_up and step_down (0x43f4e0, 0x43f580: the shown amount plus or minus a rounded step, divided back), Cash::set_from_display (0x43f4c0: a shown amount divided by the rate), Cash::set_from_preset_index (0x43f620: one of seven presets times the preset scale, divided by the rate) and Cash::set_from_amount_table (0x43f720: the same for a table of the caller).
- cash_format_money (0x441c10, cdecl (text, size, s64)) writes an amount with the currency's sign and does not read the rate. Each of its 253 calls follows a call of Cash::get_display_amount, step_up or step_down within the 40 instructions before it, in all 67 functions that call it. No text of the program holds a pound sign before a number.

## The game's logic that reads the rate

Most callers of the Cash functions are screens. Five are the game's own logic, and with a changed rate they would decide differently:

- TRANSFER_OFFER::has_same_terms (0x8e7f70) compares the terms of two offers as shown, rounded (get_display_amount with 4 at 0x8e7fb1, 0x8e7fc0, 0x8e8004, 0x8e8013, 0x8e8074, 0x8e8083).
- transfer_manager_selling_club_must_accept (0x8cd240) compares an offer with an amount as shown (0x8cd31a, 0x8cd32b).
- TRANSFER_MANAGER::check_offer_restrictions (0x8cfc90) steps an amount up by a rounded step (step_up at 0x8cfe51).
- STAFF_CONTRACT::increase_wage_step (0x867b70) and decrease_wage_step (0x867c50) step a wage (step_up at 0x867bb8, 0x867bfc, 0x867c2e).

SearchFilters::player_matches_filter (0x807450) uses a preset (0x43f620) for the value bands of the player search; it chooses only what a list shows. The other callers of step_up, step_down, set_from_display and the presets are contract, transfer, club and search screens.
