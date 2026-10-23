/* The .lng files fitted to the database (O260, guard review row A25).
 *
 * In a language other than English the game reads CM3_TEMP\<lang>.lng (database_load_language_names 0x5339a0) and
 * CM3_TEMP\eng.lng (eng_lng_load_tables 0x544fc0, the English names). Both compare the counts in the header with
 * the database (0x533e6a..0x533ec5, 0x545193..0x5451ed) and give the box "Database..cpp 7784" on a difference. The
 * .lng files of the game fit the 3.9.68 data only. The data sets 3.9.60 and 3.9.65 have their own (make_clean_files.py),
 * but a season database, a custom database and every database with added club rows (season_records.c) have none:
 * their names stayed English, and a new club read the translation of .lng row 0.
 *
 * The fix: while one of the two functions runs, the open of its .lng file (fopen 0x945b00) gets a file made for the
 * database in memory (win_memory_file). Its header holds the database's counts. Each record of the database gets the
 * entry of the 3.9.68 .lng row whose English name (eng.lng) equals the record's name; a record with no such row gets
 * its own names. For eng.lng every record gets its own names. The record names at +4 are the English names of the
 * data (the data language [0x9b5984] is 0 for every database). A record with no row is saved before the original
 * function and written back after it, so the function changes nothing in it.
 *
 * The layout (found from the 3.9.68 data: each eng.lng entry equals fields of its record; sizes from the reads
 * 0x533eff..0x5350ad and 0x54521c..0x545355): a header of 8 s32 counts (clubs, continents, nation comps, nations,
 * cities, club comps, stadiums, staff comps), then the entries in this order: clubs 0x4f, continents 0xba, nation
 * comps 0x53, nations 0x6d, stadiums 0x34, cities 0x34, staff comps 0x4f, club comps 0x53. An entry holds the name
 * (record +4, 51 bytes) and the byte after it (record +0x37); the 0x4f and larger entries then the short name
 * (record +0x38, 26 bytes) and the byte after it (record +0x52); a comp entry then the abbreviation (record +0x53,
 * 4 bytes); a nation entry then 30 bytes from record +0x53. A continent entry is not made from its record: a
 * continent with no row keeps the 3.9.68 eng.lng entry of its index and is written back.
 *
 * The club entries are indexed by the .lng row of each club: [0xae23fc][club], or the club's +0x00 when the table is
 * NULL (club_get_english_name 0x545880, docs/research/2026-10-19-club-table-users.md). */
#include "win.h"

#include <dirent.h>
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

/* A .lng file of the game's Data (3.9.68), read once. */
typedef struct {
    uint8_t *data;
    size_t size;
    int32_t count[8];
    size_t at[SECTIONS];        /* offset of each section */
} Lng;

static Lng eng68, lang68;
static char lang68_name[32];

static int read_game_lng(const char *file, Lng *l)
{
    free(l->data);
    memset(l, 0, sizeof *l);
    char dir[1100], path[1400];
    snprintf(dir, sizeof dir, "%s/Data", win_game_dir);
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    path[0] = 0;
    while ((e = readdir(d)))
        if (!strcasecmp(e->d_name, file)) { snprintf(path, sizeof path, "%s/%s", dir, e->d_name); break; }
    closedir(d);
    FILE *f = path[0] ? fopen(path, "rb") : NULL;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    l->data = n > (long)LNG_HEADER ? malloc((size_t)n) : NULL;
    if (!l->data || fread(l->data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(l->data); l->data = NULL; return 0; }
    fclose(f);
    l->size = (size_t)n;
    memcpy(l->count, l->data, sizeof l->count);
    size_t at = LNG_HEADER;
    for (int s = 0; s < SECTIONS; s++) {
        l->at[s] = at;
        if (l->count[sections[s].header] < 0) { free(l->data); l->data = NULL; return 0; }
        at += (size_t)l->count[sections[s].header] * sections[s].entry;
    }
    if (at != l->size) { free(l->data); l->data = NULL; return 0; }
    return 1;
}

/* The database side of a section: the record of each .lng row (0 for a row with no record). */
typedef struct {
    int rows;
    uint32_t *record;
} Rows;

static int comp_counts(int32_t *clubs, int32_t *nations)
{
    int32_t total = (int32_t)rd32(g_mem, COMP_TOTAL) - 0x7f, n = (int32_t)rd32(g_mem, NATION_COMP_COUNT);
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
        r->record = malloc(sizeof *r->record * (size_t)(n ? n : 1));
        for (int32_t i = 0; i < n; i++) r->record[i] = base + sec->record * (uint32_t)(first + i);
        return 1;
    }
    uint32_t map = rd32(g_mem, CLUB_ROW_TABLE);
    int32_t rows = 0;
    for (int32_t i = 0; i < n; i++) {
        int32_t row = (int32_t)(map ? rd32(g_mem, map + 4u * (uint32_t)i) : rd32(g_mem, base + sec->record * (uint32_t)i));
        if (row >= rows && row < 400000) rows = row + 1;
    }
    r->rows = rows;
    r->record = calloc((size_t)(rows ? rows : 1), sizeof *r->record);
    for (int32_t i = 0; i < n; i++) {
        uint32_t rec = base + sec->record * (uint32_t)i;
        int32_t row = (int32_t)(map ? rd32(g_mem, map + 4u * (uint32_t)i) : rd32(g_mem, rec));
        if (row >= 0 && row < rows && !r->record[row]) r->record[row] = rec;
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

static void name_of(const uint8_t *src, char *out)
{
    memcpy(out, src, 51);
    out[51] = 0;
}

/* eng.lng's rows of each section sorted by name, made once. */
static int *sorted[SECTIONS];
static int sort_section;

static int by_name(const void *a, const void *b)
{
    const Section *sec = &sections[sort_section];
    const uint8_t *e = eng68.data + eng68.at[sort_section];
    char x[52], y[52];
    name_of(e + (size_t)*(const int *)a * sec->entry, x);
    name_of(e + (size_t)*(const int *)b * sec->entry, y);
    int c = strcmp(x, y);
    return c ? c : *(const int *)a - *(const int *)b;
}

static void sort_names(void)
{
    for (int s = 0; s < SECTIONS; s++) {
        int n = eng68.count[sections[s].header];
        free(sorted[s]);
        sorted[s] = malloc(sizeof(int) * (size_t)(n ? n : 1));
        for (int j = 0; j < n; j++) sorted[s][j] = j;
        sort_section = s;
        qsort(sorted[s], (size_t)n, sizeof(int), by_name);
    }
}

/* The 3.9.68 row of a record: its English name equals eng.lng's name of that row. The same row number comes first,
 * then the first row with that name that no record took. -1 for none. */
static int row68(int s, int row, const char *name, const uint8_t *taken)
{
    const Section *sec = &sections[s];
    int n = eng68.count[sec->header];
    const uint8_t *e = eng68.data + eng68.at[s];
    char other[52];
    if (row < n && !taken[row]) {
        name_of(e + (size_t)row * sec->entry, other);
        if (!strcmp(other, name)) return row;
    }
    int lo = 0, hi = n;                     /* the first sorted row whose name is not below name */
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        name_of(e + (size_t)sorted[s][mid] * sec->entry, other);
        if (strcmp(other, name) < 0) lo = mid + 1; else hi = mid;
    }
    for (; lo < n; lo++) {
        int j = sorted[s][lo];
        name_of(e + (size_t)j * sec->entry, other);
        if (strcmp(other, name)) break;
        if (!taken[j]) return j;
    }
    return -1;
}

/* Saved records (a record with no 3.9.68 row) and the memory file. */
typedef struct { uint32_t at, size; uint8_t *bytes; } Saved;
static Saved *saved;
static int saved_count, saved_room;
static uint8_t *fitted;
static size_t fitted_size;
static char fitted_path[1400];
static int pending;             /* 1: database_load_language_names runs, 2: eng_lng_load_tables runs */

static void save_record(uint32_t at, uint32_t size)
{
    if (saved_count == saved_room) {
        saved_room = saved_room ? saved_room * 2 : 256;
        saved = realloc(saved, sizeof *saved * (size_t)saved_room);
    }
    saved[saved_count].at = at;
    saved[saved_count].size = size;
    saved[saved_count].bytes = malloc(size);
    memcpy(saved[saved_count].bytes, g_mem + at, size);
    saved_count++;
}

/* Builds the fitted file for the .lng named file. Returns 0 when it cannot (the game then reads the file as is). */
static int fit(const char *file, int english)
{
    if (!eng68.data) {
        if (!read_game_lng("eng.lng", &eng68)) return 0;
        sort_names();
    }
    if (!english && strcasecmp(lang68_name, file)) {
        lang68_name[0] = 0;
        if (!read_game_lng(file, &lang68)) return 0;
        for (int i = 0; i < 8; i++) if (lang68.count[i] != eng68.count[i]) return 0;
        snprintf(lang68_name, sizeof lang68_name, "%s", file);
    }
    Rows rows[SECTIONS];
    memset(rows, 0, sizeof rows);
    size_t size = LNG_HEADER;
    int ok = 1;
    for (int s = 0; s < SECTIONS && ok; s++) {
        ok = rows_of(s, &rows[s]);
        size += (size_t)rows[s].rows * sections[s].entry;
    }
    int32_t header[8] = {0};
    for (int s = 0; s < SECTIONS && ok; s++) header[sections[s].header] = rows[s].rows;
    free(fitted);
    fitted = ok ? calloc(1, size) : NULL;
    fitted_size = size;
    int own = 0, matched = 0;
    if (fitted) {
        memcpy(fitted, header, sizeof header);
        size_t at = LNG_HEADER;
        for (int s = 0; s < SECTIONS; s++) {
            const Section *sec = &sections[s];
            uint8_t *taken = calloc((size_t)eng68.count[sec->header] + 1, 1);
            for (int i = 0; i < rows[s].rows; i++, at += sec->entry) {
                uint32_t rec = rows[s].record[i];
                uint8_t *e = fitted + at;
                if (!rec) continue;                      /* a row with no club: never read */
                char name[52];
                name_of(g_mem + rec + 4, name);
                int j = name[0] ? row68(s, i, name, taken) : -1;
                if (j >= 0) taken[j] = 1;
                if (j >= 0) matched++; else own++;
                if (!english && j >= 0) {
                    memcpy(e, lang68.data + lang68.at[s] + (size_t)j * sec->entry, sec->entry);
                    continue;
                }
                if (s == S_CONTINENTS) {
                    /* No layout from the record: the 3.9.68 English entry of its row (or of its index). */
                    int k = j >= 0 ? j : (i < eng68.count[sec->header] ? i : -1);
                    if (k >= 0) memcpy(e, eng68.data + eng68.at[s] + (size_t)k * sec->entry, sec->entry);
                } else {
                    own_entry(s, rec, e);
                }
                if (!english) save_record(rec, sec->record);
            }
            free(taken);
        }
    }
    for (int s = 0; s < SECTIONS; s++) free(rows[s].record);
    if (!fitted) {
        if (getenv("RECOMP_LNG_DEBUG")) fprintf(stderr, "recomp: lng debug: no fit (ok %d)\n", ok);
        return 0;
    }
    fprintf(stderr, "recomp: %s fitted to the database: %d records with a 3.9.68 row, %d with their own names\n", file,
            matched, own);
    return 1;
}

static void end_fit(void)
{
    if (fitted_path[0]) win_memory_file_remove(fitted_path);
    fitted_path[0] = 0;
    for (int i = 0; i < saved_count; i++) {
        memcpy(g_mem + saved[i].at, saved[i].bytes, saved[i].size);
        free(saved[i].bytes);
    }
    saved_count = 0;
    pending = 0;
}

void season_lng_begin(int english)
{
    end_fit();
    pending = english ? 2 : 1;
    if (getenv("RECOMP_LNG_DEBUG")) fprintf(stderr, "recomp: lng debug: begin %d\n", english);
}
void season_lng_end(void) { end_fit(); }

void o_00945b00(void);

/* fopen: the open of the .lng file while one of the two functions runs. */
RT_WRAP(00945b00, crt_fopen)
{
    if (getenv("RECOMP_LNG_DEBUG"))
        fprintf(stderr, "recomp: lng debug: fopen called, pending %d, back 0x%x, %s\n", pending, rd32(g_mem, cpu.esp),
                (const char *)(g_mem + rt_arg(0)));
    if (pending && !fitted_path[0]) {
        const char *path = (const char *)(g_mem + rt_arg(0)), *mode = (const char *)(g_mem + rt_arg(1));
        if (getenv("RECOMP_LNG_DEBUG")) fprintf(stderr, "recomp: lng debug: fopen %s %s\n", path, mode);
        size_t n = strlen(path);
        const char *base = strrchr(path, '\\');
        base = base ? base + 1 : path;
        if (n > 4 && !strcasecmp(path + n - 4, ".lng") && mode[0] == 'r' && strlen(base) < sizeof lang68_name) {
            char host[1400];
            if ((win_existing_file(path, host, sizeof host) || win_host_path(path, host, sizeof host)) &&
                fit(base, pending == 2)) {
                snprintf(fitted_path, sizeof fitted_path, "%s", host);
                win_memory_file(fitted_path, fitted, fitted_size);
            }
        }
    }
    o_00945b00();
}
