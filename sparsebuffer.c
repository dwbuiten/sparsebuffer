/*
 * Copyright (c) 2020, Derek Buitenhuis
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sparsebuffer.h"

typedef struct Range {
    struct Range *prev;
    struct Range *next;
    size_t pos;
    size_t size;
    uint8_t *data;
} Range;

typedef struct SBReader {
    size_t pos;
    size_t size;
    Range *ranges;
    void *(*malloc)(size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void (*free)(void *ptr);
} SBReader;

/*
 * Util functions for ranges.
 */

/* Fees all ranges in the list. */
static void range_free(SBReader *reader, Range **ranges)
{
    Range *cur = *ranges;

    while (cur != NULL) {
        Range *tmp = cur;

        reader->free(cur->data);

        cur = cur->next;

        reader->free(tmp);
    }

    *ranges = NULL;
}

/* Removes a specific range from the list. */
static void range_remove(SBReader *reader, Range **r, size_t pos)
{
    Range *rm;

    for (rm = *r; rm != NULL; rm = rm->next) {
        if (rm->pos == pos)
            break;
    }

    assert(rm != NULL);

    if (rm->prev == NULL && rm->next == NULL) {
        range_free(reader, r);
    } else if (rm->next == NULL) {
        rm->prev->next = NULL;

        reader->free(rm->data);
        reader->free(rm);
    } else if (rm->prev == NULL) {
        rm->next->prev = NULL;
        *r             = rm->next;

        reader->free(rm->data);
        reader->free(rm);
    } else {
        rm->prev->next = rm->next;
        rm->next->prev = rm->prev;

        reader->free(rm->data);
        reader->free(rm);
    }
}

/* Inserts a new range before a given range in the list. */
static void range_insert_before(Range **r, Range *e, size_t pos)
{
    Range *rr;

    for (rr = *r; rr->pos != pos; rr = rr->next);

    assert(rr != NULL);

    if (rr == *r) {
        rr->prev = e;
        e->next  = rr;
        e->prev  = NULL;
        *r       = e;
        return;
    }

    e->next       = rr;
    e->prev       = rr->prev;
    rr->prev      = e;
    e->prev->next = e;
}

/* Inserts a new range after a given range in the list. */
static void range_insert_after(Range *r, Range *e, size_t pos)
{
    Range *rr;

    for (rr = r; rr->pos != pos; rr = rr->next);

    assert(rr != NULL);

    e->next  = rr->next;
    e->prev  = rr;
    rr->next = e;
    if(e->next != NULL)
        e->next->prev = e;
}

/* Inserts a range at the end of the list. */
static void range_insert_end(Range *r, Range *e)
{
    Range *last;

    for (last = r; last->next != NULL; last = last->next);

    last->next = e;
    e->prev    = last;
    e->next    = NULL;
}

/* Checks if two ranges intersect. */
static bool intersects(Range *a, Range *b)
{
    return !(a->pos + a->size < b->pos || b->pos + b->size < a->pos);
}

/* CHecks if a contains b. */
static bool contains(Range *a, Range *b)
{
    return a->pos <= b->pos && a->pos + a->size >= b->pos + b->size;
}

/* Merges two ranges if necessary. */
static int merge(SBReader *reader, Range *a, Range *b, Range *ret, bool *merged)
{
    if (!intersects(a, b)) {
        *merged = false;
        return 0;
    }

    if (contains(a, b)) {
        ret->pos  = a->pos;
        ret->size = a->size;
        ret->data = reader->malloc(a->size);
        if (ret->data == NULL)
            return -1;

        memcpy(ret->data, a->data, a->size);
        *merged = true;
        return 0;
    }

    if (contains(b, a)) {
        ret->pos  = b->pos;
        ret->size = b->size;
        ret->data = reader->malloc(b->size);
        if (ret->data == NULL)
            return -1;

        memcpy(ret->data, b->data, b->size);
        *merged = true;
        return 0;
    }

    Range *first, *second;
    if (a->pos <= b->pos) {
        first  = a;
        second = b;
    } else {
        first  = b;
        second = a;
    }

    size_t newsize = second->pos + second->size - first->pos;
    uint8_t *buf   = reader->malloc(newsize);
    if (buf == NULL)
        return -1;

    if (first == a) {
        memcpy(buf, first->data, first->size);
        memcpy(buf + first->size, second->data + first->pos + first->size - second->pos, second->size - (first->pos + first->size - second->pos));
    } else {
        memcpy(buf, first->data, second->pos - first->pos);
        memcpy(buf + second->pos - first->pos, second->data, second->size);
    }

    ret->pos  = first->pos;
    ret->size = newsize;
    ret->data = buf;

    *merged = true;
    return 0;
}

SBReader *sb_new_reader_custom_alloc(size_t size, void *(*custom_alloc)(size_t size),
                                     void *(*custom_realloc)(void *ptr, size_t size), void (*custom_free)(void *ptr), SBError *err)
{
    SBReader *ret;

    if (size == 0) {
        snprintf(err->error, err->size, "Invalid reader size.");
        return NULL;
    }

    ret = custom_alloc(sizeof(*ret));
    if (ret == NULL) {
        snprintf(err->error, err->size, "Could not allocate SBReader.");
        return NULL;
    }

    ret->pos     = 0;
    ret->size    = size;
    ret->ranges  = NULL;
    ret->malloc  = custom_alloc;
    ret->realloc = custom_realloc;
    ret->free    = custom_free;

    return ret;
}

SBReader *sb_new_reader(size_t size, SBError *err)
{
    return sb_new_reader_custom_alloc(size, malloc, realloc, free, err);
}


void sb_free_reader(SBReader **reader)
{
    SBReader *r = *reader;

    if (r->ranges != NULL)
        range_free(*reader, &r->ranges);

    void (*custom_free)(void *ptr) = (*reader)->free;

    custom_free(*reader);
    *reader = NULL;
}

void sb_clear(SBReader *reader)
{
    range_free(reader, &reader->ranges);
}

size_t sb_bytes_left(SBReader *reader)
{
    return reader->size - reader->pos;
}

size_t sb_size(SBReader *reader)
{
    return reader->size;
}

int sb_load_range(SBReader *reader, size_t pos, uint8_t *buf, size_t bufsize, SBError *err)
{
    if (bufsize == 0) {
        snprintf(err->error, err->size, "Invalid buffer size.");
        return -1;
    } else if (reader->pos + bufsize > reader->size) {
        snprintf(err->error, err->size, "Cannot load a range passed the end of the sparse buffer size.");
        return -1;
    }

    Range *r = reader->malloc(sizeof(*r));
    if (r == NULL) {
        snprintf(err->error, err->size, "Could not allocate new range.");
        return -1;
    }
    r->prev = NULL;
    r->next = NULL;
    r->pos  = pos;
    r->size = bufsize;
    r->data = reader->malloc(bufsize);
    if (r->data == NULL) {
        reader->free(r);
        snprintf(err->error, err->size, "Could not allocate buffer for spares list entry.");
        return -1;
    }
    memcpy(r->data, buf, bufsize);

    /* If list is empty, just add the new range and return. */
    if (reader->ranges == NULL) {
        reader->ranges = r;
        return 0;
    }

    /* Merge with an existing range if needed. */
    bool merged   = false;
    Range mr      = { 0 };
    Range *mrange = NULL;
    for (Range *e = reader->ranges; e != NULL; e = e->next) {
        int mret = merge(reader, r, e, &mr, &merged);
        if (mret < 0) {
            reader->free(r->data);
            reader->free(r);
            snprintf(err->error, err->size, "Could not allocate merged buffer.");
            return -1;
        }
        if (merged) {
            e->pos  = mr.pos;
            e->size = mr.size;
            reader->free(e->data);
            e->data = mr.data;
            mrange  = e;
            break;
        }
    }

    if (merged) {
        /* Merge with any further ranges, if necessary. */
        bool mergeagain = true;
        while (mergeagain) {
            mergeagain = false;
            Range *mrng = mrange;
            for (Range *e = mrange->next; e != NULL; e = e->next) {
                bool m;
                int mret = merge(reader, mrng, e, &mr, &m);
                if (mret < 0) {
                    reader->free(r->data);
                    reader->free(r);
                    snprintf(err->error, err->size, "Could not allocate merged buffer.");
                    return -1;
                }
                if (m) {
                    mrange->pos  = mr.pos;
                    mrange->size = mr.size;

                    reader->free(mrange->data);
                    mrange->data = mr.data;

                    range_remove(reader, &reader->ranges, e->pos);

                    mergeagain = true;
                    break;
                } else {
                    break;
                }
            }
        }
        reader->free(r->data);
        reader->free(r);
    } else {
        /* Just insert it as-is. */
        Range *e;
        for (e = reader->ranges; e != NULL; e = e->next) {
            if (r->pos < e->pos)
                break;
        }
        if (e == NULL)
            range_insert_end(reader->ranges, r);
        else
            range_insert_before(&reader->ranges, r, e->pos);
    }

    return 0;
}

size_t sb_read(SBReader *reader, uint8_t *buf, size_t size, SBError *err)
{
    if (size == 0) {
        snprintf(err->error, err->size, "Cannot read zero bytes.");
        return 0;
    }

    size_t off = reader->pos;
    size_t pos = 0;
    size_t rem = size;
    for (Range *e = reader->ranges; e != NULL; e = e->next) {
        if (e->pos + e->size < off)
            continue;

        /* Output zeroes until we hit the start of a range. */
        if (e->pos > off) {
            size_t zerosize = e->pos - off;
            if (zerosize > rem)
                zerosize = rem;
            memset(buf + pos, 0, zerosize);
            pos += zerosize;
            off += zerosize;
            rem -= zerosize;
        }
        if (rem == 0)
            break;

        size_t copysize = e->size - (off - e->pos);
        if (copysize > rem)
            copysize = rem;
        memcpy(buf + pos, e->data + (off - e->pos), copysize);
        pos += copysize;
        off += copysize;
        rem -= copysize;
        if (rem == 0)
            break;
    }

    /* Output zeros until we hit the end the requested size. */
    if (rem > 0 && off < reader->size) {
        size_t zerosize = reader->size - off;
        if (zerosize > rem)
            zerosize = rem;
        memset(buf + pos, 0, zerosize);
        pos += zerosize;
        off += zerosize;
        rem -= zerosize;
    }

    if (rem != 0) {
        snprintf(err->error, err->size, "Cannot read past EOF.");
        return 0;
    }

    return size;
}

int sb_seek(SBReader *reader, size_t offset, SBWhence whence, size_t *pos, SBError *err)
{
    size_t realoffset;

    switch (whence) {
    case SB_SET:
        realoffset = offset;
        break;
    case SB_CUR:
        realoffset = offset + reader->pos;
        break;
    case SB_END:
        if (offset > reader->size) {
            snprintf(err->error, err->size, "Cannot seek past beginning of file.");
            return -1;
        }
        realoffset = reader->size - offset;
        break;
    default:
        snprintf(err->error, err->size, "Invalid whence.");
        return -1;
    }

    if (realoffset > reader->size) {
        snprintf(err->error, err->size, "Cannot seek past end of file.");
        return -1;
    }

    reader->pos = realoffset;
    *pos        = realoffset;

    return 0;
}

int sb_remove_range(SBReader *reader, size_t start, size_t end, SBError *err)
{
    if (end >= reader->size || end < start) {
        snprintf(err->error, err->size, "Invalid range.");
        return -1;
    }

    for (Range *e = reader->ranges; e != NULL;) {
        size_t rngstart = e->pos;
        size_t rngend   = e->pos + e->size - 1;

        /* Current range is after the deletion range. */
        if (rngstart > end)
            break;

        /* Current range is before the deletion range. */
        if (rngend < start) {
            e = e->next;
            continue;
        }

        /* Current range is entirely in the deletion range. */
        if (rngstart >= start && rngend <= end) {
            Range *next = e->next;
            range_remove(reader, &reader->ranges, e->pos);
            e = next;
            continue;
        }

        /* We need to split an existing range, since we're in between it. */
        if (rngend > end && rngstart < start) {
            Range *rng0 = reader->malloc(sizeof(Range));
            if (rng0 == NULL) {
                snprintf(err->error, err->size, "Could not allocate new range buffer.");
                return -1;
            }

            rng0->pos  = end + 1;
            rng0->size = rngend + 1 - rng0->pos;
            rng0->data = reader->malloc(rng0->size);
            if (rng0->data == NULL) {
                reader->free(rng0);
                snprintf(err->error, err->size, "Could not allocate new range data.");
                return -1;
            }
            memcpy(rng0->data, e->data + end + 1 - rngstart, rng0->size);

            range_insert_after(reader->ranges, rng0, e->pos);

            e->size      = start - e->pos;
            uint8_t *tmp = reader->realloc(e->data, e->size);
            if (tmp == NULL) {
                snprintf(err->error, err->size, "Could not realloc split range data.");
                return -1;
            }
            e->data = tmp;

            e = rng0->next;

            continue;
        }

        /* Current range overlaps the start of the deletion range. */
        if (rngstart < start) {
            e->size      = start - e->pos;
            uint8_t *tmp = reader->realloc(e->data, e->size);
            if (tmp == NULL) {
                snprintf(err->error, err->size, "Could not realloc reduced range data.");
                return -1;
            }
            e->data = tmp;
        }

        /* current range overlaps the end of the deletion range. */
        if (rngend > end) {
            size_t oldSize = e->size;
            e->size -= end + 1 - e->pos;
            e->pos   = end + 1;

            uint8_t *newdata = reader->malloc(e->size);
            if (newdata == NULL) {
                snprintf(err->error, err->size, "Could not allocate new range data.");
                return 1;
            }
            memcpy(newdata, e->data + oldSize - e->size, e->size);

            reader->free(e->data);

            e->data = newdata;
        }

        e = e->next;
    }

    return 0;
}

int sb_resize(SBReader *reader, size_t newsize, SBError *err)
{
    if (newsize == 0) {
        snprintf(err->error, err->size, "Cannot resize to zero size.");
        return -1;
    }

    if (newsize < reader->size) {
        int ret = sb_remove_range(reader, newsize, reader->size - 1, err);
        if (ret < 0)
            return ret;
        if (reader->pos > newsize)
            reader->pos = newsize;
    }

    reader->size = newsize;

    return 0;
}
