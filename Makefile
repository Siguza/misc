CC       ?= cc
FAKEROOT ?= fakeroot
CFLAGS   ?= -Wall -O3

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := bindump.c clz.c dsc_syms.c rand.c vmacho.c xref.c

all: $(SRC:%.c=%)

%: %.c
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)
	$(FAKEROOT) chmod +x $@

mesu: mesu.c
	$(CC) $(CFLAGS) $@.c -o $@ -framework CoreFoundation $(LDFLAGS)

strerror: strerror.c
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS) \
		-framework CoreFoundation -framework Security

install: $(SRC:%.c=%)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -a $^ $(DESTDIR)$(BINDIR)

.PHONY: $(SRC) install
