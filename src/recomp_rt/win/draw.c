/* Runtime draw code for the Team views screen: see draw.h. */
#include "draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void draw_pixel(const DrawTarget *t, int x, int y, uint16_t colour, int alpha)
{
    if (x < 0 || y < 0 || x >= t->w || y >= t->h || alpha <= 0) return;
    uint8_t *p = t->pixels + (uint32_t)y * t->pitch + (uint32_t)x * 2;
    if (alpha < 256) {
        uint16_t old;
        memcpy(&old, p, 2);
        int r = (((colour >> 11) & 31) * alpha + ((old >> 11) & 31) * (256 - alpha)) >> 8;
        int g = (((colour >> 5) & 63) * alpha + ((old >> 5) & 63) * (256 - alpha)) >> 8;
        int b = ((colour & 31) * alpha + (old & 31) * (256 - alpha)) >> 8;
        colour = (uint16_t)(r << 11 | g << 5 | b);
    }
    memcpy(p, &colour, 2);
}

/* Bresenham: one pixel for each step along the longer axis. */
void draw_line(const DrawTarget *t, int x1, int y1, int x2, int y2, uint16_t colour)
{
    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        draw_pixel(t, x1, y1, colour, 256);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/* The midpoint circle: eight points for each step of one octant. */
void draw_circle(const DrawTarget *t, int cx, int cy, int r, uint16_t colour)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        static const int sign[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for (int i = 0; i < 4; i++) {
            draw_pixel(t, cx + sign[i][0] * x, cy + sign[i][1] * y, colour, 256);
            draw_pixel(t, cx + sign[i][0] * y, cy + sign[i][1] * x, colour, 256);
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

/* A scan line through the centre of each pixel row: the edges cross it at xs, and the pixels whose
 * centres lie between the first and second crossing, the third and fourth, and so on, are inside. */
void draw_polygon(const DrawTarget *t, const int *xy, int n, uint16_t colour, int alpha)
{
    if (n < 3) return;
    int top = xy[1], bottom = xy[1];
    for (int i = 1; i < n; i++) {
        if (xy[2 * i + 1] < top) top = xy[2 * i + 1];
        if (xy[2 * i + 1] > bottom) bottom = xy[2 * i + 1];
    }
    if (top < 0) top = 0;
    if (bottom > t->h) bottom = t->h;
    double *xs = malloc(sizeof *xs * (size_t)n);
    if (!xs) return;
    for (int y = top; y < bottom; y++) {
        double yc = y + 0.5;
        int k = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            double x0 = xy[2 * i], y0 = xy[2 * i + 1], x1 = xy[2 * j], y1 = xy[2 * j + 1];
            if ((y0 > yc) != (y1 > yc)) xs[k++] = x0 + (yc - y0) * (x1 - x0) / (y1 - y0);
        }
        qsort(xs, (size_t)k, sizeof *xs, cmp_double);
        for (int i = 0; i + 1 < k; i += 2) {
            int from = (int)ceil(xs[i] - 0.5), to = (int)ceil(xs[i + 1] - 0.5);
            if (from < 0) from = 0;
            if (to > t->w) to = t->w;
            for (int x = from; x < to; x++) draw_pixel(t, x, y, colour, alpha);
        }
    }
    free(xs);
}

static void box(const DrawTarget *t, int x1, int y1, int x2, int y2, uint16_t colour)
{
    draw_line(t, x1, y1, x2, y1, colour);
    draw_line(t, x2, y1, x2, y2, colour);
    draw_line(t, x2, y2, x1, y2, colour);
    draw_line(t, x1, y2, x1, y1, colour);
}

/* The sizes of a pitch in metres: 68 wide and 105 long. */
#define PITCH_W 68.0
#define PITCH_L 105.0

void draw_pitch(const DrawTarget *t, int x1, int y1, int x2, int y2)
{
    int w = x2 - x1, h = y2 - y1;
    if (w < 8 || h < 8) return;
    double mx = w / PITCH_W, my = h / PITCH_L;
    int cx = x1 + w / 2, cy = y1 + h / 2;

    /* Grass: 12 stripes across the pitch. */
    for (int s = 0; s < 12; s++) {
        int top = y1 + h * s / 12, bottom = y1 + h * (s + 1) / 12;
        int corners[] = {x1, top, x2 + 1, top, x2 + 1, bottom, x1, bottom};
        draw_polygon(t, corners, 4, s & 1 ? DRAW_PITCH_STRIPE : DRAW_PITCH_GRASS, 256);
    }
    int last[] = {x1, y2, x2 + 1, y2, x2 + 1, y2 + 1, x1, y2 + 1};
    draw_polygon(t, last, 4, DRAW_PITCH_STRIPE, 256);

    box(t, x1, y1, x2, y2, DRAW_PITCH_LINE);
    draw_line(t, x1, cy, x2, cy, DRAW_PITCH_LINE);
    draw_circle(t, cx, cy, (int)lround(9.15 * (mx < my ? mx : my)), DRAW_PITCH_LINE);
    draw_pixel(t, cx, cy, DRAW_PITCH_LINE, 256);

    int area_w = (int)lround(40.3 / 2 * mx), area_d = (int)lround(16.5 * my);
    int goal_w = (int)lround(18.3 / 2 * mx), goal_d = (int)lround(5.5 * my);
    int spot = (int)lround(11.0 * my);
    box(t, cx - area_w, y1, cx + area_w, y1 + area_d, DRAW_PITCH_LINE);
    box(t, cx - area_w, y2 - area_d, cx + area_w, y2, DRAW_PITCH_LINE);
    box(t, cx - goal_w, y1, cx + goal_w, y1 + goal_d, DRAW_PITCH_LINE);
    box(t, cx - goal_w, y2 - goal_d, cx + goal_w, y2, DRAW_PITCH_LINE);
    draw_pixel(t, cx, y1 + spot, DRAW_PITCH_LINE, 256);
    draw_pixel(t, cx, y2 - spot, DRAW_PITCH_LINE, 256);
}

/* A five-pointed star of radius r around (cx, cy). fill: 0 empty, 1 the left half, 2 full. */
void draw_star(const DrawTarget *t, int cx, int cy, int r, int fill, uint16_t colour, uint16_t empty)
{
    static const int unit[20] = {0, -100, 22, -31, 95, -31, 36, 12, 59, 81, 0, 38, -59, 81, -36, 12, -95, -31, -22, -31};
    int xy[20];
    for (int i = 0; i < 20; i += 2) {
        xy[i] = cx + unit[i] * r / 100;
        xy[i + 1] = cy + 1 + unit[i + 1] * r / 100;    /* the points reach 0.81 r down: one pixel lower */
    }
    draw_polygon(t, xy, 10, fill == 2 ? colour : empty, 256);
    if (fill == 1 && cx > 0) {
        DrawTarget left = *t;                      /* the left half: a target that ends at the middle */
        if (left.w > cx) left.w = cx;
        draw_polygon(&left, xy, 10, colour, 256);
    }
}
