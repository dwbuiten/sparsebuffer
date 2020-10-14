sparsebuffer
============

A C library that implements a reader where only ranges explicitly loaded into it are kept
in memory, and available to be read. All gaps in between these ranges read zeroes.

This is a C version of a Go package written primarily by Justin Ruggles (@justinruggles),
and used in Vimeo's edge CDN packager.

How to use it?
--------------

Simply copy `sparsebuffer.c` and `sparsebuffer.h` into your project, or use this repository
as a git submodule.

All API documentation lives in `sparsebuffer.h`.

There is also a Makefile provided for building a simple shared library on Linux. It
should be straightforward to build for other OSes; all public symbols are namespaced
with `sb_`. All public enums and types are namespaced with `SB`.

Example
-------

```C
char errbuf[1024];
SBError err = { &errbuf[0], 1024 };

/* Create a new reader of size 10. */
SBReader *r = sb_new_reader(10, &err);
if (r == NULL) {
    printf("Couldn't create reader: %s\n", err->error);
    abort();
}

/* Buffer now looks like: [0 0 0 0 0 0 0 0 0 0] */

uint8_t buf[2] = { 1, 1 };
int ret = sb_load_range(r, 3, &buf[0], 2, &err);
if (ret < 0) {
    printf("Failed to load range: %s\n", err->error);
    abort();
}

/* Buffer now looks like: [0 0 0 1 1 0 0 0 0 0] */
/* Only two bytes are allocated. */

uint8_t outbuf[4];
size_t bytesread = sb_read(r, &buf[0], 4, &err);
if (bytesread != 4) {
    printf("Failed to read: %s\n", err->error);
    abort();
}

/* outbuf looks like: [0 0 0 1] */

ret = sb_remove_range(r, 4, 4, &err);
if (ret < 0) {
    printf("Could not remove range: %s\n", err->error);
    abort();
}

/* Buffer now looks like: [0 0 0 1 0 0 0 0 0 0] */
/* Only one byte is allocated. */

sb_free_reader(&r);
```
