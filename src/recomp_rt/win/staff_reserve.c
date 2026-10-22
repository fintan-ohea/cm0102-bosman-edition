/* The reserve of blank staff rows (0.6.0, tools/recomp/staff_reserve.py).
 *
 * A database load reserves 23,000 blank staff rows (0x528540). A new game fills them from the top down with
 * the extra coaches, scouts, physios and managers that database_generate_extra_non_playing_staff 0x527240
 * makes for the clubs of the selected nations. With every league selected, some Update Team databases need
 * more than 23,000, and the game stops at the error box "Database.cpp 1844" (0x527e4c). The research is in
 * docs/research/2026-10-17-select-all-databases.md.
 *
 * The official data sets keep 23,000, so the base game allocates and plays as before. A custom data set
 * (database = folder:<name>) keeps 23,000 too, unless the estimate of its Select All need from its files is more
 * than 23,000: then it gets RESERVE_CUSTOM rows. The same estimate runs on the folder that an official choice
 * reads (the game's own Data, or Data-3.9.60 and Data-3.9.65), because players also copy a database into the
 * game's own Data folder (decision O207: the Update Team's November 2007 database then needs 24,218 and stopped
 * at the box). There only a valid estimate above 23,000 gives RESERVE_CUSTOM: the 3.9.68 files estimate 22,986,
 * 3.9.60 and 3.9.65 about 20,700, so the official data keep 23,000 and the base game is unchanged. The test switch RECOMP_STAFF_RESERVE=<rows> sets any
 * size from 23,000 up. RECOMP_SCREEN_LOG=1 prints how many rows a new game used. */
#include "options.h"
#include "season_records.h"
#include "win.h"
#include "../host.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define RESERVE_BASE 23000u
#define RESERVE_CUSTOM 46000u    /* twice the original: the season databases need up to 27,156 */
#define RESERVE_MAX 400000u

#define STAFF_COUNT 0xae2364u   /* g_staff_count */
#define STAFF_TABLE 0xae23bcu   /* g_staff_table, 0x6e bytes a row */
#define STAFF_ROW 0x6eu
#define MANAGER_ROWS 16u        /* the last rows of the table belong to the human managers (0x52a9d3) */
#define CLUB_JOB 0x3du         /* 8, 9 and 10 from loops 1 to 3 of 0x527240, 5 from loop 4 */
#define ROW_STATE 0x5fu         /* 6 in a blank reserve row (0x52a1c2 loop), 1 in a generated row */

/* The Select All need, estimated from the files of a custom data set (docs/research/2026-10-17-select-all-databases.md,
 * "The estimate"). It counts the four loops of 0x527240 on the file records, with every one of the 27 nations of
 * the league table selected (0xb63d60, names from the key id matcher 0x60d8da..0x60ee33):
 * - a club of England with reputation (+0x80) 500 or less: none (0x527334..0x527340, and the same test at
 *   0x527600, 0x527901, 0x527bf9),
 * - loops 1 to 3, a club of a selected nation (+0x53): clamp((rep + 500) * k / 10000, 1, k) less the filled slots,
 *   with k = 5, 7, 3 slots at +0x19f, +0x1b3, +0x1cf. The test through 0x540a50 is left out (it can only lower
 *   the count),
 * - loop 4: one manager for a club with no manager (+0xcf), in a selected nation with a division (+0x57) and a
 *   reputation, or with reputation 7501 or more; and one for each national team (nat_club.dat) with none.
 * A file value of -1 is an empty slot. Before 0x527240 runs, the game clears some manager links that the files
 * hold (the staff pass, database_links.c database_leave_club and the national manager, and the trim 0x524d70),
 * so loop 4 can need more than the files show: up to 9 % more in the 22 data sets that were measured. The
 * estimate therefore adds a margin of LOOP4_MARGIN_PCT % of the loop 4 count. */
static const char *const league_nations[] = {
    "Argentina", "Australia", "Belgium", "Brazil", "Croatia", "Denmark", "England", "Finland", "France", "Germany",
    "Greece", "Holland", "Republic Of Ireland", "Italy", "Japan", "Northern Ireland", "Norway", "Poland", "Portugal",
    "Russia", "Scotland", "South Korea", "Spain", "Sweden", "Turkey", "United States", "Wales"};
enum { NATION_REC = 290, CLUB_REC = 0x245, NAME_LEN = 52, MAX_NATIONS = 1024, LEAGUE_NATIONS = 27,
       LOOP4_MARGIN_PCT = 10 };

/* The whole file <dir>/<name> (any case) in a new buffer, or NULL. */
static uint8_t *read_data_file(const char *dir, const char *name, size_t *size)
{
    char found[256] = "", path[2048];
    HostDir *d = host_opendir(dir);
    if (!d) return NULL;
    const char *e;
    while ((e = host_readdir(d)))
        if (!strcasecmp(e, name)) { snprintf(found, sizeof found, "%s", e); break; }
    host_closedir(d);
    if (!found[0]) return NULL;
    snprintf(path, sizeof path, "%s/%s", dir, found);
    FILE *f = host_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *size = buf ? (size_t)n : 0;
    return buf;
}

static int32_t get32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }
static int get16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }

static int per_club(int rep, int k)            /* loop 1 from 0x527429, loops 2 and 3 from 0x52772c, 0x527a2d */
{
    int n = (rep + 500) * k / 10000;
    return n < 2 ? 1 : n > k - 1 ? k : n;
}

/* The estimate, or -1 when a file is missing, a file size is not a whole number of records, or fewer than the 27
 * league nations match: the caller then gives the large reserve. */
static long staff_reserve_estimate(const char *dir)
{
    size_t nn, nc, nk;
    uint8_t *na = read_data_file(dir, "nation.dat", &nn), *cl = read_data_file(dir, "club.dat", &nc),
            *nk_ = read_data_file(dir, "nat_club.dat", &nk);
    long need = -1, loop4 = 0;
    if (na && cl && nk_ && nn % NATION_REC == 0 && nc % CLUB_REC == 0 && nk % CLUB_REC == 0) {
        int32_t sel[64], eng = -2;
        int nsel = 0;
        for (size_t i = 0; i + NATION_REC <= nn && i / NATION_REC < MAX_NATIONS; i += NATION_REC) {
            char name[NAME_LEN + 1];
            memcpy(name, na + i + 4, NAME_LEN);
            name[NAME_LEN] = 0;
            for (size_t j = 0; j < sizeof league_nations / sizeof *league_nations; j++)
                if (!strcasecmp(name, league_nations[j]) && nsel < 64) {
                    sel[nsel++] = get32(na + i);
                    if (!strcasecmp(name, "England")) eng = get32(na + i);
                }
        }
        if (nsel < LEAGUE_NATIONS) goto out;
        static const int offs[3] = {0x19f, 0x1b3, 0x1cf}, ks[3] = {5, 7, 3};
        need = 0;
        for (size_t i = 0; i + CLUB_REC <= nc; i += CLUB_REC) {
            const uint8_t *c = cl + i;
            int rep = get16(c + 0x80);
            int32_t nation = get32(c + 0x53);
            if (nation == eng && rep <= 500) continue;
            int selected = 0;
            for (int j = 0; j < nsel; j++) selected |= sel[j] == nation;
            if (selected)
                for (int l = 0; l < 3; l++) {
                    int filled = 0;
                    for (int k = 0; k < ks[l]; k++) filled += get32(c + offs[l] + 4 * k) != -1;
                    if (per_club(rep, ks[l]) > filled) need += per_club(rep, ks[l]) - filled;
                }
            if (get32(c + 0xcf) == -1 && ((selected && get32(c + 0x57) != -1 && rep != 0) || rep >= 7501)) loop4++;
        }
        for (size_t i = 0; i + CLUB_REC <= nk; i += CLUB_REC)
            if (!(get32(nk_ + i + 0x53) == eng && get16(nk_ + i + 0x80) <= 500) && get32(nk_ + i + 0xcf) == -1) loop4++;
        need += loop4 + (loop4 * LOOP4_MARGIN_PCT + 99) / 100;
    }
out:
    free(na);
    free(cl);
    free(nk_);
    return need;
}

uint32_t rt_staff_reserve_size(void)
{
    uint32_t r = RESERVE_BASE;
    if (options_enum(OPT_DATABASE) == DATABASE_CUSTOM && win_data_set_dir()[0]) {
        long need = staff_reserve_estimate(win_data_set_dir());
        if (need < 0 || need > (long)RESERVE_BASE) r = RESERVE_CUSTOM;
        if (getenv("RECOMP_SCREEN_LOG"))
            fprintf(stderr, "recomp: staff reserve estimate %ld for Select All, reserve %u\n", need, r);
    } else if (options_enum(OPT_DATABASE) != DATABASE_CUSTOM) {
        /* The folder of an official choice: the game's own Data, or Data-3.9.60 / Data-3.9.65. */
        char dir[1200];
        if (win_data_set_dir()[0]) snprintf(dir, sizeof dir, "%s", win_data_set_dir());
        else snprintf(dir, sizeof dir, "%s/Data", win_game_dir);
        long need = staff_reserve_estimate(dir);
        if (need > (long)RESERVE_BASE) r = RESERVE_CUSTOM;
        if (getenv("RECOMP_SCREEN_LOG"))
            fprintf(stderr, "recomp: staff reserve estimate %ld for Select All (the game's data), reserve %u\n",
                    need, r);
    }
    /* Slice 15b: 0x527240 makes up to 16 non-playing staff for each new club record of this load. */
    r += season_records_reserve_extra();
    const char *v = getenv("RECOMP_STAFF_RESERVE");
    if (v && *v) {
        long n = strtol(v, NULL, 10);
        if (n >= (long)RESERVE_BASE && n <= (long)RESERVE_MAX) r = (uint32_t)n;
        else fprintf(stderr, "recomp: RECOMP_STAFF_RESERVE=%s is not %u to %u rows\n", v, RESERVE_BASE, RESERVE_MAX);
    }
    rt_staff_reserve = r;
    return r;
}

/* database_generate_extra_non_playing_staff: the test log counts the reserve rows that it used. The wrap
 * only reads memory after the call. */
void o_00527240(void);
RT_WRAP(00527240, generate_extra_staff)
{
    o_00527240();
    if (!getenv("RECOMP_SCREEN_LOG")) return;
    uint32_t count = rd32(g_mem, STAFF_COUNT), table = rd32(g_mem, STAFF_TABLE), used = 0, job[4] = {0};
    for (uint32_t i = 1; i <= rt_staff_reserve && i + MANAGER_ROWS <= count; i++) {
        uint32_t row = table + (count - MANAGER_ROWS - i) * STAFF_ROW;
        if (g_mem[row + ROW_STATE] == 6) continue;
        used++;
        switch (g_mem[row + CLUB_JOB]) {     /* the job that each of the four loops gives */
        case 8: job[0]++; break;
        case 9: job[1]++; break;
        case 10: job[2]++; break;
        default: job[3]++; break;
        }
    }
    fprintf(stderr, "recomp: extra non-playing staff: %u of %u reserve rows used (loops: %u job 8, %u job 9, "
            "%u job 10, %u managers), result %u\n", used, rt_staff_reserve, job[0], job[1], job[2], job[3], cpu.eax);
}
