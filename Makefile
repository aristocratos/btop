PREFIX ?= /usr/local
DOCDIR ?= $(PREFIX)/share/btop/doc
CXX=g++
CXXFLAGS=-std=c++20 -pthread

btop: btop.cpp
	$(CXX) $(CXXFLAGS) -o btop btop.cpp

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p btop $(DESTDIR)$(PREFIX)/bin/btop
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@cp -p README.md $(DESTDIR)$(DOCDIR)
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop

uninstall:
	@rm -rf $(DESTDIR)$(PREFIX)/bin/btop
	@rm -rf $(DESTDIR)$(DOCDIR)
	@rm -rf $(DESTDIR)$(PREFIX)/share/btop

distclean:
	rm -f btop
