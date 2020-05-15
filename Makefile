TARGET = cfm
SRC = cfm.c
CONF = config.h
DEFCONF = config.def.h
MANPAGE = cfm.1
PREFIX ?= /usr/local

CFLAGS += -O3 -std=c11 -Wall -W -pedantic
CPPFLAGS += -D_XOPEN_SOURCE=700

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(CONF) $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(SRC) -o $@

$(CONF):
	@cp -v $(DEFCONF) $(CONF)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -m644 $(MANPAGE) $(DESTDIR)$(PREFIX)/share/man/man1/$(MANPAGE)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/share/man/man1/$(MANPAGE)

clean:
	$(RM) $(TARGET)
