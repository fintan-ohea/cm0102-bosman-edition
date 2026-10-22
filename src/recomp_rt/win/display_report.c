/* The display report: a text file of facts about the screens, the window and the scaling of the picture,
 * for a player to send when the picture looks wrong (blurred text, a small picture in a big window).
 *
 * The report goes to display-report.txt in the user folder, beside settings.txt. The game writes it at the
 * start, when the window is open, and at quit. The key Ctrl+Shift+D (Cmd+Shift+D on macOS) writes it again,
 * copies it to the clipboard and shows a message with the path of the file (user32.c, shortcut_key).
 *
 * The report holds the last REPORT_EVENTS changes of the window (size, maximise, full screen, screen), each
 * with the scale that it gave, so that it shows what happened when the player maximised the window.
 *
 * Paths show "<user>" in place of the account name. The report holds no machine name and no serial number.
 * The facts that SDL does not give (the Windows scale, the Linux desktop session) come from the host
 * (host_display_notes). */
#include "win.h"
#include "host.h"
#include "options.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CM0102_VERSION
#define CM0102_VERSION "dev"
#endif

#define REPORT_EVENTS 10
#define REPORT_ROOM 16384

typedef struct {
    uint64_t ms;                        /* since SDL_Init */
    char line[320];
} Event;
static Event events[REPORT_EVENTS];
static int event_count;                 /* every event so far; the last REPORT_EVENTS are kept */

typedef struct { char *buf; size_t room, len; } Text;

static void add(Text *t, const char *fmt, ...)
{
    if (t->len >= t->room) return;
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(t->buf + t->len, t->room - t->len, fmt, ap);
    va_end(ap);
    if (k > 0) t->len += (size_t)k;
    if (t->len > t->room) t->len = t->room;
}

/* The path with "<user>" for the last part of the home folder: /Users/max/x gives /Users/<user>/x. */
static void hide_user(const char *path, char *out, size_t room)
{
    char home[1024];
    size_t n = 0;
    if (host_home_dir(home, sizeof home)) n = strlen(home);
    while (n > 1 && home[n - 1] == '/') home[--n] = 0;     /* a home folder with a slash at the end */
#ifdef _WIN32
    int differs = n && _strnicmp(path, home, n);           /* Windows keeps the case the player typed */
#else
    int differs = n && strncmp(path, home, n);
#endif
    if (!n || differs || (path[n] && path[n] != '/')) {
        snprintf(out, room, "%s", path);
        return;
    }
    const char *slash = strrchr(home, '/');
    int keep = slash ? (int)(slash - home) : 0;
    snprintf(out, room, "%.*s/<user>%s", keep, home, path + n);
}

static const char *scale_mode_name(SDL_ScaleMode m)
{
    return m == SDL_ScaleModeNearest ? "nearest" : m == SDL_ScaleModeLinear ? "linear" : "best";
}

/* One line on the window and the picture in it: the window size in points and in pixels, the size of the
 * picture as drawn, and its scale. */
static void window_line(Text *t)
{
    SDL_Window *w = win_sdl_window();
    SDL_Renderer *r = win_sdl_renderer();
    if (!w || !r) { add(t, "no window\n"); return; }
    int ww, wh, pw = 0, ph = 0, ow = 0, oh = 0;
    SDL_GetWindowSize(w, &ww, &wh);
    SDL_GetRendererOutputSize(r, &ow, &oh);
    SDL_GL_GetDrawableSize(w, &pw, &ph);
    SDL_Rect vp;
    float sx = 1, sy = 1;
    SDL_RenderGetViewport(r, &vp);
    SDL_RenderGetScale(r, &sx, &sy);
    Uint32 f = SDL_GetWindowFlags(w);
    char way[80];
    win_picture_way(way, sizeof way);
    add(t, "window %dx%d, output %dx%d pixels, picture %dx%d drawn at %dx%d (scale %.3f, %s)%s%s\n", ww, wh, ow, oh,
        (int)rt_screen_w, (int)rt_screen_h, (int)(vp.w * sx + 0.5f), (int)(vp.h * sy + 0.5f), sx, way,
        (f & SDL_WINDOW_MAXIMIZED) ? ", maximised" : "",
        (f & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP ? ", full screen"
        : (f & SDL_WINDOW_FULLSCREEN) ? ", full screen in an 800x600 mode" : "");
}

void display_report_event(const char *what)
{
    if (!win_sdl_window()) return;
    char line[300];
    Text t = {line, sizeof line, 0};
    window_line(&t);
    Event *e = &events[event_count % REPORT_EVENTS];
    e->ms = SDL_GetTicks64();
    snprintf(e->line, sizeof e->line, "%s: %s", what, line);
    event_count++;
}

/* The whole report in buf. */
static void build(char *buf, size_t room, const char *when)
{
    Text t = {buf, room, 0};
    char line[2048];
    time_t now = time(NULL);
    struct tm tm;
    host_localtime(now, &tm);
    add(&t, "CM 01/02: Bosman Edition display report\n");
    add(&t, "Version %s, made %04d-%02d-%02d %02d:%02d (%s)\n\n", CM0102_VERSION, tm.tm_year + 1900,
        tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, when);

    host_system_name(line, sizeof line);
    add(&t, "System: %s", line);
    SDL_version v;
    SDL_GetVersion(&v);
    add(&t, "SDL %d.%d.%d, video driver %s\n", v.major, v.minor, v.patch,
        SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");
    SDL_RendererInfo ri;
    SDL_Renderer *r = win_sdl_renderer();
    if (r && !SDL_GetRendererInfo(r, &ri))
        add(&t, "Renderer %s%s, largest texture %dx%d\n", ri.name,
            (ri.flags & SDL_RENDERER_ACCELERATED) ? " (graphics card)" : " (software)", ri.max_texture_width,
            ri.max_texture_height);

    add(&t, "\nSettings: display = %s, sharp_picture = %s, remember_window = %s, shortcuts = %s\n",
        options_string(OPT_DISPLAY), options_bool(OPT_SHARP_PICTURE) ? "on" : "off",
        options_bool(OPT_REMEMBER_WINDOW) ? "on" : "off", options_bool(OPT_SHORTCUTS) ? "on" : "off");
    SDL_Texture *tex = win_sdl_texture();
    SDL_ScaleMode sm;
    if (tex && !SDL_GetTextureScaleMode(tex, &sm)) add(&t, "Picture filter: %s\n", scale_mode_name(sm));
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    if (hint) add(&t, "SDL scale quality hint: %s\n", hint);
    if (win_short_screen()) add(&t, "Screen less than 800 pixels high: classic mode suits it better\n");

    SDL_Window *w = win_sdl_window();
    int on = w ? SDL_GetWindowDisplayIndex(w) : -1;
    add(&t, "\nScreens (SDL):\n");
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect b, u;
        SDL_DisplayMode dm;
        float dd = 0, hd = 0, vd = 0;
        SDL_GetDisplayBounds(i, &b);
        SDL_GetDisplayUsableBounds(i, &u);
        if (SDL_GetDesktopDisplayMode(i, &dm)) memset(&dm, 0, sizeof dm);
        if (SDL_GetDisplayDPI(i, &dd, &hd, &vd)) dd = hd = vd = 0;
        add(&t, "  %d%s: %s, %dx%d at (%d, %d), usable %dx%d, desktop mode %dx%d %d Hz, DPI %.0f x %.0f\n", i + 1,
            i == on ? " (the window is here)" : "", SDL_GetDisplayName(i) ? SDL_GetDisplayName(i) : "?", b.w, b.h,
            b.x, b.y, u.w, u.h, dm.w, dm.h, dm.refresh_rate, hd, vd);
    }
    host_display_notes(line, sizeof line);
    if (line[0]) add(&t, "\n%s", line);

    add(&t, "\nNow: ");
    window_line(&t);
    int first = event_count > REPORT_EVENTS ? event_count - REPORT_EVENTS : 0;
    add(&t, "\nLast window changes (%d in all):\n", event_count);
    for (int i = first; i < event_count; i++) {
        Event *e = &events[i % REPORT_EVENTS];
        add(&t, "  %7.1f s  %s", (double)e->ms / 1000.0, e->line);
    }

    char path[1100];
    hide_user(win_user_dir, path, sizeof path);
    add(&t, "\nSave folder: %s\n", path);
    hide_user(win_game_dir, path, sizeof path);
    add(&t, "Game folder: %s\n", path);
}

int display_report_write(const char *when, char *path, size_t room, char **text)
{
    static char buf[REPORT_ROOM];
    build(buf, sizeof buf, when);
    if (text) *text = buf;
    snprintf(path, room, "%s/display-report.txt", win_user_dir);
    FILE *f = host_fopen(path, "w");
    if (!f) { fprintf(stderr, "recomp: cannot write %s\n", path); return 0; }
    fputs(buf, f);
    fclose(f);
    fprintf(stderr, "recomp: display report written (%s)\n", when);
    return 1;
}

void display_report_key(void)
{
    char path[1100], shown[1100], msg[1400];
    char *text = NULL;
    display_report_event("report key");
    int ok = display_report_write("on the report key", path, sizeof path, &text);
    int copied = text && !SDL_SetClipboardText(text);
    hide_user(path, shown, sizeof shown);
    if (ok)
        snprintf(msg, sizeof msg, "The display report is saved to:\n%s\n\n%s", shown,
                 copied ? "It is also on the clipboard. Paste it into your forum post or GitHub issue."
                        : "Attach this file to your forum post or GitHub issue.");
    else
        snprintf(msg, sizeof msg, "The display report could not be saved to %s.%s", shown,
                 copied ? "\n\nIt is on the clipboard. Paste it into your forum post or GitHub issue." : "");
    if (getenv("RECOMP_SCRIPT")) return;     /* runs without a person get no message box */
    SDL_ShowSimpleMessageBox(ok ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_WARNING, "Display report", msg,
                             win_sdl_window());
}
