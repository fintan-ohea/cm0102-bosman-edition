"""Club counts of the original that become run-time values for the new divisions (0.6.0 slice 20).

INSNS replaces a whole instruction, as in league_sizes.py. Evidence: docs/research/2026-10-20-new-divisions.md.

GerRegional::create_table 0x5dd7f0 stores 19 at 0x5dd801 (kept when +0x40 is 2000, je at 0x5dd807) and else
18 at 0x5dd809, then sizes its table from +0x3e (0x5dd80f..0x5dd832) and takes the clubs of German Regional
with the North tag (0x5dd8d5..0x5dd8f1). rt_ger_regional_clubs (src/recomp_rt/win/season_divisions.c) gives
the original's count outside a season of the 3. Liga, and the clubs of the division in one (20 from 2008/09).
"""

# PorSecondB::create_table 0x7d2400 stores 20 at 0x7d2412 (before its first-season test) and sizes its table from
# +0x3e; the zone builder 0x7d2870 wants exactly 20 (cmp bp, 0x14 at 0x7d2965 and 0x7d29ff) and makes a
# League_Stage of 20 (push 0x14 at 0x7d2ab2). Liga 3 from 2021/22 has 2 series of 12, then of 10 (Q291).
PATCHES = {
    0x7D2965: (0x14, "rt_por_stage_clubs(esi, 0x14u)"),
    0x7D29FF: (0x14, "rt_por_stage_clubs(esi, 0x14u)"),
    0x7D2AB2: (0x14, "rt_por_stage_clubs(esi, 0x14u)"),
}

# Slice 20b, Italy's Serie C of three groups (src/recomp_rt/win/season_divisions_ita.c, doc
# docs/research/2026-10-20-ita-serie-c.md): C1/A, C1/B and C2/A hold 18 clubs (create_table allocates 18 rows of 0x3b
# bytes, push 0x426 at 0x64b67b, 0x64f82b, 0x653a9b, the object in edi from 0x64b679, 0x64f829, 0x653a99); a group of
# the Serie C holds up to 20. C2/A's count store 0x653aa2 (C1/A and C1/B store through slice 18). ItaSerA::
# start_new_season 0x641700 reads C2/B and C2/C whenever C2/A exists: the loads of the C2/A object at 0x6418d5 (the
# tier block) and 0x641986 (the start block) give 0 in the Serie C era after the runtime has done C2/A's part.
PATCHES.update({
    0x64B67B: (0x426, "rt_ita_table_bytes(edi, 0x426u)"),
    0x64F82B: (0x426, "rt_ita_table_bytes(edi, 0x426u)"),
    0x653A9B: (0x426, "rt_ita_table_bytes(edi, 0x426u)"),
    # The two-group Serie C2 of 2011/12 to 2013/14 with the extra division flag (Q529): C2/B's table (0x657f5b, the
    # object in edi from 0x657f59) holds up to 20 too.
    0x657F5B: (0x426, "rt_ita_table_bytes(edi, 0x426u)"),
})

INSNS = {
    0x653AA2: ("mov word ptr [edi + 0x3e], 0x12", "W16(edi + 0x3eu, rt_ita_c2a_clubs(edi, 0x12u));"),
    # C2/B and C2/C end in the era; with the flag their objects exist and keep one club (create_fixtures needs one).
    # In the two-group Serie C2 (2011/12 to 2013/14, Q529) C2/B takes the clubs of its division and C2/C ends.
    0x657F62: ("mov word ptr [edi + 0x3e], 0x12", "W16(edi + 0x3eu, rt_ita_c2_end_clubs(edi, 0x12u));"),
    0x65C0F2: ("mov word ptr [edi + 0x3e], 0x12", "W16(edi + 0x3eu, rt_ita_c2_end_clubs(edi, 0x12u));"),
    0x6418D5: ("mov ecx, dword ptr [eax + edx*4]", "ecx = rt_ita_tier_c2(R32(eax + edx * 4u));"),
    # The last tier call of both blocks (C2/C after 0x641913, or C1/B): in a two-group C2 season (Q529) the ended
    # C2/C gives C2/B again (rt_ita_tier_last).
    0x641936: ("mov ecx, dword ptr [edx + eax*4]", "ecx = rt_ita_tier_last(R32(edx + eax * 4u));"),
    # ItaCup::select_entrants 0x63faf0 takes the lower places from the clubs whose last division was C1/A or C1/B
    # (0x63fc6f..0x63fc7f by positions 3 to 5; 0x63fcef..0x63fcff from position 5 up, with no end, 0x63fd22..0x63fd30).
    # In the Serie C era group C (C2/A) counts too, so the 32 places fill and the second loop ends: the compare with
    # the C1/B id gives "equal" for the C2/A id (rt_ita_cup_c1b).
    0x63FC79: ("cmp ecx, dword ptr [0x9cf57c]",
               "t0 = ecx; t1 = rt_ita_cup_c1b(ecx, R32(0x9cf57cu)); t2 = t0 - t1 - 0; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),
    # Its second loop then goes on to the next position (inc cl at 0x63fd26) while it has fewer than 32 (cmp si,
    # 0x20 at 0x63fd28): in the era, from position 40 on the runtime fills the list (on the stack at esp + 0x1c, 4
    # bytes a club) to 32 and the loop ends (rt_ita_cup_fill), so it cannot run for ever.
    0x63FD28: ("cmp si, 0x20",
               "esi = (esi & 0xffff0000u) | (rt_ita_cup_fill(esp, esi & 0xffffu, ecx & 0xffu) & 0xffffu); "
               "t0 = (esi & 0xffffu); t1 = 0x20u; t2 = ((t0 - t1 - 0) & 0xffffu); SETF(K_SUB | SZ2, t0, t1, t2, 0);"),
    0x63FCF9: ("cmp ecx, dword ptr [0x9cf57c]",
               "t0 = ecx; t1 = rt_ita_cup_c1b(ecx, R32(0x9cf57cu)); t2 = t0 - t1 - 0; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),
    0x641986: ("mov ecx, dword ptr [ecx + edx*4]", "ecx = rt_ita_start_c2(R32(ecx + edx * 4u));"),
    # The Coppa Italia Serie C of the Serie C era and the two-group C2 seasons (O259 A21, Q628): smaller groups and
    # their match days, and the knockout's runners-up and seeds for the clubs of the Serie C divisions (rt_ita_ccup_*, season_divisions_ita.c).
    0x63D5EE: ("mov word ptr [esi + 0x3e], 5", "W16(esi + 0x3eu, rt_ita_ccup_group(esi, 5u));"),
    0x63CFD8: ("mov word ptr [ecx], 5", "W16(ecx, rt_ita_ccup_days(edi, 5u));"),
    0x63DE55: ("cmp edx, 0x162",
               "t0 = edx; t1 = rt_ita_ccup_seconds(ebp, 0x162u); t2 = t0 - t1 - 0; SETF(K_SUB | SZ4, t0, t1, t2, 0);"),
    0x63DE7B: ("mov esi, 0x16", "esi = rt_ita_ccup_first_seed(ebp, 0x16u);"),
    0x63DE86: ("lea edx, [ecx + 0x58]", "edx = rt_ita_ccup_seed_slot(ebp, ecx, esi);"),
    # PorCup::collect_entrants 0x7ce220, second pass: a club with no division is passed over (je 0x7ce3ed at
    # 0x7ce37d). With Liga 3 the club moves leave more Portuguese clubs with no division, and the cup (164 entrants,
    # 0x7ce234) stopped at por_cup.cpp 74 in a 2021/22 start; rt_por_cup_none takes them then (Q528).
    0x7CE37D: ("je 0x7ce3ed", "if (cc_e(F)) { if (rt_por_cup_none(ebx)) goto L_7ce3a6; goto L_7ce3ed; }"),
    0x7D2412: ("mov word ptr [edi + 0x3e], 0x14", "W16(edi + 0x3eu, rt_por_group_clubs(edi, 0x14u));"),
    0x5DD801: ("mov word ptr [edi + 0x3e], 0x13", "W16(edi + 0x3eu, rt_ger_regional_clubs(edi, 0x13u));"),
    0x5DD809: ("mov word ptr [edi + 0x3e], 0x12", "W16(edi + 0x3eu, rt_ger_regional_clubs(edi, 0x12u));"),
}
