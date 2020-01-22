TARGET = cfm
MANPAGE = cfm.1
PREFIX ?= /usr/local

CFLAGS = -O3 -std=c11

.PHONY: all install clean

all: $(TARGET)

install:
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -m644 $(MANPAGE) $(DESTDIR)$(PREFIX)/share/man/man1/$(MANPAGE)

clean:
	$(RM) -f $(OUTPUT)
