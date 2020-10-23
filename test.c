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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sparsebuffer.h"

void *test_alloc(size_t size)
{
    char *ptr = malloc(size + 4);
    if (ptr == NULL)
        return NULL;

    ptr[0] = 'T';
    ptr[1] = 'E';
    ptr[2] = 'S';
    ptr[3] = 'T';

    return (void *)(ptr + 4);
}

void *test_realloc(void *ptr, size_t size)
{
    assert(!memcmp(ptr - 4, "TEST", 4));

    void *newptr = realloc(ptr - 4, size + 4);
    if (newptr == NULL)
        return NULL;

    return newptr + 4;
}

void test_free(void *ptr)
{
    assert(!memcmp(ptr - 4, "TEST", 4));

    free(ptr - 4);
}

int main()
{
    char e[1024];
    SBError err = { &e[0], 1024 };

    SBReader *r = sb_new_reader_custom_alloc(50, test_alloc, test_realloc, test_free, &err);
    if (r == NULL) {
        printf("Failed to make new reader: %s\n", err.error);
        return 1;
    }

    uint8_t loc1[10];
    uint8_t loc2[10];
    memset(&loc1[0], 1, 10);
    memset(&loc2[0], 2, 10);

    int ret = sb_load_range(r, 0, &loc1[0], 10, &err);
    if (ret < 0) {
        printf("Failed to load first range: %s\n", err.error);
        return 1;
    }

    ret = sb_load_range(r, 30, &loc2[0], 10, &err);
    if (ret < 0) {
        printf("Failed to load second range: %s\n", err.error);
        return 1;
    }

    uint8_t buf[50];
    size_t read = sb_read(r, &buf[0], 50, &err);
    if (read != 50) {
        printf("Failed to read sparsebuffer: %s\n", err.error);
        return 1;
    }

    uint8_t expected1[50] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0 };
    for (size_t i = 0; i < 50; i++) {
        if (buf[i] != expected1[i]) {
            printf("Expected %"PRIu8" at pos %zu, but got %"PRIu8".\n", expected1[i], i, buf[i]);
            return 1;
        }
    }

    sb_free_reader(&r);

    r = sb_new_reader_custom_alloc(50, test_alloc, test_realloc, test_free, &err);
    if (r == NULL) {
        printf("Failed to make new reader: %s\n", err.error);
        return 1;
    }

    uint8_t loc3[10];
    uint8_t loc4[10];
    uint8_t loc5[7];
    uint8_t loc6[40];
    memset(&loc3[0], 1, 10);
    memset(&loc4[0], 2, 10);
    memset(&loc5[0], 3, 7);
    memset(&loc6[0], 4, 40);

    ret = sb_load_range(r, 0, &loc3[0], 10, &err);
    if (ret < 0) {
        printf("Failed to load third range: %s\n", err.error);
        return 1;
    }

    ret = sb_load_range(r, 4, &loc4[0], 10, &err);
    if (ret < 0) {
        printf("Failed to load fourth range: %s\n", err.error);
        return 1;
    }

    ret = sb_load_range(r, 40, &loc5[0], 7, &err);
    if (ret < 0) {
        printf("Failed to load fifth range: %s\n", err.error);
        return 1;
    }

    ret = sb_load_range(r, 5, &loc6[0], 40, &err);
    if (ret < 0) {
        printf("Failed to load sixth range: %s\n", err.error);
        return 1;
    }

    memset(&buf[0], 0, 50);
    read = sb_read(r, &buf[0], 50, &err);
    if (read != 50) {
        printf("Failed to read sparsebuffer: %s\n", err.error);
        return 1;
    }

    uint8_t expected2[50] = { 1, 1, 1, 1, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                              4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                              4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                              3, 3, 0, 0, 0 };
    for (size_t i = 0; i < 50; i++) {
        if (buf[i] != expected2[i]) {
            printf("Expected %"PRIu8" at pos %zu, but got %"PRIu8".\n", expected2[i], i, buf[i]);
            return 1;
        }
    }

    ret = sb_remove_range(r, 5, 10, &err);
    if (ret < 0) {
        printf("Failed to remove range: %s\n", err.error);
        return -1;
    }
    ret = sb_remove_range(r, 0, 1, &err);
    if (ret < 0) {
        printf("Failed to remove range: %s\n", err.error);
        return -1;
    }
    ret = sb_remove_range(r, 4, 20, &err);
    if (ret < 0) {
        printf("Failed to remove range: %s\n", err.error);
        return -1;
    }
    ret = sb_remove_range(r, 0, 20, &err);
    if (ret < 0) {
        printf("Failed to remove range: %s\n", err.error);
        return -1;
    }
    ret = sb_remove_range(r, 46, 46, &err);
    if (ret < 0) {
        printf("Failed to remove range: %s\n", err.error);
        return -1;
    }

    memset(&buf[0], 0, 50);
    read = sb_read(r, &buf[0], 50, &err);
    if (read != 50) {
        printf("Failed to read sparsebuffer: %s\n", err.error);
        return 1;
    }

    uint8_t expected3[50] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                              4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 0, 0,
                              0 };

    for (size_t i = 0; i < 50; i++) {
        if (buf[i] != expected3[i]) {
            printf("Expected %"PRIu8" at pos %zu, but got %"PRIu8".\n", expected3[i], i, buf[i]);
            return 1;
        }
    }


    sb_free_reader(&r);

    return 0;
}
