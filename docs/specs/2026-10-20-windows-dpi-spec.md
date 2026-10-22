# Windows display scaling (0.6.0)

Status: built on `claude/windows-dpi`, 2026-10-20.

## Problem

Two players on the champman0102 forum saw blurred text on Windows laptops. One of them set Windows display scaling from 125 % to 100 % and the text became sharp (2026-10-20). The game exe declared no DPI awareness, so Windows drew the game at 100 % and stretched the whole window to 125 %.

## Decisions (Max, 2026-10-20)

1. The game sets `SDL_WINDOWS_DPI_SCALING=1` before `SDL_Init` (`src/recomp_rt/win/main.c`). This turns on per-monitor awareness. Window sizes and places stay in scaled points, as on a Mac. `window.txt` from older versions, the first window size, the window fit (`winfit.h`), full screen and the mouse keep their units. Only the renderer output is in real pixels.
2. A new window on Windows opens at the picture's size in real pixels, with no bars (Max, Q7, replaces the first answer): 1280x800 pixels in widescreen, which is 1024x640 points at 125 %. Points round up, so at 150 % the window is 1281 pixels wide (`winfit_points`, `winfit.h`). A window remembered in `window.txt` keeps its size.
3. Replaced on 2026-10-21 by `2026-10-21-picture-fill-spec.md`: the picture now fills the window on every system. The old decision: with `sharp_picture` on, on Windows only, a window that holds the picture from 1 up to 2 times shows it at scale 1 with bars (Max, Q6, `winscale.h`), as a player at 100 % sees it. The first answer, a smooth fill, was dropped: a review showed that it looks as blurred as the Windows stretch. Mac and Linux keep the old rule. The rule also applies at 100 % and in full screen: a 1080p screen at 100 % in full screen holds widescreen 1.35 times and now shows it at scale 1 with bars, where it filled the screen with nearest pixels before.
4. A window smaller than the picture stays out of this change (nearest scaling, as before).
5. No separate test build. The 0.6.0 public beta is the first Windows test. The display report shows "Game handles the Windows scale: per screen" and the output size in pixels.

## Tests

- `winscale` (ctest): the whole-pixel rule with and without the Windows band.
- `winfit` (ctest): `winfit_points` at 100 %, 125 %, 150 % and 175 %.
- Cross-build of `cm0102_recomp.exe` with no warnings. Nobody can run it on Windows before the beta.
