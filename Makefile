INSTALL  ?= install
CFLAGS   ?= -Wall -O3

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin

SRC  := $(wildcard *.c)
BINS := $(SRC:%.c=%)

# mesu and strerror need some extra CFLAGS
mesu_CFLAGS     := -framework CoreFoundation
strerror_CFLAGS := $(mesu_CFLAGS) -framework Security

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $($@_CFLAGS) $(LDFLAGS)

install: all
	$(INSTALL) -Dm755 $(BINS) -t $(DESTDIR)$(BINDIR)

clean:
	rm -r $(BINS)

uninstall: clean
	rm -rf $(addprefix $(DESTDIR)$(BINDIR)/, $(BINS))

.PHONY: all clean install
