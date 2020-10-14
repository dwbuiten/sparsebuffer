PREFIX=/usr/local

CFLAGS=-O3 -std=c99 -Wall -Wextra -g -fPIC -I.

all: libsparsebuffer.so

clean:
	@rm -fv *.so *.o sparsebuffertest

libsparsebuffer.so: sparsebuffer.o
	$(CC) -Wl,--version-script,sparsebuffer.v -shared $^ -o $@

install: all
	@install -v sparsebuffer.h $(PREFIX)/include
	@install -v libsparsebuffer.so $(PREFIX)/lib/libsparsebuffer.so.1
	@ln -sv libsparsebuffer.so.1 $(PREFIX)/lib/libsparsebuffer.so

uninstall:
	@rm -fv $(PREFIX)/include/sparsebuffer.h
	@rm -fv $(PREFIX)/lib/libsparsebuffer.so.1
	@rm -fv $(PREFIX)/lib/libsparsebuffer.so

sparsebuffertest: sparsebuffer.o test.o
	$(CC) -o sparsebuffertest $^ -o $@

test: sparsebuffertest
	./sparsebuffertest
