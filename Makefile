SRCDIR = src
OBJDIR = obj

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%.o,$(SRC))
DEP = $(addsuffix .d,$(OBJ))

TARGET = cfm

CONF = config.h
DEFCONF = config.def.h
MANPAGE = cfm.1
PREFIX ?= /usr/local

CFLAGS += -O3 -std=c11 -Wall -W -pedantic
CPPFLAGS += -D_XOPEN_SOURCE=700
LDFLAGS = -s -fwhole-program

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.c.o: $(SRCDIR)/%.c | $(OBJDIR) $(CONF)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $?

$(CONF):
	@cp -v $(DEFCONF) $(CONF)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

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
	$(RM) -r $(OBJDIR)
