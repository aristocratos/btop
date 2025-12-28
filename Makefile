#* Btop++ makefile v1.6

BANNER_FILE := ansi_banner.utf8
BANNER = $(shell cat "$(BANNER_FILE)")

E = \033[
RST = $(E)0m
BLD = $(E)1m

CUR_LEFT = $(E)$(1)D
CUR_RIGHT = $(E)$(1)C

# LT_GRAY    = $(E)30m
# LT_RED     = $(E)31m
# LT_GREEN   = $(E)32m
# LT_YELLOW  = $(E)33m
# LT_BLUE    = $(E)34m
# LT_MAGENTA = $(E)35m
# LT_CYAN    = $(E)36m
# LT_WHITE   = $(E)37m

# GRAY    = $(E)90m
RED     = $(E)91m
GREEN   = $(E)92m
YELLOW  = $(E)93m
BLUE    = $(E)94m
MAGENTA = $(E)95m
CYAN    = $(E)96m
WHITE   = $(E)97m

# B_GRAY    = $(BLD)$(GRAY)
B_RED     = $(BLD)$(RED)
B_GREEN   = $(BLD)$(GREEN)
B_YELLOW  = $(BLD)$(YELLOW)
B_BLUE    = $(BLD)$(BLUE)
B_MAGENTA = $(BLD)$(MAGENTA)
B_CYAN    = $(BLD)$(CYAN)
B_WHITE   = $(BLD)$(WHITE)

cecho = printf "%b" "$(1)"

# line-based colors (with newline)
red     = $(call cecho,$(B_RED)$(1)$(RST)$(2)\n)
green   = $(call cecho,$(B_GREEN)$(1)$(RST)$(2)\n)
yellow  = $(call cecho,$(B_YELLOW)$(1)$(RST)$(2)\n)
blue    = $(call cecho,$(B_BLUE)$(1)$(RST)$(2)\n)
magenta = $(call cecho,$(B_MAGENTA)$(1)$(RST)$(2)\n)
cyan    = $(call cecho,$(B_CYAN)$(1)$(RST)$(2)\n)
white   = $(call cecho,$(B_WHITE)$(1)$(RST)$(2)\n)
# inline colors (no newline)
red_i = $(call cecho,$(B_RED)$(1)$(RST)$(2))

COMMA := ,

file_with_size = $(WHITE)$(1) $(2)$(YELLOW)($(WHITE)$$(du -ah "$(1)" 2>/dev/null |cut -f1)iB$(YELLOW))
step_duration = $$($(DATE_CMD) -d @$$(expr $$($(DATE_CMD) +%s 2>/dev/null || echo "0") - $(1) 2>/dev/null) -u +%Mm:%Ss 2>/dev/null |sed 's/^00m://' || echo 'unk')


override BTOP_VERSION := $(shell head -n100 src/btop.cpp 2>/dev/null | grep "Version =" | cut -f2 -d"\"" || echo " unknown")
override TIMESTAMP := $(shell date +%s 2>/dev/null || echo "0")
override DATESTAMP := $(shell date '+%Y-%m-%d %H:%M:%S' || echo "5 minutes ago")
ifeq ($(shell command -v gdate >/dev/null; echo $$?),0)
	DATE_CMD := gdate
else
	DATE_CMD := date
endif

ifneq ($(QUIET),true)
	override QUIET := false
endif

OLDCXX := $(CXXFLAGS)
OLDLD := $(LDFLAGS)

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

#? GPU Support
ifeq ($(PLATFORM_LC)$(ARCH),linuxx86_64)
	ifneq ($(STATIC),true)
		GPU_SUPPORT := true
		INTEL_GPU_SUPPORT := true
	endif
endif
ifneq ($(GPU_SUPPORT),true)
	GPU_SUPPORT := false
endif

ifeq ($(GPU_SUPPORT),true)
	override ADDFLAGS += -DGPU_SUPPORT
endif

#? Compiler and Linker
ifeq ($(shell $(CXX) --version | grep clang >/dev/null 2>&1; echo $$?),0)
	override CXX_IS_CLANG := true
endif
override CXX_VERSION := $(shell $(CXX) -dumpfullversion -dumpversion || echo 0)
override CXX_VERSION_MAJOR := $(shell echo $(CXX_VERSION) | cut -d '.' -f 1)

ifeq ($(DEBUG),true)
	override ADDFLAGS += -DBTOP_DEBUG
endif

#? Any flags added to TESTFLAGS must not contain whitespace for the testing to work
override TESTFLAGS := -fexceptions -fstack-clash-protection -fcf-protection
ifneq ($(PLATFORM) $(ARCH),macos arm64)
	override TESTFLAGS += -fstack-protector
endif

ifeq ($(STATIC),true)
	ifeq ($(CXX_IS_CLANG),true)
		ifeq ($(shell $(CXX) -print-target-triple | grep gnu >/dev/null; echo $$?),0)
$(error $(call red_i,ERROR: $(WHITE)$(CXX) can't statically link glibc))
		endif
	endif

	ifeq ($(PLATFORM_LC),$(filter $(PLATFORM_LC),freebsd linux midnightbsd))
		override ADDFLAGS += -DSTATIC_BUILD -static
	else
		ifeq ($(CXX_IS_CLANG),false)
			override ADDFLAGS += -static-libgcc -static-libstdc++
		endif
	endif
endif

ifeq ($(STRIP),true)
	override ADDFLAGS += -s
endif

ifeq ($(VERBOSE),true)
	override VERBOSE := false
else
	override VERBOSE := true
endif

#? Pull in platform specific source files and get thread count
ifeq ($(PLATFORM_LC),linux)
	PLATFORM_DIR := linux
	THREADS	:= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
	SU_GROUP := root
else ifeq ($(PLATFORM_LC),$(filter $(PLATFORM_LC),freebsd midnightbsd))
	PLATFORM_DIR := freebsd
	THREADS	:= $(shell getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)
	SU_GROUP := wheel
	override ADDFLAGS += -lm -lkvm -ldevstat
	ifeq ($(STATIC),true)
		override ADDFLAGS += -lelf -Wl,--eh-frame-hdr
	endif

 	ifeq ($(CXX_IS_CLANG),false)
		override ADDFLAGS += -lstdc++ -Wl,rpath=/usr/local/lib/gcc$(CXX_VERSION_MAJOR)
	endif
	export MAKE = gmake
else ifeq ($(PLATFORM_LC),macos)
	PLATFORM_DIR := osx
	THREADS	:= $(shell sysctl -n hw.ncpu || echo 1)
	override ADDFLAGS += -framework IOKit -framework CoreFoundation -Wno-format-truncation
	SU_GROUP := wheel
else ifeq ($(PLATFORM_LC),openbsd)
	PLATFORM_DIR := openbsd
	THREADS	:= $(shell sysctl -n hw.ncpu || echo 1)
	override ADDFLAGS += -lkvm -static-libstdc++
	export MAKE = gmake
	SU_GROUP := wheel
else ifeq ($(PLATFORM_LC),netbsd)
	PLATFORM_DIR := netbsd
	THREADS	:= $(shell sysctl -n hw.ncpu || echo 1)
	override ADDFLAGS += -lkvm -lprop
	export MAKE = gmake
	SU_GROUP := wheel
else
$(error $(call red_i,ERROR: $(WHITE)Unsupported platform ($(PLATFORM))))
endif

#? Use all CPU cores (will only be set if using Make 4.3+)
MAKEFLAGS := --jobs=$(THREADS)
ifeq ($(THREADS),1)
	override THREADS := auto
endif

#? LTO command line
ifeq ($(BUILD_TYPE),Release)
	ifeq ($(CXX_IS_CLANG),true)
		LTO := -flto=thin
	else
		LTO := -flto=$(THREADS)
	endif
endif

GIT_COMMIT := $(shell git rev-parse --short HEAD 2> /dev/null || true)
CONFIGURE_COMMAND := $(MAKE) STATIC=$(STATIC)
ifeq ($(PLATFORM_LC),linux)
	CONFIGURE_COMMAND +=  GPU_SUPPORT=$(GPU_SUPPORT) RSMI_STATIC=$(RSMI_STATIC)
endif

#? The Directories, Source, Includes, Objects and Binary
SRCDIR		:= src
INCDIRS		:= include $(wildcard lib/**/include)
BUILDDIR	:= obj
TARGETDIR	:= bin
SRCEXT		:= cpp
DEPEXT		:= d
OBJEXT		:= o

#? Filter out unsupported compiler flags
override GOODFLAGS := $(foreach flag,$(TESTFLAGS),$(strip $(shell echo "int main() {}" | $(CXX) -o /dev/null $(flag) -x c++ - >/dev/null 2>&1 && echo $(flag) || true)))

#? Flags, Libraries and Includes
override REQFLAGS   := -std=c++23
WARNFLAGS			:= -Wall -Wextra -pedantic
OPTFLAGS			:= -O2 $(LTO)
LDCXXFLAGS			:= -pthread -DFMT_HEADER_ONLY -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG -D_FILE_OFFSET_BITS=64 $(GOODFLAGS) $(ADDFLAGS)
override CXXFLAGS	+= $(REQFLAGS) $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
override LDFLAGS	+= $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
INC					:= $(foreach incdir,$(INCDIRS),-isystem $(incdir)) -I$(SRCDIR) -I$(BUILDDIR)
SU_USER				:= root

ifdef DEBUG
	override OPTFLAGS := -O0 -g
endif

SOURCES	:= $(sort $(shell find $(SRCDIR) -maxdepth 1 -type f -name *.$(SRCEXT)))

SOURCES += $(sort $(shell find $(SRCDIR)/$(PLATFORM_DIR) -maxdepth 1 -type f -name *.$(SRCEXT)))

OBJECTS	:= $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

ifeq ($(GPU_SUPPORT)$(INTEL_GPU_SUPPORT),truetrue)
	IGT_OBJECTS := $(BUILDDIR)/igt_perf.c.o $(BUILDDIR)/intel_device_info.c.o $(BUILDDIR)/intel_name_lookup_shim.c.o $(BUILDDIR)/intel_gpu_top.c.o
	OBJECTS += $(IGT_OBJECTS)
	SHOW_CC_INFO = false
	CC_VERSION := $(shell $(CC) -dumpfullversion -dumpversion || echo 0)
else
	SHOW_CC_INFO = true
endif

#? Setup percentage progress
SOURCE_COUNT := $(words $(OBJECTS))

ifeq ($(shell find $(BUILDDIR) -type f -newermt "$(DATESTAMP)" -name *.o >/dev/null 2>&1; echo $$?),0)
	ifneq ($(wildcard $(BUILDDIR)/.*),)
		SKIPPED_SOURCES := $(foreach fname,$(SOURCES),$(shell find $(BUILDDIR) -type f -newer $(fname) -name *.o | grep "$(basename $(notdir $(fname))).o" 2>/dev/null))
		override SOURCE_COUNT := $(shell expr $(SOURCE_COUNT) - $(words $(SKIPPED_SOURCES)))
		ifeq ($(SOURCE_COUNT),0)
			override SOURCE_COUNT = $(words $(SOURCES))
		endif
	endif
	PROGRESS = expr $$(find $(BUILDDIR) -type f -newermt "$(DATESTAMP)" -name *.o | wc -l || echo 1) '*' 90 / $(SOURCE_COUNT) | cut -c1-2
else
	PROGRESS = expr $$(find $(BUILDDIR) -type f -name *.o | wc -l || echo 1) '*' 90 / $(SOURCE_COUNT) | cut -c1-2
endif

ifeq ($(VERBOSE),true)
	# Doesn't work with `&>`
	override SUPPRESS := > /dev/null 2> /dev/null
else
	override SUPPRESS :=
endif

#? Default Make
.ONESHELL:
all: | info rocm_smi info-quiet directories btop.1 config.h btop

ifneq ($(QUIET),true)
info:
	@printf " $(BANNER)\n"
	@$(call green  ,PLATFORM     $(YELLOW)?| ,$(PLATFORM))
	@$(call cyan   ,ARCH         $(YELLOW)?| ,$(ARCH))
	@$(call magenta,GPU_SUPPORT  $(BLUE):| ,$(GPU_SUPPORT))
	@$(call yellow ,CXX          ?| ,$(CXX) $(B_YELLOW)($(WHITE)$(CXX_VERSION)$(YELLOW)))
	@$(SHOW_CC_INFO) || \
	 $(call yellow ,CC           ?| ,$(CC) $(B_YELLOW)($(WHITE)$(CC_VERSION)$(YELLOW)))
	@$(call blue   ,THREADS      :| ,$(THREADS))
	@$(call green  ,REQFLAGS     $(RED)!| ,$(REQFLAGS))
	@$(call red    ,WARNFLAGS    $(BLUE):| ,$(WARNFLAGS))
	@$(call blue   ,OPTFLAGS     :| ,$(OPTFLAGS))
	@$(call yellow ,LDCXXFLAGS   $(BLUE):| ,$(LDCXXFLAGS))
	@$(call magenta,CXXFLAGS     $(GREEN)+| ,\$$($(GREEN)REQFLAGS$(RST)) \$$($(YELLOW)LDCXXFLAGS$(RST)) \$$($(BLUE)OPTFLAGS$(RST)) \$$($(RED)WARNFLAGS$(RST)) $(OLDCXX))
	@$(call magenta,LDFLAGS      $(GREEN)+| ,\$$($(YELLOW)LDCXXFLAGS$(RST)) \$$($(BLUE)OPTFLAGS$(RST)) \$$($(RED)WARNFLAGS$(RST)) $(OLDLD))
else
info:
	 @true
endif

info-quiet: | info rocm_smi
	@$(call green,\nBuilding btop++ $(RED)($(WHITE)v$(BTOP_VERSION)$(RED)) $(YELLOW)$(PLATFORM) $(CYAN)$(ARCH))

help:
	@printf " $(BANNER)\n"
	@$(call white,btop++ makefile)
	@printf "usage: make [argument]\n\n"
	@printf "arguments:\n"
	@printf "  all          Compile btop (default argument)\n"
	@printf "  clean        Remove built objects\n"
	@printf "  distclean    Remove built objects and binaries\n"
	@printf "  install      Install btop++ to \$$PREFIX ($(PREFIX))\n"
	@printf "  setcap       Set extended capabilities on binary (preferable to setuid)\n"
	@printf "  setuid       Set installed binary owner/group to \$$SU_USER/\$$SU_GROUP ($(SU_USER)/$(SU_GROUP)) and set SUID bit\n"
	@printf "  uninstall    Uninstall btop++ from \$$PREFIX\n"
	@printf "  info         Display information about Environment,compiler and linker flags\n"

#? Make the Directories
directories:
	@$(VERBOSE) || printf "mkdir -p $(TARGETDIR)\n"
	@mkdir -p $(TARGETDIR)
	@$(VERBOSE) || printf "mkdir -p $(BUILDDIR)/$(PLATFORM_DIR)\n"
	@mkdir -p $(BUILDDIR)/$(PLATFORM_DIR)

config.h: $(BUILDDIR)/config.h

$(BUILDDIR)/config.h: $(SRCDIR)/config.h.in | directories
	@$(QUIET) || printf "$(BLD)Configuring $(BUILDDIR)/config.h$(RST)\n"
	@$(VERBOSE) || printf 'sed -e "s|@GIT_COMMIT@|$(GIT_COMMIT)|" -e "s|@CONFIGURE_COMMAND@|$(CONFIGURE_COMMAND)|" -e "s|@COMPILER@|$(CXX)|" -e "s|@COMPILER_VERSION@|$(CXX_VERSION)|" $< | tee $@ > /dev/null\n'
	@sed -e "s|@GIT_COMMIT@|$(GIT_COMMIT)|" -e "s|@CONFIGURE_COMMAND@|$(CONFIGURE_COMMAND)|" -e "s|@COMPILER@|$(CXX)|" -e "s|@COMPILER_VERSION@|$(CXX_VERSION)|" $< | tee $@ > /dev/null

#? Man page
btop.1: manpage.md | directories
ifeq ($(shell command -v lowdown >/dev/null; echo $$?),0)
	@$(call green,\nGenerating man page $@,...)
	lowdown -s -Tman -o $@ $<
else
	@$(call yellow,\nCommand 'lowdown' not found: skipping generating man page $@)
endif

#? Clean only Objects
clean:
	@$(call red,Removing: $(WHITE)built objects,...)
	@rm -rf $(BUILDDIR)
	@test -e lib/rocm_smi_lib/build && cmake --build lib/rocm_smi_lib/build --target clean &> /dev/null || true

#? Clean Objects and Binaries
distclean: clean
	@$(call red,Removing: $(WHITE)built binaries,...)
	@rm -rf $(TARGETDIR)
	@test -e lib/rocm_smi_lib/build && rm -rf lib/rocm_smi_lib/build || true

install:
	@$(call green,Installing binary to: $(WHITE)$(DESTDIR)$(PREFIX)/bin/btop)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p $(TARGETDIR)/btop $(DESTDIR)$(PREFIX)/bin/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop
	@$(call green,Installing doc to: $(WHITE)$(DESTDIR)$(PREFIX)/share/doc/btop)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/btop
	@cp -p README.md $(DESTDIR)$(PREFIX)/share/doc/btop
	@mkdir -p $(DESTDIR)$(PREFIX)/share/btop
	@$(call green,Installing themes to: $(WHITE)$(DESTDIR)$(PREFIX)/share/btop/themes)
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop
	@$(call green,Installing desktop entry to: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/applications/btop.desktop)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/applications/
	@cp -p btop.desktop $(DESTDIR)$(PREFIX)/share/applications/btop.desktop
	@$(call green,Installing PNG icon to: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	@cp -p Img/icon.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png
	@$(call green,Installing SVG icon to: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	@cp -p Img/icon.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg
ifneq ($(wildcard btop.1),)
	@$(call green,Installing man page to: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/man/man1/btop.1)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	@cp -p btop.1 $(DESTDIR)$(PREFIX)/share/man/man1/btop.1
endif

#? Set SUID bit for btop as $SU_USER in $SU_GROUP
setuid:
	@$(call white,File: $(DESTDIR)$(PREFIX)/bin/btop)
	@$(call green,Setting owner ,$(WHITE)$(SU_USER):$(SU_GROUP))
	@chown $(SU_USER):$(SU_GROUP) $(DESTDIR)$(PREFIX)/bin/btop
	@$(call green,Setting SUID bit)
	@chmod u+s $(DESTDIR)$(PREFIX)/bin/btop

#? Run setcap on btop for extended capabilities
setcap:
	@$(call white,File: $(DESTDIR)$(PREFIX)/bin/btop)
	@$(call green,Setting capabilities,...)
	@setcap "cap_perfmon=+ep cap_dac_read_search=+ep" $(DESTDIR)$(PREFIX)/bin/btop

# With 'rm -v' user will see what files (if any) got removed
uninstall:
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/bin/btop)
	@rm -rfv $(DESTDIR)$(PREFIX)/bin/btop
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/btop)
	@rm -rfv $(DESTDIR)$(PREFIX)/share/btop
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/applications/btop.desktop)
	@rm -rfv $(DESTDIR)$(PREFIX)/share/applications/btop.desktop
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png)
	@rm -rfv $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg)
	@rm -rfv $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg
	@$(call red,Removing: ,$(WHITE)$(DESTDIR)$(PREFIX)/share/man/man1/btop.1)
	@rm -rfv $(DESTDIR)$(PREFIX)/share/man/man1/btop.1

#? Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#? Compile rocm_smi
ifeq ($(GPU_SUPPORT)$(RSMI_STATIC),truetrue)
	ROCM_DIR ?= lib/rocm_smi_lib
	ROCM_BUILD_DIR := $(ROCM_DIR)/build
	ifeq ($(DEBUG),true)
		BUILD_TYPE := Debug
	else
		BUILD_TYPE := Release
	endif
.ONESHELL:
rocm_smi:
	@$(call green,\nBuilding ROCm SMI static library,...)
	@$(QUIET) || $(call white,Running CMake,...)
	CXX=$(CXX) cmake -S $(ROCM_DIR) -B $(ROCM_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_POLICY_DEFAULT_CMP0069=NEW \
		-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
		-DBUILD_SHARED_LIBS=OFF $(SUPPRESS) || \
		{ $(call red,CMake failed$(COMMA) continuing build without statically linking ROCm SMI,...); exit 0; }
	@$(QUIET) || $(call white,\nBuilding and linking,...)
	@cmake --build $(ROCM_BUILD_DIR) -j -t rocm_smi64 $(SUPPRESS) || \
		{ $(call red,Make failed$(COMMA) continuing build without statically linking ROCm SMI,...); exit 0; }
	@$(call green,100% -> $(call file_with_size,$(ROCM_BUILD_DIR)/rocm_smi/librocm_smi64.a))
	@$(call green,ROCm SMI build complete in ($(WHITE)$(call step_duration,$(TIMESTAMP))$(GREEN)))
	@$(eval override LDFLAGS += $(ROCM_BUILD_DIR)/rocm_smi/librocm_smi64.a -DRSMI_STATIC) # TODO: this seems to execute every time, no matter if the compilation failed or succeeded
	@$(eval override CXXFLAGS += -DRSMI_STATIC)
else
rocm_smi:
	@true
endif

#? Link
.ONESHELL:
btop: $(OBJECTS) | rocm_smi directories
	@sleep 0.2 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || $(call green,\nLinking and optimizing binary,...)
	@$(VERBOSE) || printf "$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS)\n"
	@$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS) || exit 1
	@$(call green,100% -> $(call file_with_size,$(TARGETDIR)/btop,$(call CUR_LEFT,100)$(call CUR_RIGHT,38)) $(GREEN)($(WHITE)$(call step_duration,$$TSTAMP)$(GREEN)))
	@$(call green,\nBuild complete in $(GREEN)($(WHITE)$(call step_duration,$(TIMESTAMP))$(GREEN)))

#? Compile
.ONESHELL:
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) | rocm_smi directories config.h
	@sleep 0.3 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || $(call white,Compiling $<)
	@$(VERBOSE) || printf "$(CXX) $(CXXFLAGS) $(INC) -MMD -c -o $@ $<\n"
	@$(CXX) $(CXXFLAGS) $(INC) -MMD -c -o $@ $< || exit 1
	@$(call green,$$($(PROGRESS))%$(call CUR_LEFT,10)$(call CUR_RIGHT,5)-> $(call file_with_size,$@,$(call CUR_LEFT,100)$(call CUR_RIGHT,38)) $(GREEN)($(WHITE)$(call step_duration,$$TSTAMP)$(GREEN)))

#? Compile intel_gpu_top C sources for Intel GPU support
.ONESHELL:
$(BUILDDIR)/%.c.o: $(SRCDIR)/$(PLATFORM_DIR)/intel_gpu_top/%.c | directories
	@sleep 0.3 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || $(call white,Compiling $<)
	@$(VERBOSE) || printf "$(CC) $(INC) -c -o $@ $<\n"
	@$(CC) $(INC) -w -c -o $@ $< || exit 1
	@$(call green,$$($(PROGRESS))%$(call CUR_LEFT,10)$(call CUR_RIGHT,5)-> $(call file_with_size,$@,$(call CUR_LEFT,100)$(call CUR_RIGHT,38)) $(GREEN)($(WHITE)$(call step_duration,$$TSTAMP)$(GREEN)))


#? Non-File Targets
.PHONY: all config.h msg help pre
