/* Clickable names (issues #11 and #18) and the original's mouse cursors.
 *
 * The original already puts a button widget on each name of a person or club: the news text gets an
 * empty button over each linked name (comp_text.cpp, 0x4aeaf0), and lists such as the squad give the
 * name's own widget an action id and the record. A click goes through the game's own handler (0x7643d0),
 * which opens the profile, so Back works. Over such a widget the hit test sets cursor kind 2 (0x9c36e8),
 * and the original shows its cursor resource 0x67. See docs/research/2026-10-14-clickable-names-inventory.md.
 *
 * This file adds two things and changes nothing in the game's memory:
 * - win_cursor_shape gives the system cursor for each cursor handle of the game. Always on.
 * - with clickable_names = on, a line under the name under the mouse. The line goes on the frame that the
 *   runtime shows (and on the screen dumps), never into the game's screen. */
#include "win.h"
#include "options.h"

#include <SDL.h>

#define DISPLAY 0xb77c94u                   /* screen manager 0: scrman 0xb74450 + 0x3844 */
#define WIDGETS (DISPLAY + 0xba95eu)
#define WIDGET_SIZE 0x18cu
#define MAX_WIDGETS 1200
#define AREA_SIZE 0xbf1u
#define CURSOR_KIND 0x9c36e8u               /* set by the hit test 0x560490: 2 over a widget with an action */
#define MOUSE_X 0xb67a20u
#define MOUSE_Y 0xb67a14u

#define CURSOR_BASE 0x00030000u             /* LoadCursorA in user32.c: CURSOR_BASE + resource id */

/* The cursors of the original (loader 0x5e2650): resource 0x65 normal, 0x67 over a widget with an
 * action, 0x69 busy. The Windows cursors IDC_ARROW (0x7f00), IDC_IBEAM (0x7f01) and IDC_WAIT (0x7f02)
 * are the fallbacks when a resource does not load. */
int win_cursor_shape(uint32_t handle)
{
    switch (handle - CURSOR_BASE) {
    case 0x67: return SDL_SYSTEM_CURSOR_HAND;
    case 0x69: case 0x7f02: return SDL_SYSTEM_CURSOR_WAIT;
    case 0x7f01: return SDL_SYSTEM_CURSOR_IBEAM;
    default: return SDL_SYSTEM_CURSOR_ARROW;
    }
}

static uint32_t widget(int i) { return WIDGETS + WIDGET_SIZE * (uint32_t)i; }
static int s16at(uint32_t a) { return (int16_t)rd16(g_mem, a); }
static int s32at(uint32_t a) { return (int32_t)rd32(g_mem, a); }

/* Where the widget draw put the text of each widget: the first and last x of the text, and the start of
 * the text, so that a later change of the text is seen. */
typedef struct { int set, x1, x2; char text[24]; } TextPlace;
static TextPlace places[MAX_WIDGETS];
static uint32_t drawn_widget;               /* the widget that 0x5ee590 draws, or 0 */

/* What each widget's box looked like when the game drew it: a hash of its pixels. A box or popup that the
 * game paints over a name later changes those pixels, while the hit test still finds the name under it
 * (the goal box of the match screen does). Then the name gets no line (Q150). */
static uint32_t drawn_hash[MAX_WIDGETS];
static uint8_t hashed[MAX_WIDGETS];
#define DRAW_SURFACE 0xaebde4u              /* the surface that the draw functions of the original draw on */

static int widget_index(uint32_t w)
{
    if (w < WIDGETS || w >= WIDGETS + WIDGET_SIZE * MAX_WIDGETS || (w - WIDGETS) % WIDGET_SIZE) return -1;
    return (int)((w - WIDGETS) / WIDGET_SIZE);
}

void o_005ee590(void);
void o_005e6d80(void);
void o_0055f7c0(void);
void foreign_forget(void);

/* The reset of a Display to empty (0x55f7c0, thiscall, 1 stack argument): the game does it when it builds
 * a screen again. The pictures that widget i showed on the old screen are gone with it: a new widget i
 * that the game has not drawn yet must not be compared with the old hash (Q150). */
RT_WRAP(0055f7c0, display_reset)
{
    if (cpu.ecx == DISPLAY) {
        foreign_forget();                   /* the rules of the foreign player colour (foreign.c) */
        memset(hashed, 0, sizeof hashed);
        memset(drawn_hash, 0, sizeof drawn_hash);
        for (int i = 0; i < MAX_WIDGETS; i++) places[i].set = 0;
    }
    o_0055f7c0();
}

/* FNV-1a over the pixels of the box x1..x2, y1..y2 of a surface; 0 when the box is not on it. */
static uint32_t box_hash(uint32_t px, uint32_t pitch, int x1, int y1, int x2, int y2)
{
    if (!px || x1 < 0 || y1 < 0 || x2 >= (int)rt_screen_w || y2 >= (int)rt_screen_h || x1 > x2 || y1 > y2) return 0;
    uint32_t h = 2166136261u;
    for (int y = y1; y <= y2; y++) {
        const uint8_t *row = g_mem + px + (uint32_t)y * pitch + (uint32_t)x1 * 2;
        for (int k = 0; k < (x2 - x1 + 1) * 2; k++) h = (h ^ row[k]) * 16777619u;
    }
    return h | 1;
}

/* The widget draw, thiscall on the widget, one argument. */
RT_WRAP(005ee590, widget_draw)
{
    uint32_t outer = drawn_widget;
    drawn_widget = cpu.ecx;
    o_005ee590();
    int i = widget_index(drawn_widget);
    uint32_t surface = rd32(g_mem, DRAW_SURFACE);
    if (i >= 0 && surface) {
        uint32_t w = drawn_widget;
        drawn_hash[i] = box_hash(rd32(g_mem, surface + 0x14), rd32(g_mem, surface + 0x10), s32at(w + 0x10),
                                 s32at(w + 0x14), s32at(w + 0x18), s32at(w + 0x1c));
        hashed[i] = 1;
    }
    views_widget_drawn(drawn_widget);
    drawn_widget = outer;
}

/* The width of text in a font, by the game's own function 0x5e5ad0 (font, text). */
static int text_width(int font, uint32_t text)
{
    Cpu saved = cpu;
    uint32_t args[2] = {(uint32_t)font, text};
    uint32_t w = rt_callback(0x5e5ad0u, 2, args);
    cpu = saved;
    return (int32_t)w;
}

/* The text box 0x5e6d80, cdecl: (x1, y1, x2, y2, flags, font, colour, text, cursor). For the text of a
 * widget of screen manager 0 it keeps where one line of text lands, with the alignment rules of the
 * text box (flag 1 left, flag 0x40 right, else centred). Text on more than one line is not kept.
 *
 * Same game (Q152): it measures with clickable_names off too. The measure goes through the original's text
 * engine, which keeps work memory for the glyphs it meets (docs/research/2026-10-16-hd-text.md), so a measure
 * in only one state moved the game's memory blocks between clickable_names on and off (a heap trace showed
 * one block made at another moment; same-save yeovil.sav with the menu of 0.6.0 differed in Friendly
 * League_453.tmp). Only the underline reads the places, and only with the option on. */
RT_WRAP(005e6d80, text_box)
{
    int i = drawn_widget ? widget_index(drawn_widget) : -1;
    uint32_t text = rt_arg(7);
    if (i >= 0 && text) {
        TextPlace *p = &places[i];
        int x1 = (int32_t)rt_arg(0), x2 = (int32_t)rt_arg(2), flags = (int)rt_arg(4);
        int font = (int16_t)rt_arg(5);
        const char *s = win_str(text);
        p->set = 0;
        if (*s && !strchr(s, '\n') && !(flags & 0x80)) {
            int w = text_width(font, text), box = x2 - x1 + 1;
            if (w > 0 && w <= box) {
                p->x1 = (flags & 1) ? x1 : (flags & 0x40) ? x2 - w + 1 : (box - w) / 2 + x1;
                p->x2 = p->x1 + w - 1;
                snprintf(p->text, sizeof p->text, "%s", s);
                p->set = 1;
            }
        }
    }
    o_005e6d80();
}

/* The underline that the frame shows: on, x1..x2 at row y, in an RGB565 colour, for widget i. */
static struct { int on, x1, x2, y, i; uint16_t colour; } line;

/* True for the action ids that open a person's or a club's profile (0x7643d0): 7d5 club, 7d6 club
 * reserves, 7d7, 7d8, 7da..7dd and 7e7 a staff profile at different pages. */
static int name_action(uint32_t id)
{
    return (id >= 0x7d5 && id <= 0x7d8) || (id >= 0x7da && id <= 0x7dd) || id == 0x7e7;
}

static int inside(uint32_t w, int x, int y)
{
    return s32at(w + 0x10) <= x && x <= s32at(w + 0x18) && s32at(w + 0x14) <= y && y <= s32at(w + 0x1c);
}

/* The widget that the hit test 0x560490 would take under (x, y): the last button in its order, first the
 * widgets with no area, then the rows in view of each open area that is not a popup. -1 for none. */
static int widget_under(int x, int y)
{
    int hit = -1;
    int free_count = s16at(DISPLAY + 0x12f4f8);
    for (int k = 0; k < free_count && k < MAX_WIDGETS; k++) {
        int i = s16at(DISPLAY + 0x12eb96 + 2 * (uint32_t)k);
        if (i >= 0 && i < MAX_WIDGETS && (rd32(g_mem, widget(i) + 0xc) & 2) && inside(widget(i), x, y)) hit = i;
    }
    int area_count = s16at(DISPLAY + 0x12f4f6);
    for (int k = 0; k < area_count && k < 250; k++) {
        int a_index = s16at(DISPLAY + 0x12e9a2 + 2 * (uint32_t)k);
        if (a_index < 0 || a_index >= 250) continue;
        uint32_t a = DISPLAY + 4 + AREA_SIZE * (uint32_t)a_index;
        if (!rd32(g_mem, a + 0xbe9) || s16at(a + 0x20e) != -1) continue;
        int first = s16at(a + 0xb76), count = s16at(a + 0xb74), rows = g_mem[a + 0xbac];
#define ENTRY(n) ((n) == -1 ? s16at(a + 0x210) : s16at(a + 0x212 + 2 * (uint32_t)(n)))
        int first_widget = ENTRY(first);
        if (first_widget < 0 || first_widget >= MAX_WIDGETS) continue;
        for (int n = first; n < count && n < 1200; n++) {
            int i = ENTRY(n);
            if (i < 0 || i >= MAX_WIDGETS) break;
            if (s32at(widget(i) + 0x24) - s32at(widget(first_widget) + 0x24) > rows - 1) break;
            if ((rd32(g_mem, widget(i) + 0xc) & 2) && inside(widget(i), x, y)) hit = i;
        }
#undef ENTRY
    }
    return hit;
}

/* The text colour of the label under an empty link widget: the news text is a label per line, with an
 * empty button on each name (0x4aeaf0). */
static uint16_t label_colour(int link)
{
    uint32_t w = widget(link);
    int x = (s32at(w + 0x10) + s32at(w + 0x18)) / 2, y = (s32at(w + 0x14) + s32at(w + 0x1c)) / 2;
    int area = s16at(w + 0x7a), count = s16at(DISPLAY + 0x12e9a0);
    for (int i = 0; i < count && i < MAX_WIDGETS; i++) {
        uint32_t l = widget(i);
        if (i != link && s16at(l + 0x7a) == area && (rd32(g_mem, l + 0xc) & 1) && g_mem[l + 0x80] && inside(l, x, y))
            return rd16(g_mem, l + 0x78);
    }
    return 0xffff;
}

uint32_t win_primary_pixels(void);
extern int win_screen_dirty;

/* True when the shown picture of widget i differs from the picture that the game drew for it: something
 * covers it. A widget with no hash (not drawn yet) counts as not covered. */
static int covered(int i)
{
    if (i < 0 || i >= MAX_WIDGETS || !hashed[i]) return 0;
    uint32_t w = widget(i);
    uint32_t now = box_hash(win_primary_pixels(), rt_screen_w * 2, s32at(w + 0x10), s32at(w + 0x14), s32at(w + 0x18),
                            s32at(w + 0x1c));
    return now != drawn_hash[i];
}

/* The row for the line: two pixels under the lowest row of x1..x2 that holds a pixel of the text colour,
 * inside the widget. Without such a pixel, the bottom row of the widget. */
static int line_row(uint32_t w, int x1, int x2, uint16_t colour)
{
    int y1 = s32at(w + 0x14), y2 = s32at(w + 0x1c), low = -1;
    uint32_t px = win_primary_pixels();
    if (px && x1 >= 0 && x2 < (int)rt_screen_w && y1 >= 0 && y2 < (int)rt_screen_h)
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                if (rd16(g_mem, px + 2 * ((uint32_t)y * rt_screen_w + (uint32_t)x)) == colour) { low = y; break; }
    if (low < 0) return y2;
    return low + 2 < y2 ? low + 2 : y2;
}

/* The x extent of a link over a name in running text. The news text can put more than one empty link on
 * one name, for example one on the whole name and one on the surname. The extent joins every empty link
 * on the same row with the same action and the same record, so that the whole name gets one line. */
static void link_extent(int link, int *x1, int *x2)
{
    uint32_t w = widget(link);
    *x1 = s32at(w + 0x10);
    *x2 = s32at(w + 0x18);
    int count = s16at(DISPLAY + 0x12e9a0);
    for (int i = 0; i < count && i < MAX_WIDGETS; i++) {
        uint32_t o = widget(i);
        if (i == link || !(rd32(g_mem, o + 0xc) & 2) || g_mem[o + 0x80]) continue;
        if (rd32(g_mem, o + 8) != rd32(g_mem, w + 8) || rd32(g_mem, o + 0x48) != rd32(g_mem, w + 0x48)) continue;
        if (s16at(o + 0x7a) != s16at(w + 0x7a) || s32at(o + 0x14) != s32at(w + 0x14)) continue;
        if (s32at(o + 0x18) < *x1 - 1 || s32at(o + 0x10) > *x2 + 1) continue;   /* not touching */
        if (s32at(o + 0x10) < *x1) *x1 = s32at(o + 0x10);
        if (s32at(o + 0x18) > *x2) *x2 = s32at(o + 0x18);
    }
}

/* Moves x1 and x2 in to the first and last columns that hold a pixel of the text colour inside the
 * widget, so that the line does not start under the space before a name. No change without such a pixel. */
static void trim_to_text(uint32_t w, int *x1, int *x2, uint16_t colour)
{
    int y1 = s32at(w + 0x14), y2 = s32at(w + 0x1c), first = -1, last = -1;
    uint32_t px = win_primary_pixels();
    if (!px || *x1 < 0 || *x2 >= (int)rt_screen_w || y1 < 0 || y2 >= (int)rt_screen_h) return;
    for (int x = *x1; x <= *x2; x++)
        for (int y = y1; y <= y2; y++)
            if (rd16(g_mem, px + 2 * ((uint32_t)y * rt_screen_w + (uint32_t)x)) == colour) {
                if (first < 0) first = x;
                last = x;
                break;
            }
    if (first < 0) return;
    /* A full stop or comma after the name: a last run of columns whose pixels all sit in the bottom
     * third of the text. Letters reach higher, so they stay. */
    int top = y2, bottom = y1;
    for (int x = first; x <= last; x++)
        for (int y = y1; y <= y2; y++)
            if (rd16(g_mem, px + 2 * ((uint32_t)y * rt_screen_w + (uint32_t)x)) == colour) {
                if (y < top) top = y;
                if (y > bottom) bottom = y;
            }
    int low = bottom - (bottom - top) / 3, x = last, high = 0;
    for (; x >= first; x--) {
        int any = 0;
        for (int y = y1; y <= y2; y++)
            if (rd16(g_mem, px + 2 * ((uint32_t)y * rt_screen_w + (uint32_t)x)) == colour) {
                any = 1;
                if (y < low) high = 1;
            }
        if (!any) break;                           /* the gap before the last run */
    }
    if (!high && x > first) {                      /* drop the low run and the gap before it */
        while (x > first) {
            int any = 0;
            for (int y = y1; y <= y2 && !any; y++)
                any = rd16(g_mem, px + 2 * ((uint32_t)y * rt_screen_w + (uint32_t)x)) == colour;
            if (any) break;
            x--;
        }
        last = x;
    }
    *x1 = first;
    *x2 = last;
}

/* Called after each input poll. Finds the name under the mouse and the line under it. A change marks the
 * frame for the next present. */
void names_after_poll(void)
{
    /* Test runs only: RECOMP_TEST_COVER="x1 y1 x2 y2" paints a red box on the shown screen after the game
     * draws, as the goal box of the match screen does (tests/smoke/name_cover_run.py). */
    const char *cover = getenv("RECOMP_TEST_COVER");
    int c[4];
    uint32_t shown = win_primary_pixels();
    if (cover && shown && sscanf(cover, "%d %d %d %d", &c[0], &c[1], &c[2], &c[3]) == 4)
        for (int cy = c[1]; cy <= c[3] && cy < (int)rt_screen_h; cy++)
            for (int cx = c[0]; cx <= c[2] && cx < (int)rt_screen_w; cx++)
                wr16(g_mem, shown + 2 * ((uint32_t)cy * rt_screen_w + (uint32_t)cx), 0xf800);
    int on = 0, x1 = 0, x2 = 0, y = 0;
    uint16_t colour = 0;
    if (options_bool(OPT_CLICKABLE_NAMES) && rd32(g_mem, CURSOR_KIND) == 2 && !rd16(g_mem, DISPLAY)) {
        int i = widget_under(s32at(MOUSE_X), s32at(MOUSE_Y));
        uint32_t w = i >= 0 ? widget(i) : 0;
        if (w && name_action(rd32(g_mem, w + 8)) && rd32(g_mem, w + 0x38) == 1 && rd32(g_mem, w + 0x48) &&
            !covered(i)) {
            const char *text = win_str(w + 0x80);
            if (!*text) {                                  /* a link over a name in running text */
                link_extent(i, &x1, &x2);
                colour = label_colour(i);
                trim_to_text(w, &x1, &x2, colour);
                on = 1;
            } else if (places[i].set && !strncmp(places[i].text, text, sizeof places[i].text - 1)) {
                x1 = places[i].x1;                         /* a row: under the text only */
                x2 = places[i].x2;
                colour = rd16(g_mem, w + 0x78);
                on = 1;
            }
            if (on) y = line_row(w, x1, x2, colour);
            line.i = i;
        }
    }
    if (on != line.on || (on && (x1 != line.x1 || x2 != line.x2 || y != line.y || colour != line.colour))) {
        line.on = on;
        line.x1 = x1;
        line.x2 = x2;
        line.y = y;
        line.colour = colour;
        win_screen_dirty = 1;
    }
}

static void rgb(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((c >> 11) * 255 / 31);
    *g = (uint8_t)(((c >> 5) & 0x3f) * 255 / 63);
    *b = (uint8_t)((c & 0x1f) * 255 / 31);
}

/* Draws the line on the frame, after the game's picture. */
void names_draw(SDL_Renderer *renderer)
{
    if (!line.on || covered(line.i)) return;
    uint8_t r, g, b;
    rgb(line.colour, &r, &g, &b);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect rect = {line.x1, line.y, line.x2 - line.x1 + 1, 1};
    SDL_RenderFillRect(renderer, &rect);
}

/* Draws the line on a copy of the screen for a dump. */
void names_draw_surface(SDL_Surface *s)
{
    if (!line.on || !s || covered(line.i)) return;
    if (getenv("RECOMP_SCREEN_LOG")) fprintf(stderr, "recomp: name line in dump: x %d..%d, y %d\n", line.x1, line.x2, line.y);
    uint8_t r, g, b;
    rgb(line.colour, &r, &g, &b);
    SDL_Rect rect = {line.x1, line.y, line.x2 - line.x1 + 1, 1};
    SDL_FillRect(s, &rect, SDL_MapRGB(s->format, r, g, b));
}
