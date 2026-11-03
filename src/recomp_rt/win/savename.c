/* Save names (#34): Save Game suggests "Club YYYY-MM-DD" from the player's club and the game date.
 * The player can edit the name in the text entry as before.
 *
 * Save Game (side bar handler, menu item 0x3fe at 0x763eda) copies the last save name, game options
 * + 0x113, into the name buffer 0xdd6ed4 (0x763f2d..0x763f5f). It then calls the file dialog
 * FUN_0058f500 (cdecl, 14 arguments) at 0x763fb3 with the buffer as argument 5 and the save routine
 * 0x838e10 as argument 6. The wrap writes the suggested name into the buffer when the dialog is
 * opened with that buffer and that routine. Other dialogs (printouts, scout searches) do not change.
 *
 * The club: the manager of the player slot that has the turn (0xb74c26 + slot * 0x300, slot at
 * 0xb77c7a), club pointer at manager + 0x39, short name at club + 0x38, long name at club + 4. The side
 * bar reads the same fields at 0x762ff0..0x763017. A manager with no club keeps the original name.
 *
 * The club name is in code page 1252 (club.dat). The path code passes a name to the host as it is, and
 * a host path is UTF-8, so the suggestion is converted to UTF-8 (cp1252.h). Without that a club like
 * Köln or Málaga fails to save. */
#include "win.h"
#include "cp1252.h"

#include <stdio.h>
#include <string.h>

#define GAME_DATE 0xae2c90u        /* s16 day of the year from 0, s16 year */
#define PLAYER_MANAGER 0xb74c26u   /* per player slot, 0x300 bytes apart: the manager */
#define PLAYER_SLOT 0xb77c7au      /* s16: the player slot that has the turn */
#define NAME_BUFFER 0xdd6ed4u      /* the text of the name entry of the file dialog */
#define SAVE_ROUTINE 0x00838e10u   /* the save callback of Save Game */
#define CLUB_MAX 24                /* characters of the club name in the suggestion */
#define CLUB_MAX_EDITED 15         /* with " (edited)" (editor.c, fork): the name is no longer than without it */
#define CLUB_BYTES 49              /* its bytes as UTF-8 (cp1252.h), with the end */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* The game date: day of the year from 0 and year at GAME_DATE, as year, month and day of the month
 * from 1. */
int win_game_date(int *year, int *month, int *day)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11 && d >= days[m] + (m == 1 && leap(y))) d -= days[m] + (m == 1 && leap(y)), m++;
    *year = y, *month = m + 1, *day = d + 1;
    return 1;
}

/* The suggested name in out, or 0 when the player has no club. */
static int suggest(char *out, size_t size)
{
    uint32_t slot = (uint32_t)(int16_t)rd16(g_mem, PLAYER_SLOT);
    uint32_t manager = rd32(g_mem, PLAYER_MANAGER + slot * 0x300u);
    uint32_t club = manager ? rd32(g_mem, manager + 0x39) : 0;
    if (!club) return 0;
    char name[CLUB_BYTES];
    int edited = editor_game_edited(), most = edited ? CLUB_MAX_EDITED : CLUB_MAX;
    cp1252_file_name(name, sizeof name, (const char *)(g_mem + club + 0x38), most);
    if (!name[0]) cp1252_file_name(name, sizeof name, (const char *)(g_mem + club + 4), most);
    if (!name[0]) return 0;

    int year, month, day;
    win_game_date(&year, &month, &day);
    snprintf(out, size, "%s %04d-%02d-%02d%s", name, year, month, day, edited ? " (edited)" : "");
    return 1;
}

void o_0058f500(void);

/* The file dialog, cdecl (14 arguments). */
RT_WRAP(0058f500, file_dialog)
{
    char name[80];
    if (rt_arg(5) == NAME_BUFFER && rt_arg(6) == SAVE_ROUTINE && suggest(name, sizeof name))
        memcpy(g_mem + NAME_BUFFER, name, strlen(name) + 1);
    o_0058f500();
}
