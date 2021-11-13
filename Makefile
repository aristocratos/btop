#* Btop++ makefile v1.4

BANNER  = \n \033[38;5;196m██████\033[38;5;240m╗ \033[38;5;196m████████\033[38;5;240m╗ \033[38;5;196m██████\033[38;5;240m╗ \033[38;5;196m██████\033[38;5;240m╗\n \033[38;5;160m██\033[38;5;239m╔══\033[38;5;160m██\033[38;5;239m╗╚══\033[38;5;160m██\033[38;5;239m╔══╝\033[38;5;160m██\033[38;5;239m╔═══\033[38;5;160m██\033[38;5;239m╗\033[38;5;160m██\033[38;5;239m╔══\033[38;5;160m██\033[38;5;239m╗   \033[38;5;160m██\033[38;5;239m╗    \033[38;5;160m██\033[38;5;239m╗\n \033[38;5;124m██████\033[38;5;238m╔╝   \033[38;5;124m██\033[38;5;238m║   \033[38;5;124m██\033[38;5;238m║   \033[38;5;124m██\033[38;5;238m║\033[38;5;124m██████\033[38;5;238m╔╝ \033[38;5;124m██████\033[38;5;238m╗\033[38;5;124m██████\033[38;5;238m╗\n \033[38;5;88m██\033[38;5;237m╔══\033[38;5;88m██\033[38;5;237m╗   \033[38;5;88m██\033[38;5;237m║   \033[38;5;88m██\033[38;5;237m║   \033[38;5;88m██\033[38;5;237m║\033[38;5;88m██\033[38;5;237m╔═══╝  ╚═\033[38;5;88m██\033[38;5;237m╔═╝╚═\033[38;5;88m██\033[38;5;237m╔═╝\n \033[38;5;52m██████\033[38;5;236m╔╝   \033[38;5;52m██\033[38;5;236m║   ╚\033[38;5;52m██████\033[38;5;236m╔╝\033[38;5;52m██\033[38;5;236m║        ╚═╝    ╚═╝\n \033[38;5;235m╚═════╝    ╚═╝    ╚═════╝ ╚═╝      \033[1;3;38;5;240mMakefile v1.4\033[0m

override BTOP_VERSION := $(shell head -n100 src/btop.cpp 2>/dev/null | grep "Version =" | cut -f2 -d"\"" || echo " unknown")
override TIMESTAMP := $(shell date +%s 2>/dev/null || echo "0")
ifeq ($(shell command -v gdate >/dev/null; echo $$?),0)
	DATE_CMD := gdate
else
	DATE_CMD := date
endif

ifneq ($(QUIET),true)
	override PRE := info info-quiet
	override QUIET := false
else
	override PRE := info-quiet
endif

PREFIX ?= /usr/local

#? Detect PLATFORM and ARCH from uname/gcc if not set
PLATFORM ?= $(shell uname -s || echo unknown)
ifneq ($(filter unknown Darwin, $(PLATFORM)),)
	override PLATFORM := $(shell $(CXX) -dumpmachine | awk -F"-" '{ print (NF==4) ? $$3 : $$2 }')
	ifeq ($(PLATFORM),apple)
		override PLATFORM := macos
	endif
endif
ifeq ($(shell uname -v | grep ARM64 >/dev/null 2>&1; echo $$?),0)
	ARCH ?= arm64
else
	ARCH ?= $(shell $(CXX) -dumpmachine | cut -d "-" -f 1)
endif

override PLATFORM_LC := $(shell echo $(PLATFORM) | tr '[:upper:]' '[:lower:]')

#? Any flags added to TESTFLAGS must not contain whitespace for the testing to work
override TESTFLAGS := -fexceptions -fstack-clash-protection -fcf-protection
ifneq ($(PLATFORM) $(ARCH),macos arm64)
	override TESTFLAGS += -fstack-protector
endif

ifeq ($(STATIC),true)
	override ADDFLAGS += -DSTATIC_BUILD -static -static-libgcc -static-libstdc++ -Wl,--fatal-warnings
endif

ifeq ($(STRIP),true)
	override ADDFLAGS += -s
endif

#? Compiler and Linker
ifeq ($(shell command -v g++-11 >/dev/null; echo $$?),0)
	CXX := g++-11
else ifeq ($(shell command -v g++11 >/dev/null; echo $$?),0)
	CXX := g++11
else ifeq ($(shell command -v g++ >/dev/null; echo $$?),0)
	CXX := g++
endif
override CXX_VERSION := $(shell $(CXX) -dumpfullversion -dumpversion || echo 0)

#? Try to make sure we are using GCC/G++ version 11 or later if not instructed to use g++-10
ifeq ($(CXX),g++)
	ifeq ($(shell g++ --version | grep clang >/dev/null 2>&1; echo $$?),0)
		V_MAJOR := 0
	else
		V_MAJOR := $(shell echo $(CXX_VERSION) | cut -f1 -d".")
	endif
	ifneq ($(shell test $(V_MAJOR) -ge 11; echo $$?),0)
		ifeq ($(shell command -v g++-11 >/dev/null; echo $$?),0)
			override CXX := g++-11
			override CXX_VERSION := $(shell $(CXX) -dumpfullversion -dumpversion || echo 0)
		endif
	endif
endif

#? Pull in platform specific source files and get thread count
ifeq ($(PLATFORM_LC),linux)
	PLATFORM_DIR := linux
	THREADS	:= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
	SU_GROUP := root
else ifeq ($(PLATFORM_LC),freebsd)
	PLATFORM_DIR := freebsd
	THREADS	:= $(shell getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)
	SU_GROUP := root
	override ADDFLAGS += -lstdc++ -lm -lkvm -Wl,-rpath=/usr/local/lib/gcc11
	export MAKE = gmake
else ifeq ($(PLATFORM_LC),macos)
	PLATFORM_DIR := osx
	THREADS	:= $(shell sysctl -n hw.ncpu || echo 1)
	override ADDFLAGS += -framework IOKit -framework CoreFoundation -Wno-format-truncation
	SU_GROUP := wheel
else
$(error $(shell printf "\033[1;91mERROR: \033[97mUnsupported platform ($(PLATFORM))\033[0m"))
endif

#? Use all CPU cores (will only be set if using Make 4.3+)
MAKEFLAGS := --jobs=$(THREADS)
ifeq ($(THREADS),1)
	override THREADS := auto
endif

#? The Directories, Source, Includes, Objects and Binary
SRCDIR		:= src
INCDIR		:= include
BUILDDIR	:= obj
TARGETDIR	:= bin
SRCEXT		:= cpp
DEPEXT		:= d
OBJEXT		:= o

#? Filter out unsupported compiler flags
override GOODFLAGS := $(foreach flag,$(TESTFLAGS),$(strip $(shell echo "int main() {}" | $(CXX) -o /dev/null $(flag) -x c++ - >/dev/null 2>&1 && echo $(flag) || true)))

#? Flags, Libraries and Includes
override REQFLAGS   := -std=c++20
WARNFLAGS			:= -Wall -Wextra -pedantic
OPTFLAGS			:= -O2 -ftree-loop-vectorize -flto=$(THREADS)
LDCXXFLAGS			:= -pthread -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS $(GOODFLAGS) $(ADDFLAGS)
override CXXFLAGS	+= $(REQFLAGS) $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
override LDFLAGS	+= $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
INC					:= -I$(INCDIR) -I$(SRCDIR)
SU_USER				:= root

SOURCES	:= $(shell find $(SRCDIR) -maxdepth 1 -type f -name *.$(SRCEXT))

SOURCES += $(shell find $(SRCDIR)/$(PLATFORM_DIR) -type f -name *.$(SRCEXT))

OBJECTS	:= $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#? Default Make
all: $(PRE) directories btop

info:
	@printf " $(BANNER)\n"
	@printf "\033[1;92mPLATFORM   \033[1;93m?| \033[0m$(PLATFORM)\n"
	@printf "\033[1;96mARCH       \033[1;93m?| \033[0m$(ARCH)\n"
	@printf "\033[1;93mCXX        \033[1;93m?| \033[0m$(CXX) \033[1;93m(\033[97m$(CXX_VERSION)\033[93m)\n"
	@printf "\033[1;94mTHREADS    \033[1;94m:| \033[0m$(THREADS)\n"
	@printf "\033[1;92mREQFLAGS   \033[1;91m!| \033[0m$(REQFLAGS)\n"
	@printf "\033[1;91mWARNFLAGS  \033[1;94m:| \033[0m$(WARNFLAGS)\n"
	@printf "\033[1;94mOPTFLAGS   \033[1;94m:| \033[0m$(OPTFLAGS)\n"
	@printf "\033[1;93mLDCXXFLAGS \033[1;94m:| \033[0m$(LDCXXFLAGS)\n"
	@printf "\033[1;95mCXXFLAGS   \033[1;92m+| \033[0;37m\$$(\033[92mREQFLAGS\033[37m) \$$(\033[93mLDCXXFLAGS\033[37m) \$$(\033[94mOPTFLAGS\033[37m) \$$(\033[91mWARNFLAGS\033[37m)\n"
	@printf "\033[1;95mLDFLAGS    \033[1;92m+| \033[0;37m\$$(\033[93mLDCXXFLAGS\033[37m) \$$(\033[94mOPTFLAGS\033[37m) \$$(\033[91mWARNFLAGS\033[37m)\n"

info-quiet:
	@sleep 0.1 2>/dev/null || true
	@printf "\n\033[1;92mBuilding btop++ \033[91m(\033[97mv$(BTOP_VERSION)\033[91m) \033[93m$(PLATFORM) \033[96m$(ARCH)\033[0m\n"

help:
	@printf " $(BANNER)\n"
	@printf "\033[1;97mbtop++ makefile\033[0m\n"
	@printf "usage: make [argument]\n\n"
	@printf "arguments:\n"
	@printf "  all          Compile btop (default argument)\n"
	@printf "  clean        Remove built objects\n"
	@printf "  distclean    Remove built objects and binaries\n"
	@printf "  install      Install btop++ to \$$PREFIX ($(PREFIX))\n"
	@printf "  setuid       Set installed binary owner/group to \$$SU_USER/\$$SU_GROUP ($(SU_USER)/$(SU_GROUP)) and set SUID bit\n"
	@printf "  uninstall    Uninstall btop++ from \$$PREFIX\n"
	@printf "  info         Display information about Environment,compiler and linker flags\n"

#? Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)/$(PLATFORM_DIR)

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
	@printf "\033[1;92mInstalling themes to: \033[1;97m$(DESTDIR)$(PREFIX)/share/btop/themes\033[0m\n"
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop

#? Set SUID bit for btop as $SU_USER in $SU_GROUP
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
.ONESHELL:
btop: $(OBJECTS)
	@sleep 0.2 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || printf "\n\033[1;92mLinking and optimizing binary\033[37m...\033[0m\n"
	@$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS) || exit 1
	@printf "\033[1;92m-> \033[1;37m$(TARGETDIR)/btop \033[100D\033[35C\033[1;93m(\033[1;97m$$(du -ah $(TARGETDIR)/btop | cut -f1)iB\033[1;93m) \033[92m(\033[97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $${TSTAMP} 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo '')\033[92m)\033[0m\n"
	@printf "\n\033[1;92mBuild complete in \033[92m(\033[97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $(TIMESTAMP) 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo "unknown")\033[92m)\033[0m\n"

#? Compile
.ONESHELL:
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@sleep 0.3 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || printf "\033[1;97mCompiling $<\033[0m\n"
	@$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $< || exit 1
	@$(CXX) $(CXXFLAGS) $(INC) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT) >/dev/null || exit 1
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp
	@printf "\033[1;92m-> \033[1;37m$@ \033[100D\033[35C\033[1;93m(\033[1;97m$$(du -ah $@ | cut -f1)iB\033[1;93m) \033[92m(\033[97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $${TSTAMP} 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo '')\033[92m)\033[0m\n"

#? Non-File Targets
.PHONY: all msg help pre
