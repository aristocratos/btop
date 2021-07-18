PREFIX ?= /usr/local
DOCDIR ?= $(PREFIX)/share/btop/doc

#Compiler and Linker
CXX := g++

#Try to make sure we are using GCC/G++ version 11 or later
CXX_VERSION = $(shell $(CXX) -dumpversion)
ifneq ($(shell test $(CXX_VERSION) -ge 11; echo $$?),0)
	ifneq ($(shell command -v g++-11),)
		CXX := g++-11
	endif
endif

#The Target Binary Program
TARGET		:= btop

#The Directories, Source, Includes, Objects and Binary
SRCDIR		:= src
INCDIR		:= include
BUILDDIR	:= obj
TARGETDIR	:= bin
SRCEXT		:= cpp
DEPEXT		:= d
OBJEXT		:= o

#Flags, Libraries and Includes
CXXFLAGS	:= -std=c++20 -pthread -O3 -Wall -Wextra -Wno-stringop-overread -pedantic
INC			:= -I$(INCDIR) -I$(SRCDIR)

SOURCES		:= $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS		:= $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#Default Make
all: directories $(TARGET)

#Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@rm -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
distclean: clean
	@rm -rf $(TARGETDIR)

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p $(TARGETDIR)/btop $(DESTDIR)$(PREFIX)/bin/btop
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@cp -p README.md $(DESTDIR)$(DOCDIR)
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop

#Set suid bit for btop to root, will make btop run with admin privileges regardless of actual user
su-setuid:
	@su --session-command "chown root:root $(DESTDIR)$(PREFIX)/bin/btop && chmod 4755 $(DESTDIR)$(PREFIX)/bin/btop" root

uninstall:
	@rm -rf $(DESTDIR)$(PREFIX)/bin/btop
	@rm -rf $(DESTDIR)$(DOCDIR)
	@rm -rf $(DESTDIR)$(PREFIX)/share/btop

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#Link
btop: $(OBJECTS)
	$(CXX) -o $(TARGETDIR)/btop $^ -pthread

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<
	@$(CXX) $(CXXFLAGS) $(INC) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#Non-File Targets
.PHONY: all
