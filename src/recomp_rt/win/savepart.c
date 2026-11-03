/* Parts of the fork in a saved game (fork of fintan-ohea): the player editor's bosman_editor.dat (editor.c) and the
 * scouts' bosman_scouting.dat (scouting.c).
 *
 * The save container (zip.cpp of the original, tools/save/savfile.py, season.c): three words (version, flags,
 * count), then 1000 rows of 0x10c bytes (offset, size, name of 260 bytes), then the parts. Version 2 and 4 store
 * each part with the byte run code of zwrite 0x932e10, version 1 and 3 store it as it is. The original opens a
 * part by its exact name and extracts only the parts whose name ends in .tmp (load_game_data 0x82a9c0), so it
 * never reads these. The parts are added after the save has written the file, as season.c adds its part. */
#include "win.h"
#include "../host.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_ROWS 1000u
#define ROW_SIZE 0x10cu
#define ROW_NAME 8u
#define NAME_SIZE 260u
#define MAX_PARTS 999u              /* zopen 0x9326e0 refuses a new part at 999 (season.c) */

static int read_at(int fd, int64_t at, void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_read(fd, buf, n) == (long)n;
}

static int write_at(int fd, int64_t at, const void *buf, uint32_t n)
{
    return host_seek(fd, at, SEEK_SET) == at && host_write(fd, buf, n) == (long)n;
}

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* The part's bytes as the container stores them: as they are (version 1 and 3), or with the byte run code of
 * zwrite 0x932e10 (version 2 and 4), which stores a byte above 0x80 or a run as a count and a byte. */
static uint32_t stored_form(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t version)
{
    if (version != 2 && version != 4) {
        memcpy(out, in, n);
        return n;
    }
    uint32_t o = 0;
    for (uint32_t i = 0; i < n;) {
        uint32_t j = i + 1;
        while (j < n && in[j] == in[i] && j - i < 0x7f) j++;
        if (j - i < 2 && in[i] < 0x81) out[o++] = in[i];
        else out[o++] = (uint8_t)(j - i + 0x80), out[o++] = in[i];
        i = j;
    }
    return o;
}

/* The decoder of zread 0x932c30 (season.c): n bytes out of room stored bytes. 0 when the bytes end early. */
static int decode(const uint8_t *in, uint32_t room, uint8_t *out, uint32_t n)
{
    uint32_t o = 0, i = 0;
    while (o < n && i < room) {
        uint8_t b = in[i++];
        if (b < 0x81) { out[o++] = b; continue; }
        if (i >= room) return 0;
        for (uint32_t k = 0; k < (uint32_t)(b - 0x80) && o < n; k++) out[o++] = in[i];
        i++;
    }
    return o == n;
}

/* Finds the row of the part name: its index, or count when the file has none. -1 when the file is no container. */
static int find_row(int fd, const char *name, uint32_t *version, uint32_t *count, uint8_t *row)
{
    uint8_t head[12];
    if (!read_at(fd, 0, head, 12)) return -1;
    *version = get32(head);
    *count = get32(head + 8);
    if (*version < 1 || *version > 4 || *count > HEADER_ROWS) return -1;
    for (uint32_t k = 0; k < *count; k++) {
        if (!read_at(fd, 12 + (int64_t)k * ROW_SIZE, row, ROW_SIZE)) return -1;
        if (!strncmp((const char *)row + ROW_NAME, name, NAME_SIZE)) return (int)k;
    }
    return (int)*count;
}

int savepart_write(const char *host, const char *name, const uint8_t *data, uint32_t size)
{
    if (strlen(name) >= NAME_SIZE || size > (1u << 24)) return 1;
    int fd = host_open(host, O_RDWR, 0);
    if (fd < 0) return 1;
    int bad = 1;
    uint32_t version, count;
    uint8_t row[ROW_SIZE], *stored = malloc(2 * (size_t)size + 2);
    int at = find_row(fd, name, &version, &count, row);
    if (!stored || at < 0 || ((uint32_t)at == count && count >= MAX_PARTS)) goto done;
    uint32_t n = stored_form(data, size, stored, version);
    int64_t end = host_seek(fd, 0, SEEK_END);
    if (end < 12 + (int64_t)HEADER_ROWS * ROW_SIZE || end + n > 0xfffffffe) goto done;
    /* The bytes, then the row, then the count, as season.c: a write that stops leaves the old file whole. */
    if (!write_at(fd, end, stored, n)) goto done;
    memset(row, 0, sizeof row);
    put32(row, (uint32_t)end);
    put32(row + 4, size);
    memcpy(row + ROW_NAME, name, strlen(name) + 1);
    if (!write_at(fd, 12 + (int64_t)at * ROW_SIZE, row, ROW_SIZE)) goto done;
    if ((uint32_t)at == count) {
        uint8_t c[4];
        put32(c, count + 1);
        if (!write_at(fd, 8, c, 4)) goto done;
    }
    bad = 0;
done:
    free(stored);
    host_close(fd);
    return bad;
}

int savepart_read(const char *host, const char *name, uint8_t *data, uint32_t room)
{
    int fd = host_open(host, O_RDONLY, 0);
    if (fd < 0) return 0;
    uint32_t version, count;
    uint8_t row[ROW_SIZE], *stored = NULL;
    int result = 0, at = find_row(fd, name, &version, &count, row);
    uint32_t size = at >= 0 && (uint32_t)at < count ? get32(row + 4) : 0;
    if (at >= 0 && (uint32_t)at < count) {
        result = -1;
        int64_t end = host_seek(fd, 0, SEEK_END), offset = get32(row);
        uint32_t want = 2 * size + 2;
        uint32_t got = end - offset < (int64_t)want ? (uint32_t)(end > offset ? end - offset : 0) : want;
        stored = malloc(want);
        if (size && size <= room && stored && got && read_at(fd, offset, stored, got)) {
            int ok = (version == 2 || version == 4) ? decode(stored, got, data, size)
                                                     : (got >= size && (memcpy(data, stored, size), 1));
            if (ok) result = (int)size;
        }
    }
    free(stored);
    host_close(fd);
    return result;
}
