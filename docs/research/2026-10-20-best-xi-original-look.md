# How the original draws a pitch, an eleven, a player list and the comparison

Date: 2026-10-20. Source: `clean-3968/cm0102.exe` (3.9.68), read with REA (Ghidra 12.1.4). Every fact has an address. "Inference" marks what the code does not show directly. Nothing in the repo was changed.

## Helpers used below

- `0x55f990` Display::add_item, thiscall on the Display of the player slot `[0xb74c4c + s16[0xb77c7a]*0x300]`, 18 args, ret 0x48. It copies the args into the next widget (0x18c bytes) through `0x5ee150`. Arg order (from the stores in 0x5ee150):
  1. type flags (+0xc): 1 plain, 2 button, 0x40 drag, 0x200 link button. Values seen: 1, 0x42, 0x102, 0x202, 0x246, 0x302, 0x346.
  2. to 5. x1, y1, x2, y2 (+0x10..+0x1c). 0x5ee150 swaps reversed edges, clamps x1 and y1 to 0, x2 to 799 and y2 to 599.
  6. column (+0x20), 7. row (+0x24). Used only when the item belongs to an area.
  8. style (+0x38), the panel flags of `include/gui_utils.h`: 0x10 fill, 0x20 border (bevel, or outline with 0x200 flat), 0x40 sunken, 0x400 line, 0x2000 dashed, 0x10000 ellipse.
  9. colour (+0x72), 10. second colour (+0x74), 11. text flags (+0x3c), 12. font (+0x76), 13. text colour (+0x78), 14. text (+0x80), 15. (+0x40), 16. id (+0x8), 17. data (+0x48), 18. owner area (+0x7a), -1 for none. When it is not -1, 0x55f990 calls `0x403200` to add the item to that area.
- `0x55fba0` Display::add_area (x1, y1, x2, y2, columns, column weights*, rows, row weights*, flags, colour, owner). `0x804100` label and `0x8040a0` button in an area cell (area, column, row, style, colour, text flags, font, text colour, text [, id, data]). `0x803fd0` text and `0x804030` button at their own rectangle.
- `0x5e4800(red, green, blue, format)` packs an RGB565 pixel.
- Text flags (`include/gui_text.h`): 1 left, 2 top, 0x20 emboss, 0x40 right. No flag means centred both ways. 0xc carries no left or top bit, so it centres. The meaning of bits 4 and 8 in the text flags: I don't know.
- Fonts (`src/si/gui_font.c`): 1 arial_narrow_10, 2 arial_narrow_11, 3 arial_14, 4 arial_16, 5 arial_18, 6 trade_cond_24_bold, 7 trade_cond_28_bold. There is no font 0 (text spec). Font 2 is 18 px high in classic mode (comparison inventory).
- Colours (data map): `[0xaebdec]` yellow, `[0xaebdea]` black, `[0xaebde8]` pale cyan (128,255,255), `[0xaebe04]` dark blue (0,0,128), `[0xaebdd4]` orange (255,128,0), `[0xaebdd6]` navy (0,0,96), `[0xae3184]` pale grey, `[0xae317e]` grey, `[0xae31a8]` red, `[0xae31aa]` green, `[0xae3154]` silver.

## 1. 0x8ac600 draw_pitch

### Arguments

The function map lists 10 arguments. The code reads 11 (cdecl, plain `ret`, returns 1):

| # | Meaning | Evidence |
|---|---|---|
| 1-4 | left, top, right, bottom | 0x8ac6a0.. |
| 5-8 | int* out_left, out_top, out_right, out_bottom | stores after the stripes |
| 9 | `across`: 0 = goals at top and bottom, not 0 = goals at left and right | `MOV EAX,[ESP+0x7c]; TEST; JZ 0x8aceea` at 0x8ac677 |
| 10 | short owner area (-1 = none) | `MOV ESI,[ESP+0x6c]; CMP SI,-1` at 0x8ac656 |
| 11 | float scale: pixels per yard | `FLD float ptr [ESP+0x74]` at 0x8ac8bb, 0x8ad11e |

Both callers pass 0x40800000 = 4.0f as argument 11. The name `widget_id` in the function map is wrong. Argument 10 is the owner area that every item goes into (arg 18 of 0x55f990).

### What it does

It draws nothing itself. It adds about 22 widgets to the current Display through `0x55f990` (type 1). The normal widget draw then paints them. So a build function calls it, and a widget draw callback does not.

Colours:

- `[0xde1f34]` = RGB(0, 0x40, 0), the grass. Made once at 0x8ac61b, guard bit 1 of `[0xde1e20]`.
- `[0xde1e24]` = RGB(0, 0x38, 0), the darker stripes. Made once at 0x8ac645, guard bit 2.
- Line colour (local, every call, 0x8ac670): RGB(0, 0x80, 0) when the owner is -1, RGB(0, 0x50, 0) in an area. The lines are light green. They are not white.

Items, for argument 9 = 0 (both callers), from 0x8aceea to 0x8ad76d:

1. Background: the whole rectangle, style 0x30 (fill and bevel border), colour grass. Style 0x70 (sunken bevel) when the owner is not -1. (0x8acf16, 0x8acf5f)
2. Three stripes, style 0x10, colour `[0xde1e24]`, inset 1 px: x from left + w*k/7 to left + w*(k+1)/7 for k = 1, 3, 5, from top + 1 to bottom - 1. So the stripes run from goal to goal. (0x8acf91..0x8ad119)
3. The playing area: out_left = left + 2s, out_top = top + 2s, out_right = right - 2s, out_bottom = bottom - 2s, s = scale, truncated by `__ftol` 0x945640. (0x8ad11e..0x8ad162)
4. Touch and goal lines: outline 0x220 (border + flat) of the playing area. (0x8ad1b4)
5. Halfway line: style 0x400 from (out_left, cy) to (out_right, cy), cy = (out_top + out_bottom + 1)/2 rounded down. (0x8ad210)
6. Centre circle: style 0x10020 (ellipse, border), radius 10s. Colour and second colour are both the line colour. (0x8ad240..0x8ad2c4)
7. At each end (top: 0x8ad2c9..0x8ad548, bottom: 0x8ad54d..0x8ad76d):
   - the penalty arc: an ellipse 0x10020 of radius 10s around the penalty spot;
   - two fills (0x10) that paint the grass colour and then the stripe colour over the part of the arc inside the box, so only the "D" stays;
   - the penalty spot: style 0x400 with both ends at 12s from the goal line;
   - the goal area: outline 0x220, 6s deep;
   - the penalty area: outline 0x220, 18s deep, 22s each side of the centre.
   The constants are doubles at 0x9688b0 (10.0), 0x96af40 (22.0), 0x969550 (18.0), 0x96d6a0 (12.0), 0x969000 (6.0), 0x967890 (1.0). Which half-width the goal area uses: inference 10s (the same stack double as the arcs), not checked item by item.

For argument 9 not 0 (0x8ac686..0x8acee9) the same items turn by 90 degrees: the stripes are bands from top + h*k/7 to top + h*(k+1)/7, the halfway line is vertical. No caller of 3.9.68 uses this branch.

### Units

The rectangle is in 800x600 numbers. 0x5ee150 clamps to 799 and 599. In the port, the widget create wrapper (`src/recomp_rt/win/display.c` header) scales 800x600 numbers to the screen. So in widescreen mode each item is scaled on its own: x by 1.6 and y by 1.333. The circles then come out as ellipses 1.2 times wider than high. Inference from the wrapper rule, not seen in a run.

### Callers (procedure_callers: 2)

| Caller | Call | Rect | Arg 9 | Area | Scale |
|---|---|---|---|---|---|
| tactics screen build 0x899ea0 | 0x89e434 | (450, 145)-(780, 545) | 0 | -1 | 4.0 |
| team award page 0x415f60 | 0x416052 | (480, *top)-(780, *bottom) | 0 | -1 | 4.0 |

Both run during a screen build. 0x415f60 is called from 0x4155bf. Ghidra puts that address in no function. It lies after the award screen build 0x414cf0, which 0x414c9e pushes as a build. Inference: 0x4155bf is part of that build.

### Can the runtime call it in its own build?

Yes, under these conditions:

- Call it while the screen builds, after the frame areas, with owner -1. It adds widgets to the Display of the player slot, as `best_xi.c` does with WIDGET_CREATE.
- It allocates nothing and calls no random function. It only writes the widget table, the draw index and the colour guard `[0xde1e20]`. Inference: it does not change the game.
- It uses about 22 of the 1200 widget slots. Widget indices after it move by that count, so code that stores a widget index (for example `pitch_widget` in `views.c`) must read the index after the call.
- Argument 9 = 0 gives the upright pitch with the own goal at the bottom, which `0x8ad780` expects. For the port's pitch on its side, argument 9 = 1 is the only branch that draws it. The position function 0x8ad780 has no sideways mode, so the runtime must map the points itself.

### 0x8ad780 get_pitch_point_for_position_flags

cdecl(position flags, out_left, out_top, out_right, out_bottom, int* x, int* y, five_columns). Upright pitch, own goal at the bottom. h = out_bottom - out_top, w = out_right - out_left.

- y: 1 (GK) bottom - 10; 2 (SW) bottom - h/12 - 15; 4 (D) bottom - h/6 - 20; 8 (DM) bottom - h/3 - 10; 0x10 (M) bottom - h/2; 0x20 (AM) bottom - 2h/3; 0x40 (F) bottom - 5h/6. GK x is the centre.
- x, with five_columns = 0: 0x80 w/8, 0x100 3w/8, 0x200 5w/10, 0x400 5w/8, 0x800 7w/8. With five_columns not 0: w/10, 3w/10, 5w/10, 7w/10, 9w/10.
- five_columns comes from `0x5b2e80` (Formation::has_central_slot_in_line). An unknown flag shows the assert box (lines 0x21a8, 0x21dd) and returns 0.

## 2. The eleven on a pitch

### Team award page 0x415f60

- Pitch: draw_pitch(480, top, 780, bottom, ..., 0, -1, 4.0) at 0x416052.
- Left list area: (110, top + 40)-(470, bottom), 4 columns with weights 14, 7, 60, 45 (bytes at 0x415fb0..0x415fbf), rows = (height - 16) / font_height(2), flags 2, at 0x4164a3. Per row, 0x4164c7..0x416681:
  - column 0: the shirt number "%d" (0x98806c), shirt byte +0x14 or the row + 1. Label 0x804100 style 0x30, text flags 0xc, font 1, text pale grey `[0xae3184]`. Box colour dark blue `[0xaebe04]` for rows 0..10, else `[0xad9c68]` = RGB(0, 0, 0x40) (made at 0x415f97);
  - column 2: the name `0x53e140(..., style 1, ...)`, style 1, text flags 1 (left), font 2. Text colour yellow when `0x600a50(club, 0, 1, 0, 0)` is not 0, else pale grey for rows 0..10 and grey after. A link 0x7d7 when the staff still matches (`0x53e990`);
  - column 3: the club name (club + 0x38), yellow, font 2, text flags 1, link 0x7d5.
- One marker per slot, 0x4166a8..0x4168b4:
  - place: `0x8ad780(slot mask, playing area, &x, &y, 0x5b2e80(...))`;
  - colours: `0x53b640(club, kit 1, &fg, &bg)`. With no club: fg pale grey, bg dark blue `[0xaebe04]`;
  - circle: `0x55f990(1, x-13, y-18, x+13, y+8, 0, 0, style 0x10030, bg, bg, text flags 0, font 1, fg, "<shirt number>", 0, 0, 0, -1)` at 0x4167e4. A filled ellipse with a bevel, 27 x 27 px, with the shirt number centred;
  - name: `0x53e140(..., style 5, ...)`, then `0x804030(x-35, y+6, x+35, y+16, style 1, colour 0, text flags 0xc, font 1, yellow, name, 0x7d7, staff)` at 0x41685d, or `0x803fd0` with the same box when the staff no longer matches.
- Heading line: `0x803fd0(110, top, 470, top+35, 2, 0, 0xc, font 3, pale cyan, year text, 1)`, then top += 40. Year buttons "<< %s" and "%s >>" in a 2-column area (530, top-65)-(780, top-45): `0x8040a0` style 0x30, colour grey, text flags 0xc, font 1, text pale grey, ids 4 and 5, or labels with text flags 0x2c (emboss) at the end of the range.

### Tactics screen markers (0x899ea0, loop 0x89e8c2..0x89ebc5)

The marker is drawn inline in the build. 0x8aa9d0 is not a draw function (it is Tactics_Screen::get_slot_pitch_position).

- Place: `0x8ad780` as above, on the playing area of the pitch call at 0x89e434.
- Colours, set up at 0x899ea0 start and 0x89e4a6..0x89e4d5:
  - outfield: bg = kit background, fg = kit foreground from `0x8a8330` → `0x53b640(club, kit byte this+0xdeb, ...)` (decompile line `FUN_008a8330(&DAT_00de1f64,2000,&local_8b4,&local_8bc)`). A red club gives a red circle;
  - goalkeeper (mask bit 1): bg `[0xde1e18]` = RGB(0, 0x90, 0), or yellow when `0x540520(...) == 5` (meaning: I don't know); fg black `[0xaebdea]`;
  - a pending change (`0x8ab670` out flag): bg = high half of `[0xde1d0c]` = RGB(0, 0x40, 0), fg and name colour `[0xde1f32]` = RGB(0, 0x80, 0).
- Text: `0x8aa200(slot, buf, 2000, 0, 0)` gives the squad number (via 0x85a760) or the slot + 1.
- Circle: `0x55f990(0x246 or 1, x-13, y-18, x+13, y+8, 0, 0, 0x10030, bg, bg, 0, font 1, fg, number, 0, id 0x1d, data slot, -1)` at 0x89ea87 / 0x89eae4. Type 0x246 makes it a drag button when the tactic is not locked (this+0xe40 == 0). Then an outline ellipse 0x10020 type 0x346, id 0x1e, same box.
- A second position (`0x5b3460` flag): a filled ellipse (x-17, y-22)-(x+9, y+4) behind it with no text (0x89ea29).
- A custom slot (`0x8a9fa0`): a black ring 0x10020 at (x-14, y-19)-(x+14, y+9) (0x89eb42).
- Name: `0x8aa550(slot, buf, 2000, 1, 5, 0)`, then `0x55f990(1, x-45, y+6, x+45, y+16, 0, 0, style 1, 0, 0, text flags 0xc, font 1, yellow, name, ..., -1)` at 0x89ebb3. The name is not a link here.

Same marker on both screens: a 27 px kit-coloured circle with the number in font 1, and the name below it in yellow font 1 in a 10 px high box (70 px wide on the award page, 90 px on the tactics screen).

## 3. The original comparison screen

Build `0x876520`, input `0x889c30`, pushed by `0x870650` and the profile input at 0x888fcc. Full layout with addresses: `docs/research/2026-10-16-comparison-screen-inventory.md` (I did not read 0x876520 again in this session). Summary for the Compare tab:

- Heading bar: area (100, 10)-(790, 70) flags 0x30 red `[0xae31a8]`. Text "<p1> ~ <p2>" (0xa859d0), names as links 0x7d7, font 7 (font 6 when wider than 690 px), pale grey, by comp_text draw `0x4aeaf0` (0x8766bb..0x876819).
- Age and nationality line: (110, 80)-(780, 110), font 3, yellow.
- Attribute table: area (110, 115)-(780, y_bottom) flags 2, then the same box with 6 columns, weights 3,2,3,2,3,2, rows = height / font_height(2), flags 1 (0x879a1b, 0x879a4f). Classic: 13 rows.
  - name cell: `0x804100(area, c, row, 2, 0, 1, font 2, pale grey, "  %s")`;
  - value cell: one number, `abs(p1 - p2)` as " %d" (0xa85738), text flags 0xc, font 2. Colour: P1 colour `[0xae31aa]` (green) when p1 is higher, Equal colour yellow `[0xaebdec]` when equal, P2 colour red `[0xae31a8]` when p2 is higher. Neither player's own value shows;
  - masked: " -" in orange `[0xaebdd4]`;
  - extra rows: Preferred Foot, Avg. Rating, Morale, On Loan, Goals or Conceded, Appearances, Assists, Man of Match, orange, "%s - %s".
  - 39 cells, dealt round robin to the three column pairs, sorted by name with qsort 0x945333.
- Position suitability grid: (110, 361)-(780, 451), 4 columns, 5 rows, name and stars ("  *" to "  *****", "  -", "  n/a"), font 2, in P1, Equal or P2 colour.
- Legend: (110, 451)-(780, 487): "Legend:" pale grey, the masked note in orange.
- Summary: (110, 487)-(780, 505), 3 columns: "<p1> better" green, "Both the same" yellow, "<p2> better" red, font 2, text flags 0xc.
- Positions line: `0x803fd0(110, 515, 780, 545, 2, 0, 0xc, font 3, pale cyan, "<pos1> ~ <pos2>", 1)`.
- Side bar `0x75db00(4, 0)`, Back bar `0x5edcc0`, background `0x41c790(p1, 0, 0)`.
- Caution: the build calls `0x8b1750(p1, 0)` (training manager), which uses the random function. A copy of the look must not call the build itself.

## 4. The tactics screen player list (0x899ea0, 0x89c819 on)

- Panel: `0x55fba0(110, 145, 440, 545, 1, 0, 1, 0, flags 2, 0, -1)`.
- Grid: `0x55fba0(110, 153, 440, 537, 9 columns, weights, 20 rows, 0, flags 1, 0, -1)`. With a heading row: (110, 145)-(440, 545) and 21 rows, plus a heading label in column 5 (style 0x10, black, text flags 0xc, font 1, pale grey).
- Row height: 384/20 = 19.2 px, or 400/21 = 19.0 px with the heading.
- A second area on the same box, 2 columns weights 14, 112 (`local_8ce`), holds the row drag items (type 0x102 and 0x302, id 0x16).
- Column weights (bytes `local_8f4` set at the build start): 14, 1, 5, 1, 60, 15, 16, 7, 7. The view (node slot 10, `local_929`) merges columns:
  - views 1, 3, 4, 5: column 6 = 30, columns 7 and 8 = 0;
  - view 2: column 7 = 14, column 8 = 0;
  - view 6: no change;
  - other views: column 5 = 45, columns 6 to 8 = 0.
- Column 0, the number box: `0x55f990(type 0x42 or 1, 0,0,0,0, col 0, row, style 0x30, box, box, text flags 0xc, font 1, pale grey, "<number>", 0, id 0x19, data row, area)`. Box colour dark blue `[0xaebe04]` (0, 0, 128) for rows 0..10, else `[0xde1f36]` = RGB(0, 0, 0x40). Type 0x42 (drag button) when the tactic is not locked. Rows past the squad get a label `0x804100(area, 0, row, 0x30, box, 0x2c or 0xc, font 1, pale grey, text)`.
- Column 2, cards: yellow card `0x804100(..., 0x20030, yellow, 0xc, 1, black, 0)`, red card `0x40030` or `0x20030` with red `[0xae31a8]`.
- Column 4, the name: `0x8aa550(row, buf, 2000, 1, 1, 0)`, then `0x55f990(0x202, 0,0,0,0, col 4, row, style 1, 0, 0, text flags 1 (left), font 2, colour, name, 0, id 0x7d7 (0x7dc for a national team), data player, area)`. Colour: pale grey `[0xae3184]` on the pitch, grey `[0xae317e]` off it, pale cyan `[0xaebde8]` for the man of the match (`0x8ab360`).
- Column 5 in views 2 and 6: penalty and set piece marks, font 1, text flags 0xc; yellow for the first five takers, grey after.
- Column 6: view 1 the positions text `0x53ee50(..., 1)` (font 1, text flags 1); views 3 and 5 a stat from `0x7c25b0`, view 4 recent ratings `0x7c1660` (text flags 0xc). Colour grey off the pitch, yellow on it. In views 2 and 6, `0x8aadb0` match events ("inj", "off", "sub", "on" minute) in silver, or orange `[0xaebdd4]` for "on".
- Column 7: view 2 the condition "%d%%" (0x98e8d4) from `0x8aafd0`, text flags 0xc, font 1, grey off the pitch and yellow on it. View 6 the match rating, font 2, pale cyan, and goals in column 8, yellow.
- I found no orange percentage on this screen. The orange "88%" condition is in the squad list (columns inventory 2026-10-14, `[0xaebdd4]` with "%d%%").

## Spec references

- Text and fonts spec: fonts 1 to 7 only, field +0x76 is the font and +0x78 the text colour, text flags order (box, flags, font, colour, text, cursor), no T2K path.
- Area and widget draw spec: the ellipse style 0x10000 (`FUN_005e7840`) is out of scope of the C port in `src/si`. The recompiled runtime runs the original widget draw, so the ellipse draws there. Inference: the `src/si` port cannot show the pitch circles or the markers.
- The spec keeps the clip rule: a fill or line that is partly off screen draws nothing. A pitch rectangle must stay inside the screen.
