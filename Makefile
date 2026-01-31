#* Btop++ makefile v1.6

BANNER   = \n [38;5;196m██████[38;5;240m╗ [38;5;196m████████[38;5;240m╗ [38;5;196m██████[38;5;240m╗ [38;5;196m██████[38;5;240m╗
BANNER  += \n [38;5;160m██[38;5;239m╔══[38;5;160m██[38;5;239m╗╚══[38;5;160m██[38;5;239m╔══╝[38;5;160m██[38;5;239m╔═══[38;5;160m██[38;5;239m╗[38;5;160m██[38;5;239m╔══[38;5;160m██[38;5;239m╗   [38;5;160m██[38;5;239m╗    [38;5;160m██[38;5;239m╗
BANNER  += \n [38;5;124m██████[38;5;238m╔╝   [38;5;124m██[38;5;238m║   [38;5;124m██[38;5;238m║   [38;5;124m██[38;5;238m║[38;5;124m██████[38;5;238m╔╝ [38;5;124m██████[38;5;238m╗[38;5;124m██████[38;5;238m╗
BANNER  += \n [38;5;88m██[38;5;237m╔══[38;5;88m██[38;5;237m╗   [38;5;88m██[38;5;237m║   [38;5;88m██[38;5;237m║   [38;5;88m██[38;5;237m║[38;5;88m██[38;5;237m╔═══╝  ╚═[38;5;88m██[38;5;237m╔═╝╚═[38;5;88m██[38;5;237m╔═╝
BANNER  += \n [38;5;52m██████[38;5;236m╔╝   [38;5;52m██[38;5;236m║   ╚[38;5;52m██████[38;5;236m╔╝[38;5;52m██[38;5;236m║        ╚═╝    ╚═╝
BANNER  += \n [38;5;235m╚═════╝    ╚═╝    ╚═════╝ ╚═╝      [1;3;38;5;240mMakefile v1.6[0m

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
$(error $(shell printf "[1;91mERROR: [97m$(CXX) can't statically link glibc[0m"))
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
$(error $(shell printf "[1;91mERROR: [97mUnsupported platform ($(PLATFORM))[0m"))
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

P := %%

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
	@printf "[1;92mPLATFORM     [1;93m?| [0m$(PLATFORM)\n"
	@printf "[1;96mARCH         [1;93m?| [0m$(ARCH)\n"
	@printf "[1;95mGPU_SUPPORT  [1;94m:| [0m$(GPU_SUPPORT)\n"
	@printf "[1;93mCXX          [1;93m?| [0m$(CXX) [1;93m([97m$(CXX_VERSION)[93m)\n"
	@$(SHOW_CC_INFO) || printf "[1;93mCC           [1;93m?| [0m$(CC) [1;93m([97m$(CC_VERSION)[93m)\n"
	@printf "[1;94mTHREADS      [1;94m:| [0m$(THREADS)\n"
	@printf "[1;92mREQFLAGS     [1;91m!| [0m$(REQFLAGS)\n"
	@printf "[1;91mWARNFLAGS    [1;94m:| [0m$(WARNFLAGS)\n"
	@printf "[1;94mOPTFLAGS     [1;94m:| [0m$(OPTFLAGS)\n"
	@printf "[1;93mLDCXXFLAGS   [1;94m:| [0m$(LDCXXFLAGS)\n"
	@printf "[1;95mCXXFLAGS     [1;92m+| [0;37m\$$([92mREQFLAGS[37m) \$$([93mLDCXXFLAGS[37m) \$$([94mOPTFLAGS[37m) \$$([91mWARNFLAGS[37m) $(OLDCXX)\n"
	@printf "[1;95mLDFLAGS      [1;92m+| [0;37m\$$([93mLDCXXFLAGS[37m) \$$([94mOPTFLAGS[37m) \$$([91mWARNFLAGS[37m) $(OLDLD)\n"
else
info:
	 @true
endif

info-quiet: | info rocm_smi
	@printf "\n[1;92mBuilding btop++ [91m([97mv$(BTOP_VERSION)[91m) [93m$(PLATFORM) [96m$(ARCH)[0m\n"

help:
	@printf " $(BANNER)\n"
	@printf "[1;97mbtop++ makefile[0m\n"
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
	@$(QUIET) || printf "[1mConfiguring $(BUILDDIR)/config.h[0m\n"
	@$(VERBOSE) || printf 'sed -e "s|@GIT_COMMIT@|$(GIT_COMMIT)|" -e "s|@CONFIGURE_COMMAND@|$(CONFIGURE_COMMAND)|" -e "s|@COMPILER@|$(CXX)|" -e "s|@COMPILER_VERSION@|$(CXX_VERSION)|" $< | tee $@ > /dev/null\n'
	@sed -e "s|@GIT_COMMIT@|$(GIT_COMMIT)|" -e "s|@CONFIGURE_COMMAND@|$(CONFIGURE_COMMAND)|" -e "s|@COMPILER@|$(CXX)|" -e "s|@COMPILER_VERSION@|$(CXX_VERSION)|" $< | tee $@ > /dev/null

#? Man page
btop.1: manpage.md | directories
ifeq ($(shell command -v lowdown >/dev/null; echo $$?),0)
	@printf "\n[1;92mGenerating man page $@[37m...[0m\n"
	lowdown -s -Tman -o $@ $<
else
	@printf "\n[1;93mCommand 'lowdown' not found: skipping generating man page $@[0m\n"
endif

#? Clean only Objects
clean:
	@printf "[1;91mRemoving: [1;97mbuilt objects...[0m\n"
	@rm -rf $(BUILDDIR)
	@test -e lib/rocm_smi_lib/build && cmake --build lib/rocm_smi_lib/build --target clean &> /dev/null || true

#? Clean Objects and Binaries
distclean: clean
	@printf "[1;91mRemoving: [1;97mbuilt binaries...[0m\n"
	@rm -rf $(TARGETDIR)
	@test -e lib/rocm_smi_lib/build && rm -rf lib/rocm_smi_lib/build || true

install:
	@printf "[1;92mInstalling binary to: [1;97m$(DESTDIR)$(PREFIX)/bin/btop[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -p $(TARGETDIR)/btop $(DESTDIR)$(PREFIX)/bin/btop
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/btop
	@printf "[1;92mInstalling doc to: [1;97m$(DESTDIR)$(PREFIX)/share/doc/btop[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/btop
	@cp -p README.md $(DESTDIR)$(PREFIX)/share/doc/btop
	@mkdir -p $(DESTDIR)$(PREFIX)/share/btop
	@printf "[1;92mInstalling themes to: [1;97m$(DESTDIR)$(PREFIX)/share/btop/themes[0m\n"
	@cp -pr themes $(DESTDIR)$(PREFIX)/share/btop
	@printf "[1;92mInstalling desktop entry to: [1;97m$(DESTDIR)$(PREFIX)/share/applications/btop.desktop[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/applications/
	@cp -p btop.desktop $(DESTDIR)$(PREFIX)/share/applications/btop.desktop
	@printf "[1;92mInstalling PNG icon to: [1;97m$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	@cp -p Img/icon.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png
	@printf "[1;92mInstalling SVG icon to: [1;97m$(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	@cp -p Img/icon.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg
ifneq ($(wildcard btop.1),)
	@printf "[1;92mInstalling man page to: [1;97m$(DESTDIR)$(PREFIX)/share/man/man1/btop.1[0m\n"
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	@cp -p btop.1 $(DESTDIR)$(PREFIX)/share/man/man1/btop.1
endif

#? Set SUID bit for btop as $SU_USER in $SU_GROUP
setuid:
	@printf "[1;97mFile: $(DESTDIR)$(PREFIX)/bin/btop\n"
	@printf "[1;92mSetting owner [1;97m$(SU_USER):$(SU_GROUP)[0m\n"
	@chown $(SU_USER):$(SU_GROUP) $(DESTDIR)$(PREFIX)/bin/btop
	@printf "[1;92mSetting SUID bit[0m\n"
	@chmod u+s $(DESTDIR)$(PREFIX)/bin/btop

#? Run setcap on btop for extended capabilities
setcap:
	@printf "[1;97mFile: $(DESTDIR)$(PREFIX)/bin/btop\n"
	@printf "[1;92mSetting capabilities...[0m\n"
	@setcap "cap_perfmon=+ep cap_dac_read_search=+ep" $(DESTDIR)$(PREFIX)/bin/btop

# With 'rm -v' user will see what files (if any) got removed
uninstall:
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/bin/btop[0m\n"
	@rm -rfv $(DESTDIR)$(PREFIX)/bin/btop
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/share/btop[0m\n"
	@rm -rfv $(DESTDIR)$(PREFIX)/share/btop
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/share/applications/btop.desktop[0m\n"
	@rm -rfv $(DESTDIR)$(PREFIX)/share/applications/btop.desktop
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png[0m\n"
	@rm -rfv $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/btop.png
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg[0m\n"
	@rm -rfv $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/btop.svg
	@printf "[1;91mRemoving: [1;97m$(DESTDIR)$(PREFIX)/share/man/man1/btop.1[0m\n"
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
	@printf "\n[1;92mBuilding ROCm SMI static library[37m...[0m\n"
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || printf "[1;97mRunning CMake...[0m\n"
	CXX=$(CXX) cmake -S $(ROCM_DIR) -B $(ROCM_BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_POLICY_DEFAULT_CMP0069=NEW -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DBUILD_SHARED_LIBS=OFF $(SUPPRESS) || { printf "[1;91mCMake failed, continuing build without statically linking ROCm SMI[37m...[0m\n"; exit 0; }
	@$(QUIET) || printf "\n[1;97mBuilding and linking...[0m\n"
	@cmake --build $(ROCM_BUILD_DIR) -j -t rocm_smi64 $(SUPPRESS) || { printf "[1;91mMake failed, continuing build without statically linking ROCm SMI[37m...[0m\n"; exit 0; }
	@printf "[1;92m100$(P)[10D[5C-> [1;37m$(ROCM_BUILD_DIR)/rocm_smi/librocm_smi64.a [1;93m([1;97m$$(du -ah $(ROCM_BUILD_DIR)/rocm_smi/librocm_smi64.a | cut -f1)iB[1;93m)[0m\n"
	@printf "[1;92mROCm SMI build complete in [92m([97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $(TIMESTAMP) 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo "unknown")[92m)[0m\n"
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
	@$(QUIET) || printf "\n[1;92mLinking and optimizing binary[37m...[0m\n"
	@$(VERBOSE) || printf "$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS)\n"
	@$(CXX) -o $(TARGETDIR)/btop $^ $(LDFLAGS) || exit 1
	@printf "[1;92m100$(P) -> [1;37m$(TARGETDIR)/btop [100D[38C[1;93m([1;97m$$(du -ah $(TARGETDIR)/btop | cut -f1)iB[1;93m) [92m([97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $${TSTAMP} 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo '')[92m)[0m\n"
	@printf "\n[1;92mBuild complete in [92m([97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - $(TIMESTAMP) 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo "unknown")[92m)[0m\n"

#? Compile
.ONESHELL:
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) | rocm_smi directories config.h
	@sleep 0.3 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || printf "[1;97mCompiling $<[0m\n"
	@$(VERBOSE) || printf "$(CXX) $(CXXFLAGS) $(INC) -MMD -c -o $@ $<\n"
	@$(CXX) $(CXXFLAGS) $(INC) -MMD -c -o $@ $< || exit 1
	@printf "[1;92m$$($(PROGRESS))$(P)[10D[5C-> [1;37m$@ [100D[38C[1;93m([1;97m$$(du -ah $@ | cut -f1)iB[1;93m) [92m([97m$$($(DATE_CMD) -d @$$(expr $$($(DATE_CMD) +%s 2>/dev/null || echo "0") - $${TSTAMP} 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo '')[92m)[0m\n"

#? Compile intel_gpu_top C sources for Intel GPU support
.ONESHELL:
$(BUILDDIR)/%.c.o: $(SRCDIR)/$(PLATFORM_DIR)/intel_gpu_top/%.c | directories
	@sleep 0.3 2>/dev/null || true
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@$(QUIET) || printf "[1;97mCompiling $<[0m\n"
	@$(VERBOSE) || printf "$(CC) $(INC) -c -o $@ $<\n"
	@$(CC) $(INC) -w -c -o $@ $< || exit 1
	@printf "[1;92m$$($(PROGRESS))$(P)[10D[5C-> [1;37m$@ [100D[38C[1;93m([1;97m$$(du -ah $@ | cut -f1)iB[1;93m) [92m([97m$$($(DATE_CMD) -d @$$(expr $$($(DATE_CMD) +%s 2>/dev/null || echo "0") - $${TSTAMP} 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo '')[92m)[0m\n"


#? Non-File Targets
.PHONY: all config.h msg help pre
