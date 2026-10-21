/* Runtime draw code for the Team views screen (spec 2026-10-15-features-0.4.0, slice 3): lines in any
 * direction, filled polygons and a football pitch. The original has no diagonal line, no polygon fill
 * and no picture of a pitch, so these are new.
 *
 * The functions draw RGB565 pixels into a buffer in host memory. They know nothing of the game: views.c
 * gives them the game's draw surface. Every coordinate is a pixel of the buffer, ends included, and
 * every function clips to the buffer. */
#ifndef RECOMP_DRAW_H
#define RECOMP_DRAW_H

#include <stdint.h>

typedef struct {
    uint8_t *pixels;        /* the first pixel of the first row */
    uint32_t pitch;         /* bytes from one row to the next */
    int w, h;
} DrawTarget;

/* The colours of the pitch: two greens for the stripes and the white of the lines. */
#define DRAW_PITCH_GRASS 0x2c64u
#define DRAW_PITCH_STRIPE 0x3485u
#define DRAW_PITCH_LINE 0xdefbu

/* One pixel, with alpha 0 (none) to 256 (full colour) over the pixel that is there. */
void draw_pixel(const DrawTarget *t, int x, int y, uint16_t colour, int alpha);

/* A line from (x1, y1) to (x2, y2), one pixel wide, both ends included. */
void draw_line(const DrawTarget *t, int x1, int y1, int x2, int y2, uint16_t colour);

/* The outline of a circle of radius r around (cx, cy). */
void draw_circle(const DrawTarget *t, int cx, int cy, int r, uint16_t colour);

/* A filled polygon of n corners (x, y pairs in xy). A pixel is inside when its centre is inside (the
 * even-odd rule), so two polygons that share an edge do not overlap. alpha as for draw_pixel. */
void draw_polygon(const DrawTarget *t, const int *xy, int n, uint16_t colour, int alpha);

/* A pitch that fills the box x1, y1, x2, y2, upright: one goal at the top, one at the bottom. Grass in
 * stripes, then the lines: touch lines and goal lines, halfway line, centre circle and spot, both
 * penalty areas, goal areas and penalty spots. */
void draw_pitch(const DrawTarget *t, int x1, int y1, int x2, int y2);

/* A five-pointed star of radius r around (cx, cy): fill 0 empty (in empty), 1 the left half in colour, 2 full
 * in colour (Best XI, Q420). */
void draw_star(const DrawTarget *t, int cx, int cy, int r, int fill, uint16_t colour, uint16_t empty);

#endif
