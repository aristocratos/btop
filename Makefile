PREFIX ?= /usr/local
DOCDIR ?= $(PREFIX)/share/btop/doc
CXX = g++
CXXFLAGS = -std=c++20 -pthread
INCLUDES = -I./src

btop: btop.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o bin/btop btop.cpp

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
