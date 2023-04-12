INSTALL  ?= install
CFLAGS   ?= -Wall -O3

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin

SRC  := $(wildcard *.c)
BINS := $(SRC:%.c=%)

# mesu and strerror need some extra CFLAGS
mesu_CFLAGS     := -framework CoreFoundation
strerror_CFLAGS := -framework CoreFoundation -framework Security

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $($@_CFLAGS) $(LDFLAGS)

install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m755 $(BINS) $(DESTDIR)$(BINDIR)

clean:
	rm -r $(BINS)

uninstall: clean
	rm -rf $(addprefix $(DESTDIR)$(BINDIR)/, $(BINS))

.PHONY: all clean install
