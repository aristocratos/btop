PREFIX ?= /usr/local
DOCDIR ?= $(PREFIX)/share/btop/doc
CPP = g++
override CPPFLAGS += -std=c++20 -pthread
OPTFLAG = -O3
INFOFLAGS += -Wall -Wextra -Wno-stringop-overread -pedantic
INCLUDES = -Isrc -Iinclude

btop: btop.cpp
	@mkdir -p bin
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(OPTFLAG) $(INFOFLAGS) -o bin/btop btop.cpp

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p bin/btop $(DESTDIR)$(PREFIX)/bin/btop
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@cp -p README.md $(DESTDIR)$(DOCDIR)
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop

uninstall:
	@rm -rf $(DESTDIR)$(PREFIX)/bin/btop
	@rm -rf $(DESTDIR)$(DOCDIR)
	@rm -rf $(DESTDIR)$(PREFIX)/share/btop

clean:
	rm -rf bin
