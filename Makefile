#* Btop++ makefile v1.0

BANNER  = \n \033[38;5;196m██████\033[38;5;240m╗ \033[38;5;196m████████\033[38;5;240m╗ \033[38;5;196m██████\033[38;5;240m╗ \033[38;5;196m██████\033[38;5;240m╗\n \033[38;5;160m██\033[38;5;239m╔══\033[38;5;160m██\033[38;5;239m╗╚══\033[38;5;160m██\033[38;5;239m╔══╝\033[38;5;160m██\033[38;5;239m╔═══\033[38;5;160m██\033[38;5;239m╗\033[38;5;160m██\033[38;5;239m╔══\033[38;5;160m██\033[38;5;239m╗   \033[38;5;160m██\033[38;5;239m╗    \033[38;5;160m██\033[38;5;239m╗\n \033[38;5;124m██████\033[38;5;238m╔╝   \033[38;5;124m██\033[38;5;238m║   \033[38;5;124m██\033[38;5;238m║   \033[38;5;124m██\033[38;5;238m║\033[38;5;124m██████\033[38;5;238m╔╝ \033[38;5;124m██████\033[38;5;238m╗\033[38;5;124m██████\033[38;5;238m╗\n \033[38;5;88m██\033[38;5;237m╔══\033[38;5;88m██\033[38;5;237m╗   \033[38;5;88m██\033[38;5;237m║   \033[38;5;88m██\033[38;5;237m║   \033[38;5;88m██\033[38;5;237m║\033[38;5;88m██\033[38;5;237m╔═══╝  ╚═\033[38;5;88m██\033[38;5;237m╔═╝╚═\033[38;5;88m██\033[38;5;237m╔═╝\n \033[38;5;52m██████\033[38;5;236m╔╝   \033[38;5;52m██\033[38;5;236m║   ╚\033[38;5;52m██████\033[38;5;236m╔╝\033[38;5;52m██\033[38;5;236m║        ╚═╝    ╚═╝\n \033[38;5;235m╚═════╝    ╚═╝    ╚═════╝ ╚═╝

BTOP_VERSION = $(shell head -n100 src/btop.cpp 2>/dev/null | grep "Version =" | cut -f2 -d"\"" || echo " unknown")
TIMESTAMP = $(shell date +%s)

PREFIX ?= /usr/local

#? Compiler and Linker
CXX ?= g++
CXX_VERSION = $(shell $(CXX) -dumpfullversion -dumpversion || echo 0)

#? Try to make sure we are using GCC/G++ version 11 or later if not instructed to use g++-10
ifneq ($(CXX),g++-10)
	V_MAJOR = $(shell echo $(CXX_VERSION) | cut -f1 -d"." || echo 0)
	ifneq ($(shell test $(V_MAJOR) -ge 11; echo $$?),0)
		ifeq ($(shell command -v g++-11 >/dev/null; echo $$?),0)
			override CXX = g++-11
		endif
	endif
endif

#? Only enable fcf-protection if on x86_64
ARCH = $(shell uname -p ||true)
ifeq ($(ARCH),x86_64)
	ADDFLAGS = -fcf-protection
endif
ifeq ($(ARCH),unknown)
	ARCH = $(shell uname -m ||true)
endif
PLATFORM = $(shell uname -s ||true)

#? Use all CPU cores (will only be set if using Make >=4.3)
MAKEFLAGS := --jobs=$(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

#? The Directories, Source, Includes, Objects and Binary
SRCDIR		:= src
INCDIR		:= include
BUILDDIR	:= obj
TARGETDIR	:= bin
SRCEXT		:= cpp
DEPEXT		:= d
OBJEXT		:= o

#? Flags, Libraries and Includes
REQFLAGS			:= -std=c++20
WARNFLAGS			:= -Wall -Wextra -Wno-stringop-overread -pedantic -pedantic-errors -Wfatal-errors
OPTFLAGS			:= -O2 -ftree-loop-vectorize
override LDCXXFLAGS	+= -pthread -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS -fexceptions -fstack-protector -fstack-clash-protection -flto $(ADDFLAGS)
override CXXFLAGS	+= $(REQFLAGS) $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
override LDFLAGS	+= $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
INC					:= -I$(INCDIR) -I$(SRCDIR)
SU_USER				:= root
SU_GROUP			:= root

SOURCES		:= $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS		:= $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#? Default Make
all: msg directories btop

msg:
	@printf " $(BANNER)\n"
	@printf "\033[1;97mCompiler   : \033[0m$(CXX) ($(CXX_VERSION))\n"
	@printf "\033[1;97mREQFLAGS   : \033[0m$(REQFLAGS)\n"
	@printf "\033[1;97mWARNFLAGS  : \033[0m$(WARNFLAGS)\n"
	@printf "\033[1;97mOPTFLAGS   : \033[0m$(OPTFLAGS)\n"
	@printf "\033[1;97mLDCXXFLAGS : \033[0m$(LDCXXFLAGS)\n"

	@printf "\n\033[1;92mBuilding btop++ v$(BTOP_VERSION) for $(PLATFORM) ($(ARCH))\033[0m\n"

help:
	@printf "\033[1;97mbtop++ makefile\033[0m\n"
	@printf "usage: make [argument]\n\n"
	@printf "arguments:\n"
	@printf "  all          Compile btop (default argument)\n"
	@printf "  clean        Remove built objects\n"
	@printf "  distclean    Remove built objects and binaries\n"
	@printf "  install      Install btop++ to \$$PREFIX\n"
	@printf "  setuid       Set installed binary owner/group to \$$SU_USER/\$$SU_OWNER and set SUID bit\n"
	@printf "  uninstall    Uninstall btop++ from \$$PREFIX\n"

#? Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#? Clean only Objects
clean:
	@printf "\033[1;91mRemoving: \033[1;97mbuilt objects...\033[0m\n"
	@rm -rf $(BUILDDIR)

#? Clean Objects and Binaries
distclean: clean
	@printf "\033[1;91mRemoving: \033[1;97mbuilt binaries...\033[0m\n"
	@rm -rf $(TARGETDIR)

install:
	@printf "\033[1;92mInstalling binary to: \033[1;97m$(DESTDIR)$(PREFIX)/bin/btop\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p $(TARGETDIR)/btop $(DESTDIR)$(PREFIX)/bin/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop
	@printf "\033[1;92mInstalling doc to: \033[1;97m$(DESTDIR)$(PREFIX)/share/btop\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/btop
	@cp -p README.md $(DESTDIR)$(PREFIX)/share/btop
	@printf "\033[1;92mInstalling themes to: \033[1;97m$(DESTDIR)$(PREFIX)/share/btop/themes\n"
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop

#? Set suid bit for btop for $SU_USER in SU_GROUP, will make btop run with (root by default) privileges regardless of actual user
setuid:
	@printf "\033[1;97mFile: $(DESTDIR)$(PREFIX)/bin/btop\n"
	@printf "\033[1;92mSetting owner \033[1;97m$(SU_USER):$(SU_GROUP)\033[0m\n"
	@chown $(SU_USER):$(SU_GROUP) $(DESTDIR)$(PREFIX)/bin/btop
	@printf "\033[1;92mSetting SUID bit\033[0m\n"
	@chmod u+s $(DESTDIR)$(PREFIX)/bin/btop

uninstall:
	@printf "\033[1;91mRemoving: \033[1;97m$(DESTDIR)$(PREFIX)/bin/btop\033[0m\n"
	@rm -rf $(DESTDIR)$(PREFIX)/bin/btop
	@printf "\033[1;91mRemoving: \033[1;97m$(DESTDIR)$(PREFIX)/share/btop\033[0m\n"
	@rm -rf $(DESTDIR)$(PREFIX)/share/btop

#? Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#? Link
btop: $(OBJECTS)
	@sleep 0.1 2>/dev/null || true
	@printf "\n\033[1;92mLinking and optimizing binary\033[0m\n"
	@$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS)
	@printf "\033[1;97m./$(TARGETDIR)/btop ($$(du -ah $(TARGETDIR)/btop | cut -f1)iB)\033[0m\n"
	@printf "\n\033[1;92mBuild complete in (\033[1;97m$$(date -d @$$(expr $$(date +%s) - $(TIMESTAMP)) -u +%Mm:%Ss)\033[1;92m)\033[0m\n"

#? Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@sleep 0.1 2>/dev/null || true
	@printf "\033[1;97mCompiling $< \n"
	@$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<
	@$(CXX) $(CXXFLAGS) $(INC) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT) >/dev/null
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#? Non-File Targets
.PHONY: all msg help
