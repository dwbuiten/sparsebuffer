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

#ifndef SPARSEBUFFER_H_
#define SPARSEBUFFER_H_

#include <stdint.h>
#include <stdlib.h>

/* Opaque type for the API. */
typedef struct SBReader SBReader;

/* User supplied error buffer. */
typedef struct SBError {
    char *error;
    size_t size;
} SBError;

/* Whence enums for seeking. */
typedef enum SBWhence {
    SB_SET = 0,
    SB_CUR = 1,
    SB_END = 2
} SBWhence;

/*
 * Creates a new sparse buffer reader with default allocators.
 *
 * Arguments:
 *   * size - The size of the sparse buffer for the reader.
 *   * err  - A user supplied error buffer.
 *
 * Returns:
 *   A new sparse buffer reader, which must be freed with sb_free_reader()
 *   after use, or NULL on error.
 */
SBReader *sb_new_reader(size_t size, SBError *err);

/*
 * Creates a new sparse buffer reader with user-provided allocators.
 *
 * Arguments:
 *   * size           - The size of the sparse buffer for the reader.
 *   * custom_alloc   - User provided allocation function, which must have the same semantics as malloc.
 *   * custom_realloc - User provided reallocation function, which must have the same semantics as realloc.
 *   * custom_free    - User provided free functon, which must have the same semntics as free.
 *   * err            - A user supplied error buffer.
 *
 * Returns:
 *   A new sparse buffer reader, which must be freed with sb_free_reader()
 *   after use, or NULL on error.
 */
SBReader *sb_new_reader_custom_alloc(size_t size, void *(*custom_alloc)(size_t size),
                                     void *(*custom_realloc)(void *ptr, size_t size), void (*custom_free)(void *ptr), SBError *err);

/*
 * Frees a sparse buffer reader and sets it to NULL;
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 */
void sb_free_reader(SBReader **reader);

/*
 * Clears all loaded buffers in a sparse buffer reader.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 */
void sb_clear(SBReader *reader);

/*
 * Resizes an existing sparse buffer reader to a new size.
 *
 * Arguments:
 *   * reader  - A pointer to a sparse buffer reader pointer allocated by
 *               sb_new_reader().
 *   * newsize - The size to resize the reader too. May be smaller or larger.
 *   * err     - A user supplied error buffer.
 *
 * Returns:
 *   0 on success, and < 0 on error.
 */
int sb_resize(SBReader *reader, size_t newsize, SBError *err);

/*
 * Gets the bytes left to read in a given sparse buffer reader.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 *
 * Returns:
 *   The number of bytes left to read until EOF, in the reader.
 */
size_t sb_bytes_left(SBReader *reader);

/*
 * Gets the total size in bytes of a given sparse buffer reader.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 *
 * Returns:
 *   The total size of the reader.
 */
size_t sb_size(SBReader *reader);

/*
 * Loads a range into the sparse buffer.
 *
 * Arguments:
 *   * reader  - A pointer to a sparse buffer reader pointer allocated by
 *               sb_new_reader().
 *   * pos     - The byte position to load the range into, in the sparse buffer.
 *   * buf     - The buffer to load into the sparse buffer.
 *   * bufsize - The size of the buffer to load.
 *   * err     - A user supplied error buffer.
 *
 * Returns:
 *   0 on success, and < 0 on error.
 */
int sb_load_range(SBReader *reader, size_t pos, uint8_t *buf, size_t bufsize, SBError *err);

/*
 * Remove a range from the sparse buffer.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 *   * start  - The starting position of the range to remove from the sparse buffer.
 *   * end    - The ending position of the range to remove from the sparse buffer.
 *   * err    - A user supplied error buffer.
 *
 * Returns:
 *   0 on success, and < 0 on error.
 */
int sb_remove_range(SBReader *reader, size_t start, size_t end, SBError *err);

/*
 * Read bytes at the current positin of the sparsebuffer.
 *
 * Zeroes will be returned for any ranges not loaded into the sparse buffer.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 *   * buf    - A user supplied buffer to read into.
 *   * size   - The number of bytes to read.
 *   * err    - A user supplied error buffer.
 *
 * Returns:
 *   size on success, != size on error.
 */
size_t sb_read(SBReader *reader, uint8_t *buf, size_t size, SBError *err);

/*
 * Seek to a given position in the sparse buffer.
 *
 * Arguments:
 *   * reader - A pointer to a sparse buffer reader pointer allocated by
 *              sb_new_reader().
 *   * offset - The offset to seek to.
 *   * whence - From whence to seek. See: SBWhence.
 *   * pos    - A user supplied buffer in which the absolute position that
 *              has been seeked to is written.
 *   * err    - A user supplied error buffer.
 *
 * Returns:
 *   0 on success, and < 0 on error.
 */
int sb_seek(SBReader *reader, size_t offset, SBWhence whence, size_t *pos, SBError *err);

#endif
