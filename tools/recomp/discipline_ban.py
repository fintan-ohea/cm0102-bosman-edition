"""Two fixes for the error box discipline..cpp 1149 of the original's ban release (src/recomp_rt/win/discipline_ban.c,
src/recomp_rt/win/season_intl.c).

Discipline::release_ban 0x556d00 (this in esi, the staff id in ebp) frees a player's four ban slots. For slot 3,
the international ban, it also removes his entry from the nation index of the Discipline object (this+0xc, count
this+0x12 at 0x556c47, entries of 8 bytes: the staff id and the key of his first nation +0x1a when the ban was
added, 0x556c10..0x556c57). It looks for the entry with bsearch 0x946464 (comparator 0x55e5f0: the nation key,
then the staff id) under the key of his first nation now (0x556d54..0x556d94), then of his second nation +0x1e
(0x556d9a..0x556dcd). When neither finds it, it shows the box at 0x556dcf..0x556e25 (line 0x47d) and goes on
with the next slot (0x556e32). The box came in a 16-nation long game in June 2014 (Discipline::serve_international_bans
0x557120, from RulingBody::process_match 0x7e8095): the player's first nation was no longer the one of his entry.
rt_discipline_ban_entry finds the entry by the staff id alone: the release removes it (0x556e34) and logs the
nations. With no entry at all it goes on with the next slot, as the original does after its box.

The load. The save 0x555dd0 writes every rule set of the 43 that exists (0x555f2e..0x555f4a); the rule-set load
0x55d420 makes the Oceania body RbOceania (0x7e13d0, slot 35) only when a selected nation has the continent Oceania
(0x6699d0 at 0x55deac, je 0x55def0 at 0x55deb6). A season game with changing rules on from 2005/06 or before with
Australia selected makes the body, and Australia moves to Asia on 1 January 2006 (season_intl.c). A later load left
the body out and read the rest of discipline.dat out of step: a nation index of 1 entry for 16 world bans, the box
at the next release, then a double free. rt_discipline_oceania_body makes the load take the body in that game.
"""

INSNS = {
    0x556DCF: ("lea ecx, [esp + 0x1c]",
               "{ uint32_t e_ = rt_discipline_ban_entry(esi, ebp); if (e_ == 1u) ecx = (uint32_t)(esp + 0x1cu); "
               "else if (e_) { eax = e_; goto L_556e34; } else goto L_556e63; }"),
    0x55DEB6: ("je 0x55def0", "if (cc_e(F) && !rt_discipline_oceania_body()) goto L_55def0;"),
}
