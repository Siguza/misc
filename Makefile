CC       ?= cc
INSTALL  ?= install
CFLAGS   ?= -Wall -O3

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC  := bindump.c clz.c dsc_syms.c rand.c vmacho.c xref.c
BINS := bindump clz dsc_syms mesu strerror rand vmacho xref

all: $(SRC:%.c=%) mesu strerror

%: %.c
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

mesu: mesu.c
	$(CC) $(CFLAGS) $@.c -o $@ -framework CoreFoundation $(LDFLAGS)

strerror: strerror.c
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS) \
		-framework CoreFoundation -framework Security

install: all
	$(INSTALL) -Dm755 $(BINS) $(DESTDIR)$(BINDIR)

clean:
	rm -r $(BINS)

uninstall: clean
	rm -rf $(addprefix $(DESTDIR)$(BINDIR)/, $(BINS))

.PHONY: all clean install