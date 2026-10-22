# Picture fill (0.6.0)

Status: In build on `claude/picture-fill`, 2026-10-21.

## Problem

A player on the champman0102 forum has a 1366x768 laptop at 100 % (Windows 10, AMD Vega 3, version 0.5.0). In widescreen the game shrank the picture to about 87 % with nearest pixels, so the text was small and its letters were uneven. In classic mode a maximised window held the picture 1.16 times, but the game kept it at 800x600 with black bars. On main after the Windows DPI fix, full screen in classic also stayed at 800x600 with bars, at a fit of 1.28.

## Solution

The picture fills the window at any size, larger or smaller, and keeps its shape. Bars show only where the shape of the window differs from the shape of the picture. Letters keep an even shape at every size. The window cannot get smaller than the picture. Classic mode gets a real 800x600 full screen, as the original has. On a short screen, widescreen tells the player that classic mode suits the screen better.

## User stories

1. As a player on a 1366x768 laptop, I want classic in a maximised window to fill the height of the screen, so that the text is bigger.
2. As a player on a 1366x768 laptop, I want classic in full screen to change the screen to 800x600, so that the monitor fills the screen as it did with the original.
3. As a player on a Mac, I want classic in full screen to fill the screen, because a Mac screen has no 800x600 mode.
4. As a player in widescreen on a screen less than 800 pixels high, I want the game to tell me that classic mode suits my screen, so that I get bigger text.
5. As a player who resizes the window, I want the picture to follow the window at every size, with no bars unless the shape differs.
6. As a player with `sharp_picture` on, I want even letters at sizes that are not a whole number of times the picture.
7. As a player with `sharp_picture` off, I want a smooth picture with no uneven letters.
8. As a player with a remembered window smaller than the picture, I want the window to grow to the smallest size at start.
9. As a player who sends a display report, I want it to say how the game scales the picture and whether full screen uses the 800x600 mode.

## Decisions

Max, 2026-10-21. Decisions 1 to 4 came with the task, Q5 to Q12 from the grilling.

1. The picture scales up or down freely and fills the window, keeping its shape. Bars show only where the shape differs. This replaces Q6 of `2026-10-20-windows-dpi-spec.md` (Windows scale 1 with bars from 1 to 2 times) and the rule of `sharp_picture` that used whole pixels when they lost at most a fifth of the fit.
2. The smallest window is 800x600 in classic and 1280x800 in widescreen.
3. Classic gets a real full screen at 800x600, a change of the screen's mode as in the original. The monitor or the graphics chip then scales it.
4. Widescreen tells the player to use classic when the screen is less than 800 pixels high.
5. The filter is "sharp bilinear". The game draws its picture with nearest pixels into a work texture of a whole number of times its size, the smallest whole number that is at least the fit. The work texture goes to the window with linear filtering. At a fit of 1 or less there is no work texture, and the picture goes to the window with linear filtering. At a fit that is a whole number the result is exact. The whole number goes down when the work texture would be larger than the renderer's largest texture.
6. `sharp_picture` stays an option, on by default. On gives sharp bilinear. Off gives plain linear filtering, a smooth and softer picture. Off no longer gives nearest pixels, because nearest pixels give uneven letters.
7. The smallest window is the size of a new window: the picture's size, in real pixels on Windows (the size that the DPI fix gives a new window) and in points on a Mac and on Linux. It is never larger than the usable part of the screen, with the same shape. Widescreen on a 1366x768 screen gets about 1110x694. The game sets it again when the window moves to another screen.
8. Full screen (F11 on Windows and Linux, Cmd+F on a Mac) in classic changes the screen to 800x600 when the screen lists that mode. Otherwise it uses the desktop size with the picture filled. Windows and Linux PCs normally list 800x600. The Mac of the project lists no 800x600 mode (its smallest is 1024x640), so a Mac uses the desktop size. A remembered full screen at start follows the same rule.
9. Widescreen full screen uses the desktop size with the picture filled. A 16:9 screen shows narrow bars at the sides, because the picture is 16:10.
10. When the player switches away from the 800x600 full screen (Alt+Tab), the game minimises and the desktop goes back to its own size. This is SDL's default. After a lost Direct3D device the game draws its picture again, and after a device reset it makes its textures again.
11. The widescreen hint. On a screen less than 800 real pixels high, in widescreen, the game shows a message box once, at the first start on such a screen: "Your screen is less than 800 pixels high. Classic mode fits it better. Choose it in Game Options, Display mode, then start the game again." The file `hints.txt` in the user folder, beside `window.txt`, keeps the line `short_screen_hint = shown`. It is not in `settings.txt`, because it is not an option. The Display mode row of Bosman Edition Options says "Next start. Classic suits this screen" on such a screen, every time. The height is that of the desktop mode of the window's screen: real pixels on Windows, points on a Mac.
12. A remembered window smaller than the smallest size grows to it at start. SDL does this when the game sets the smallest size.

The display report shows the way the picture is scaled ("sharp: 2 times with nearest pixels, then linear" or "linear"), full screen "in an 800x600 mode" when the mode changed, and, in widescreen, a line when the screen is less than 800 pixels high. Under SDL's dummy video driver (the smoke runs) there is no screen, so there is no hint.

The drawing does not touch the game's memory, so the game's decisions stay the same (Q152).

## Testing

- `winscale` (ctest): the fit, the whole number of the work texture (with the texture limit), the rule for the 800x600 mode and the rule for the hint, with the cases of the forum player.
- `tests/window/test_window.py`: the smallest window in classic and widescreen, a resize below it, a remembered window that grows, the fill of the picture with bars on one side only (from the display report), sharp bilinear and linear, the hint once, `hints.txt` and the display report line. The test gives a screen height with `RECOMP_TEST_SCREEN_H` and never goes to full screen, so it does not change the screen of the person at the computer.
- `tests/window/test_display_report.py`: the report at start, on the key and at quit, with the way of scaling.
- The smoke runs in classic and widescreen.
- The script step `shot` now reads the window before the present, because the back buffer was empty after it on a Mac (Metal).
- `test_window.py` also reads a shot of classic at 1000x700: black bars at the sides and a drawn picture between them.
- Nobody can run Windows on the project's Mac. The cross-build must have no warnings. Manual checks for the 0.6.0 beta on Windows: the 800x600 full screen, Alt+Tab from it and back, the forum player's laptop, and whether SDL 2.32 with `SDL_WINDOWS_DPI_SCALING=1` gives the desktop mode in real pixels at 125 % (the "desktop mode" of the display report shows it). The hint and the 800x600 check depend on that answer.

## Out of scope

- The size of the text in widescreen (open item 0 of the handover).
- A full screen mode change in widescreen.
- The text of the site's `sharp_picture` line: the release slice updates it with the release.

## Open decisions

None.

## Slices

### 1. Picture fill, smallest window, classic full screen, widescreen hint
Blocked by: none
Delivers: every decision above.
Merge danger: two-way door. Blast radius: the picture of every player on every system. A wrong filter or size shows a soft, small or black picture, and a wrong mode change can leave a Windows screen at 800x600 until the game quits. The game's decisions do not change. Anything a player sees triggers the review rule: one Opus review.
- [x] `winscale` passes.
- [x] `test_window.py` and `test_display_report.py` pass on the Mac.
- [x] The Mac build has no warnings. The Windows cross-build of the changed files has no warnings. The full `.exe` does not link on main at 79fec25 because of a fault outside this change (`season_divisions_ita.c:349` defines `unlink`, which MinGW's `stdio.h` declares).
- [x] ctest passes (44 of 44). The Mac smoke runs match main: 1, 3, 4 and 5 pass, and 2-setup differs from `smoke-ref` by 2.6 % (classic) and 2.0 % (widescreen) on main too, because the reference is older than the Setup Game strip.
- [x] One Opus review: no must-fix. Its four should-fix items and four nits are fixed or moved to the manual checks above.
