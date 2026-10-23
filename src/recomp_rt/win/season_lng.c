/* The .lng files fitted to the database (O260, guard review row A25).
 *
 * The game keeps the translated names in CM3_TEMP\<lang>.lng, one file for each of the ten languages. A data load
 * of a new game copies them from Data (database_load 0x528540: the copy loop 0x528618..0x5286c8, through the data
 * set's swap of Data, files.c), a save stores them (0x53158a) and a load extracts them from the save (0x5285f7).
 * Three functions read them by row: database_load_language_names 0x5339a0 writes the names of .lng row i into
 * record i (row [0xae23fc][club] for a club), eng_lng_load_tables 0x544fc0 makes the English name table, and
 * set_key_ids 0x601a40 matches the key records by the English names of eng.lng when the names of the data are in
 * another language (a load of a save made in one, 0x603c52..0x603c89). All three compare the counts in the header
 * with the database and give the box "Database..cpp 7784" (0x5339a0) or no names on a difference. The files of
 * the game fit the 3.9.68 data, and those of the data sets 3.9.60 and 3.9.65 fit those (make_clean_files.py). A
 * season or custom database has other counts and other rows, so its names stayed English; a club that the club
 * records add (season_records.c) has a row past the file.
 *
 * The fix: at the end of the data load of a new game (season_comps.c), while the records hold the English names
 * of the data and before the language names read the files (Config::apply_and_save 0x5d8480), each
 * CM3_TEMP\<lang>.lng that does not fit the database is written again in the database's row order. A club's row is
 * then its own index (+0x00; [0xae23fc] is made by the club trim later, database_trim_club_table 0x5267c0, which
 * gives each kept club its row before the trim, so the rows stay right: season_records.c then leaves them). The source of a row is the row of the copied file whose eng.lng name equals the record's name: the same
 * row number first, then the first row with that name that no record took. A record with no such row gets its own
 * names (it stays English in every language): a club that the club records add, for example. The new files then go into the save and come back at its load, so
 * every later read (a language change during a career, a load in another language, the key ids) reads rows that
 * fit the records. When the files fit (the 3.9.68 game, the data sets) nothing is written: the base game reads
 * the original files.
 *
 * The layout (found from the 3.9.68 data: each eng.lng entry equals fields of its record; sizes from the reads
 * 0x533eff..0x5350ad and 0x54521c..0x545355): a header of 8 s32 counts (clubs, continents, nation comps, nations,
 * cities, club comps, stadiums, staff comps), then the entries in this order: clubs 0x4f, continents 0xba, nation
 * comps 0x53, nations 0x6d, stadiums 0x34, cities 0x34, staff comps 0x4f, club comps 0x53. An entry holds the name
 * (record +4, 51 bytes) and the byte after it (record +0x37); the 0x4f and larger entries then the short name
 * (record +0x38, 26 bytes) and the byte after it (record +0x52); a comp entry then the abbreviation (record +0x53,
 * 4 bytes); a nation entry then 30 bytes from record +0x53. A continent with no row gets the eng.lng entry of its
 * index. The 0x5339a0 write of a comp entry whose short name is empty gives the record its long name as short
 * name (0x5355eb..0x535617), so a record with no short name of its own shows its long name there.
 *
 * The comp counts leave out the records that the new competitions module adds (season_comps_added_records): the
 * three readers run with them hidden (season_comps.c), and they have no translation in any file. */
#include "win.h"
#include "season_comps.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LNG_HEADER 32u
#define CLUB_ROW_TABLE 0xae23fcu
#define COMP_TOTAL 0xae2378u            /* s32: club comps + nation comps + 0x7f friendly records */
#define NATION_COMP_COUNT 0xae2380u     /* s32 */

typedef struct {
    const char *name;
    uint32_t array, count;      /* globals: the record array and its s32 count */
    uint32_t record;            /* record size */
    uint32_t entry;             /* .lng entry size */
    int header;                 /* index of the count in the header */
} Section;

/* In file order. The comps share one array (club comps, then nation comps); their counts come from comp_counts. */
enum { S_CLUBS, S_CONTINENTS, S_NATION_COMPS, S_NATIONS, S_STADIUMS, S_CITIES, S_STAFF_COMPS, S_CLUB_COMPS, SECTIONS };
static const Section sections[SECTIONS] = {
    {"clubs", 0xae23b4u, 0xae235cu, 0x245u, 0x4fu, 0},
    {"continents", 0xae23a4u, 0xae234cu, 0xc6u, 0xbau, 1},
    {"nation comps", 0xae23d0u, 0, 0x6bu, 0x53u, 2},
    {"nations", 0xae23a8u, 0xae2350u, 0x122u, 0x6du, 3},
    {"stadiums", 0xae23b0u, 0xae2358u, 0x4eu, 0x34u, 6},
    {"cities", 0xae23acu, 0xae2354u, 0x38u, 0x34u, 4},
    {"staff comps", 0xae23e0u, 0xae2388u, 0x65u, 0x4fu, 7},
    {"club comps", 0xae23d0u, 0, 0x6bu, 0x53u, 5},
};

/* The ten files (database_language_file_exists 0x535930). */
static const char *const files[] = {"eng.lng", "fra.lng", "ger.lng", "ita.lng", "esp.lng",
                                    "dan.lng", "hol.lng", "nor.lng", "por.lng", "swe.lng"};
#define FILES ((int)(sizeof files / sizeof files[0]))

/* ---- A .lng file ---- */

typedef struct {
    uint8_t *data;
    size_t size;
    int32_t count[8];
    size_t at[SECTIONS];        /* offset of each section */
    int *sorted[SECTIONS];      /* the rows of each section sorted by name */
} Lng;

static void lng_free(Lng *l)
{
    free(l->data);
    for (int s = 0; s < SECTIONS; s++) free(l->sorted[s]);
    memset(l, 0, sizeof *l);
}

/* The file in a folder, matched without regard to case; 0 when it is not there. */
static int find_in(const char *dir, const char *file, char *path, size_t room)
{
    HostDir *d = host_opendir(dir);                     /* UTF-8 paths: a user folder with accents (host.h) */
    if (!d) return 0;
    const char *e;
    path[0] = 0;
    while ((e = host_readdir(d)))
        if (!strcasecmp(e, file)) { snprintf(path, room, "%s/%s", dir, e); break; }
    host_closedir(d);
    return path[0] != 0;
}

static void name_of(const uint8_t *src, char *out)
{
    memcpy(out, src, 51);
    out[51] = 0;
}

static const Lng *sort_lng;
static int sort_section;

static int by_name(const void *a, const void *b)
{
    const uint8_t *e = sort_lng->data + sort_lng->at[sort_section];
    uint32_t size = sections[sort_section].entry;
    char x[52], y[52];
    name_of(e + (size_t)*(const int *)a * size, x);
    name_of(e + (size_t)*(const int *)b * size, y);
    int c = strcmp(x, y);
    return c ? c : *(const int *)a - *(const int *)b;
}

static int lng_read(const char *path, Lng *l, int sort)
{
    memset(l, 0, sizeof *l);
    FILE *f = host_fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    l->data = n > (long)LNG_HEADER ? malloc((size_t)n) : NULL;
    if (!l->data || fread(l->data, 1, (size_t)n, f) != (size_t)n) { fclose(f); lng_free(l); return 0; }
    fclose(f);
    l->size = (size_t)n;
    memcpy(l->count, l->data, sizeof l->count);
    size_t at = LNG_HEADER;
    for (int s = 0; s < SECTIONS; s++) {
        l->at[s] = at;
        if (l->count[sections[s].header] < 0) { lng_free(l); return 0; }
        at += (size_t)l->count[sections[s].header] * sections[s].entry;
    }
    if (at != l->size) { lng_free(l); return 0; }
    for (int s = 0; sort && s < SECTIONS; s++) {
        int c = l->count[sections[s].header];
        l->sorted[s] = malloc(sizeof(int) * (size_t)(c ? c : 1));
        for (int j = 0; j < c; j++) l->sorted[s][j] = j;
        sort_lng = l;
        sort_section = s;
        qsort(l->sorted[s], (size_t)c, sizeof(int), by_name);
    }
    return 1;
}

/* The eng.lng row of a name: the same row number first, then the first row with that name that no record took.
 * -1 for none. An empty name matches its own row only. */
static int row_of(const Lng *l, int s, int row, const char *name, const uint8_t *taken)
{
    uint32_t size = sections[s].entry;
    int n = l->count[sections[s].header];
    const uint8_t *e = l->data + l->at[s];
    char other[52];
    if (row < n && !taken[row]) {
        name_of(e + (size_t)row * size, other);
        if (!strcmp(other, name)) return row;
    }
    if (!name[0]) return -1;
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        name_of(e + (size_t)l->sorted[s][mid] * size, other);
        if (strcmp(other, name) < 0) lo = mid + 1; else hi = mid;
    }
    for (; lo < n; lo++) {
        int j = l->sorted[s][lo];
        name_of(e + (size_t)j * size, other);
        if (strcmp(other, name)) break;
        if (!taken[j]) return j;
    }
    return -1;
}

/* ---- The database side ---- */

/* The record of each .lng row of a section, 0 for none; extras: clubs that share a row with another club. */
typedef struct {
    int rows, extras;
    uint32_t *record;
} Rows;

static int comp_counts(int32_t *clubs, int32_t *nations)
{
    int32_t total = (int32_t)rd32(g_mem, COMP_TOTAL) - 0x7f - season_comps_added_records();
    int32_t n = (int32_t)rd32(g_mem, NATION_COMP_COUNT);
    if (total < 0 || n < 0 || n > total) return 0;
    *clubs = total - n;
    *nations = n;
    return 1;
}

static int rows_of(int s, Rows *r)
{
    const Section *sec = &sections[s];
    uint32_t base = rd32(g_mem, sec->array);
    int32_t n, first = 0;
    memset(r, 0, sizeof *r);
    if (s == S_CLUB_COMPS || s == S_NATION_COMPS) {
        int32_t c, nc;
        if (!comp_counts(&c, &nc)) return 0;
        n = s == S_CLUB_COMPS ? c : nc;
        first = s == S_CLUB_COMPS ? 0 : c;
    } else {
        n = (int32_t)rd32(g_mem, sec->count);
    }
    if (n < 0 || n > 400000 || (n && !base)) return 0;
    if (s != S_CLUBS) {
        r->rows = n;
        r->record = calloc((size_t)(n ? n : 1), sizeof *r->record);
        for (int32_t i = 0; i < n; i++) r->record[i] = base + sec->record * (uint32_t)(first + i);
        return 1;
    }
    uint32_t map = rd32(g_mem, CLUB_ROW_TABLE);
    int32_t rows = 0;
    for (int32_t i = 0; i < n; i++) {
        int32_t row = (int32_t)(map ? rd32(g_mem, map + 4u * (uint32_t)i) : rd32(g_mem, base + sec->record * (uint32_t)i));
        if (row < 0 || row >= 400000) return 0;
        if (row >= rows) rows = row + 1;
    }
    r->rows = rows;
    r->record = calloc((size_t)(rows ? rows : 1), sizeof *r->record);
    for (int32_t i = 0; i < n; i++) {
        uint32_t rec = base + sec->record * (uint32_t)i;
        int32_t row = (int32_t)(map ? rd32(g_mem, map + 4u * (uint32_t)i) : rd32(g_mem, rec));
        if (!r->record[row]) r->record[row] = rec;
        else r->extras++;
    }
    return 1;
}

/* An entry made from its record's own names. */
static void own_entry(int s, uint32_t rec, uint8_t *e)
{
    const Section *sec = &sections[s];
    memset(e, 0, sec->entry);
    memcpy(e, g_mem + rec + 4, 51);
    e[51] = g_mem[rec + 0x37];
    if (sec->entry >= 0x4f) {
        memcpy(e + 52, g_mem + rec + 0x38, 26);
        e[78] = g_mem[rec + 0x52];
    }
    if (sec->entry == 0x53) memcpy(e + 79, g_mem + rec + 0x53, 4);
    if (sec->entry == 0x6d) memcpy(e + 79, g_mem + rec + 0x53, 30);
}

/* ---- The fit ---- */

/* The source row of each record of each section (-1: its own names), from the copied eng.lng. */
static int *source_rows[SECTIONS];

static int match_rows(const Lng *eng, const Rows *rows)
{
    int matched = 0;
    for (int s = 0; s < SECTIONS; s++) {
        free(source_rows[s]);
        source_rows[s] = malloc(sizeof(int) * (size_t)(rows[s].rows ? rows[s].rows : 1));
        uint8_t *taken = calloc((size_t)eng->count[sections[s].header] + 1, 1);
        for (int i = 0; i < rows[s].rows; i++) {
            uint32_t rec = rows[s].record[i];
            int j = -1;
            if (rec) {
                char name[52];
                name_of(g_mem + rec + 4, name);
                j = row_of(eng, s, i, name, taken);
                if (j >= 0) taken[j] = 1, matched++;
            }
            source_rows[s][i] = j;
        }
        free(taken);
    }
    return matched;
}

/* The fitted copy of one file: the source rows of the file, a record's own names for a record with no row, and
 * for a row with no record the file's own entry of that row (it is never read). */
static uint8_t *fitted_file(const Lng *file, const Lng *eng, const Rows *rows, size_t *size)
{
    size_t n = LNG_HEADER;
    for (int s = 0; s < SECTIONS; s++) n += (size_t)rows[s].rows * sections[s].entry;
    uint8_t *out = calloc(1, n);
    if (!out) return NULL;
    int32_t header[8] = {0};
    for (int s = 0; s < SECTIONS; s++) header[sections[s].header] = rows[s].rows;
    memcpy(out, header, sizeof header);
    size_t at = LNG_HEADER;
    for (int s = 0; s < SECTIONS; s++) {
        const Section *sec = &sections[s];
        int have = file->count[sec->header];
        for (int i = 0; i < rows[s].rows; i++, at += sec->entry) {
            uint32_t rec = rows[s].record[i];
            int j = source_rows[s][i];
            uint8_t *e = out + at;
            if (j >= 0) memcpy(e, file->data + file->at[s] + (size_t)j * sec->entry, sec->entry);
            else if (!rec) { if (i < have) memcpy(e, file->data + file->at[s] + (size_t)i * sec->entry, sec->entry); }
            else if (s == S_CONTINENTS) {
                if (i < eng->count[sec->header]) memcpy(e, eng->data + eng->at[s] + (size_t)i * sec->entry, sec->entry);
            } else own_entry(s, rec, e);
        }
    }
    *size = n;
    return out;
}

static int written_now;          /* 1 when the last data load wrote the files */

int season_lng_fitted(void) { return written_now; }

void season_lng_fit_temp(void)
{
    written_now = 0;
    char eng_path[1400];
    if (!find_in(win_user_dir, "eng.lng", eng_path, sizeof eng_path)) return;    /* CM3_TEMP: the user folder */
    Lng eng;
    if (!lng_read(eng_path, &eng, 1)) return;
    Rows rows[SECTIONS];
    int ok = 1, fits = 1;
    for (int s = 0; s < SECTIONS; s++) {
        if (ok) ok = rows_of(s, &rows[s]);
        else memset(&rows[s], 0, sizeof rows[s]);
        if (ok && (rows[s].rows != eng.count[sections[s].header] || rows[s].extras)) fits = 0;
    }
    const char *always = getenv("RECOMP_LNG_FIT");     /* test only: "always" writes the files that fit too */
    if (ok && (!fits || (always && !strcmp(always, "always")))) {
        int matched = match_rows(&eng, rows), records = 0, written = 0;
        for (int s = 0; s < SECTIONS; s++)
            for (int i = 0; i < rows[s].rows; i++) records += rows[s].record[i] != 0;
        for (int k = 0; k < FILES; k++) {
            char path[1400];
            Lng file;
            if (!find_in(win_user_dir, files[k], path, sizeof path) || !lng_read(path, &file, 0)) continue;
            int same = 1;
            for (int i = 0; i < 8; i++) same &= file.count[i] == eng.count[i];
            size_t size = 0;
            uint8_t *out = same ? fitted_file(&file, &eng, rows, &size) : NULL;
            lng_free(&file);
            if (!out) continue;
            /* A temporary file first, then the rename: a full disk cannot leave a cut-off file for the save. */
            char temp[1500];
            snprintf(temp, sizeof temp, "%s.fit", path);
            FILE *f = host_fopen(temp, "wb");
            int whole = 0;
            if (f) {
                whole = fwrite(out, 1, size, f) == size;
                whole = (fclose(f) == 0) && whole;
            }
            if (whole && host_replace(temp, path) == 0) written++;
            else {
                host_unlink(temp);
                fprintf(stderr, "recomp: %s could not be written in the rows of the database; it stays as copied\n",
                        files[k]);
            }
            free(out);
        }
        written_now = written > 0;
        fprintf(stderr, "recomp: .lng files fitted to the database: %d files, %d of %d records with a row of the "
                        "copied files\n", written, matched, records);
    }
    for (int s = 0; s < SECTIONS; s++) free(rows[s].record);
    lng_free(&eng);
}
