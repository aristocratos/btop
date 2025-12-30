# ![btop++](Img/logo.png)

<a href="https://repology.org/project/btop/versions">
    <img src="https://repology.org/badge/vertical-allrepos/btop.svg" alt="Packaging status" align="right">
</a>

![Linux](https://img.shields.io/badge/-Linux-grey?logo=linux)
![macOS](https://img.shields.io/badge/-OSX-black?logo=apple)
![FreeBSD](https://img.shields.io/badge/-FreeBSD-red?logo=freebsd)
![NetBSD](https://img.shields.io/badge/-NetBSD-black?logo=netbsd)
![OpenBSD](https://img.shields.io/badge/-OpenBSD-black?logo=openbsd)
![Usage](https://img.shields.io/badge/Usage-System%20resource%20monitor-yellow)
![c++23](https://img.shields.io/badge/cpp-c%2B%2B23-green)
![latest_release](https://img.shields.io/github/v/tag/aristocratos/btop?label=release)
[![Donate](https://img.shields.io/badge/-Donate-yellow?logo=paypal)](https://paypal.me/aristocratos)
[![Sponsor](https://img.shields.io/badge/-Sponsor-red?logo=github)](https://github.com/sponsors/aristocratos)
[![Coffee](https://img.shields.io/badge/-Buy%20me%20a%20Coffee-grey?logo=Ko-fi)](https://ko-fi.com/aristocratos)
[![btop](https://snapcraft.io/btop/badge.svg)](https://snapcraft.io/btop)
[![Continuous Build Linux](https://github.com/aristocratos/btop/actions/workflows/continuous-build-linux.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-linux.yml)
[![Continuous Build macOS](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml)
[![Continuous Build FreeBSD](https://github.com/aristocratos/btop/actions/workflows/continuous-build-freebsd.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-freebsd.yml)
[![Continuous Build NetBSD](https://github.com/aristocratos/btop/actions/workflows/continuous-build-netbsd.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-netbsd.yml)
[![Continuous Build OpenBSD](https://github.com/aristocratos/btop/actions/workflows/continuous-build-openbsd.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-openbsd.yml)

## Index

* [News](#news)
* [Documents](#documents)
* [Description](#description)
* [Features](#features)
* [Themes](#themes)
* [Support and funding](#support-and-funding)
* [Prerequisites](#prerequisites) (Read this if you are having issues!)
* [Screenshots](#screenshots)
* [Keybindings](#help-menu)
* [Installation Linux/macOS](#installation)
* [Compilation Linux](#compilation-linux)
* [Compilation macOS](#compilation-macos-osx)
* [Compilation FreeBSD](#compilation-freebsd)
* [Compilation NetBSD](#compilation-netbsd)
* [Compilation OpenBSD](#compilation-openbsd)
* [Testing](#testing)
* [GPU compatibility](#gpu-compatibility)
* [Installing the snap](#installing-the-snap)
* [Configurability](#configurability)
* [License](#license)

If you are considering donating, please first consider donating to:

[<img src="https://images.squarespace-cdn.com/content/v1/66fd17c779966209da4359da/9dcb67db-433e-41cb-94d7-cecba280dc0b/Picture+1.png" height="100px">](https://www.qm4ua.org/)
[<img src="https://secure2.convio.net/stccad/images/content/pagebuilder/stc-logo2022.png" width="400px">](https://donate.savethechildren.org)



## News

##### 4 December 2025

Since there is a increasing amount of AI generated/assisted PR's, the following guidlines have been added to CONTRIBUTING.md:

* Submissions where the majority of the code is AI generated must be marked with [AI generated].

* "Vibe coded" PR's where it seems like the author doesn't understand the generated code will be dismissed.

##### 22 September 2024

Btop release v1.4.0

Intel GPU support added, note that only GPU utilization, power usage and clock speed available to monitor. Thanks to [@bjia56](https://github.com/bjia56) for contributions.

NetBSD support added. Thanks to [@fraggerfox](https://github.com/fraggerfox) for contributions.

See [CHANGELOG.md](CHANGELOG.md) and latest [release](https://github.com/aristocratos/btop/releases/latest) for detailed list of new features, bug fixes and new themes.

##### 7 January 2024

Btop release v1.3.0

Big release with GPU support added for Linux and platform support for OpenBSD. Big thanks to [@romner-set](https://github.com/romner-set) (GPU support) and [@joske](https://github.com/joske) (OpenBSD support) for contributions.
And a multitude of bugfixes and small changes, see [CHANGELOG.md](CHANGELOG.md) and latest [release](https://github.com/aristocratos/btop/releases/latest) for detailed list and attributions.

See news entry below for more information regarding GPU support.

##### 25 November 2023

GPU monitoring added for Linux!

Compile from git main to try it out.

Use keys `5`, `6`, `7` and `0` to show/hide the gpu monitoring boxes. `5` = Gpu 1, `6` = Gpu 2, etc.

Gpu stats/graphs can also be displayed in the "Cpu box" (not as verbose), see the cpu options menu for info and configuration.

Note that the binaries provided on the release page (when released) and the continuous builds will not have gpu support enabled.

Because the GPU support relies on loading of dynamic gpu libraries, gpu support will not work when also static linking.

See [Compilation Linux](#compilation-linux) for more info on how to compile with gpu monitoring support.

Many thanks to [@romner-set](https://github.com/romner-set) who wrote the vast majority of the implementation for GPU support.

Big update with version bump to 1.3 coming soon.

##### 28 August 2022

[![btop4win](https://github.com/aristocratos/btop4win/raw/master/Img/logo.png)](https://github.com/aristocratos/btop4win)

First release of btop4win available at https://github.com/aristocratos/btop4win

<details>
<summary>More...</summary>

##### 16 January 2022

Release v1.2.0 with FreeBSD support. No release binaries for FreeBSD provided as of yet.

Again a big thanks to [@joske](https://github.com/joske) for his porting efforts!

Since compatibility with Linux, macOS and FreeBSD are done, the focus going forward will be on new features like GPU monitoring.

##### 13 November 2021

Release v1.1.0 with macOS support. Binaries in [continuous-build-macos](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml) are only x86 for now.
macOS binaries + installer are included for both x86 and ARM64 (Apple Silicon) in the releases.

Big thank you to [@joske](https://github.com/joske) who wrote the vast majority of the implementation!

##### 30 October 2021

Work on the OSX [macOS] and FreeBSD branches, both initiated and mostly worked on by [@joske](https://github.com/joske), will likely be completed in the coming weeks.
The OSX [macOS] branch has some memory leaks that needs to be sorted out and both have some issues with the processes cpu usage calculation and other smaller issues that needs fixing.

If you want to help out, test for bugs/fix bugs or just try out the branches:

**macOS / OSX**
```bash
# Install and use Homebrew or MacPorts package managers for easy dependency installation
brew install coreutils make gcc@11 lowdown
git clone https://github.com/aristocratos/btop.git
cd btop
git checkout OSX
gmake
```

**FreeBSD**
```bash
sudo pkg install gmake gcc11 coreutils git lowdown
git clone https://github.com/aristocratos/btop.git
cd btop
git checkout freebsd
gmake
```

Note that GNU make (`gmake`) is recommended but not required for macOS/OSX but it is required on FreeBSD.


##### 6 October 2021

macOS development have been started by [@joske](https://github.com/joske), big thanks :)
See branch [OSX](https://github.com/aristocratos/btop/tree/OSX) for current progress.

##### 18 September 2021

The Linux version of btop++ is complete. Released as version 1.0.0

I will be providing statically compiled binaries for a range of architectures in every release for those having problems compiling.

For compilation GCC 11 is required.

Please report any bugs to the [Issues](https://github.com/aristocratos/btop/issues/new?assignees=aristocratos&labels=bug&template=bug_report.md&title=%5BBUG%5D) page.

The development plan right now:

* 1.1.0 macOS [OSX] support
* 1.2.0 FreeBSD support
* 1.3.0 Support for GPU monitoring
* 1.X.0 Other platforms and features...

Windows support is not in the plans as of now, but if anyone else wants to take it on, I will try to help.

##### 5 May 2021

This project is gonna take some time until it has complete feature parity with bpytop, since all system information gathering will have to be written from scratch without any external libraries.
And will need some help in the form of code contributions to get complete support for BSD and macOS/OSX.

</details>

## Documents

**[CHANGELOG.md](CHANGELOG.md)**

**[CONTRIBUTING.md](CONTRIBUTING.md)**

**[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)**

## Description

Resource monitor that shows usage and stats for processor, memory, disks, network and processes.

C++ version and continuation of [bashtop](https://github.com/aristocratos/bashtop) and [bpytop](https://github.com/aristocratos/bpytop).

## Features

* Easy to use, with a game inspired menu system.
* Full mouse support: all buttons with a highlighted key are clickable and mouse scrolling works in process list and menu boxes.
* Fast and responsive UI with UP, DOWN key process selection.
* Function for showing detailed stats for selected process.
* Ability to filter processes.
* Easy switching between sorting options.
* Tree view of processes.
* Send any signal to selected process.
* Pause the process list.
* UI menu for changing all config file options.
* Auto scaling graph for network usage.
* Shows IO activity and speeds for disks.
* Battery meter
* Selectable symbols for the graphs.
* Custom presets
* And more...

## Themes

Btop++ uses the same theme files as bpytop and bashtop (some color values missing in bashtop themes).

See [themes](https://github.com/aristocratos/btop/tree/main/themes) folder for available themes.

Btop searches the following directories for system themes:

* `../share/btop/themes` (this path is relative to the btop executable)
* `/usr/local/share/btop/themes`
* `/usr/share/btop/themes`

The first directory that exists and isn't empty is used as the system themes directory.

The user themes directory depends on which environment variables are set:

* If `$XDG_CONFIG_HOME` is set, the user themes directory is `$XDG_CONFIG_HOME/btop/themes`
* Otherwise, if `$HOME` is set, the user themes directory is `$HOME/.config/btop/themes`
* Otherwise, the user themes directory is `~/.config/btop/themes`

The `make install` command places the default themes in `[$PREFIX or /usr/local]/share/btop/themes`.
User created themes should be placed in the user themes directory.

Use the `--themes-dir` command-line option to specify a custom themes directory.
When specified, this directory takes priority over the default search paths.

Let me know if you want to contribute with new themes.

The new Process list pausing and Process following features introduce a few new theme attributes.
These attributes still need to be added to all of the existing themes (except the default one).

Process list banner attributes:
* proc_pause_bg: background color of the banner when the list is paused.
* proc_follow_bg: background color of the banner when the process following feature is active.
* proc_banner_bg: background color of the banner when the process following feature is active AND the list is paused.
* proc_banner_fg: foreground (text) color of the banner

Process following attributes:
* followed_bg: background color of the followed process in the list.
* followed_fg: foreground color of the followed process in the list.

## Support and funding

You can sponsor this project through GitHub. See [my sponsors page](https://github.com/sponsors/aristocratos) for options.

Or donate through [PayPal](https://paypal.me/aristocratos) or [ko-fi](https://ko-fi.com/aristocratos).

Any support is greatly appreciated!

## Prerequisites

For the best experience run within a terminal with support for:

* 24-bit truecolor ([See list of terminals with truecolor support](https://github.com/termstandard/colors))
* 256-color terminals are supported through 24-bit to 256-color conversion when setting "truecolor" to False in the options or with "-lc/--low-color" arguments.
* 16 color TTY mode will be activated if a real tty device is detected. Can be forced with "-t/--tty" arguments.
* Wide characters (Are sometimes problematic in web-based terminals)

Also necessary is a UTF8 locale and a font that includes:

* Unicode Block “Braille Patterns” U+2800 - U+28FF (Not needed in TTY mode or with graphs set to type: block or tty.)
* Unicode Block “Geometric Shapes” U+25A0 - U+25FF
* Unicode Block "Box Drawing" and "Block Elements" U+2500 - U+259F

### **Optional Dependencies (Needed for GPU monitoring) (Only Linux)**

GPU monitoring also requires a btop binary built with GPU support (`GPU_SUPPORT=true` flag).

See [GPU compatibility](#gpu-compatibility) section for more about compiling with GPU support.

 * **NVIDIA**

   If you have an NVIDIA GPU you must use an official NVIDIA driver, both the closed-source and open-source ones have been verified to work.

   In addition to that you must also have the nvidia-ml dynamic library installed, which should be included with the driver package of your distribution.

 * **AMD**

   If you have an AMD GPU `rocm_smi_lib` is required, which may or may not be packaged for your distribution.

 * **INTEL**

   Requires a working C compiler if compiling from source.

   Also requires the user to have permission to read from SYSFS.

   Can be set with `make setcap` (preferred) or `make setuid` or by running btop with `sudo` or equivalent.

### **Notice (Text rendering issues)**

* If you are having problems with the characters in the graphs not looking like they do in the screenshots, it's likely a problem with your systems configured fallback font not having support for braille characters.

* See [Terminess Powerline](https://github.com/ryanoasis/nerd-fonts/blob/master/patched-fonts/Terminus/TerminessNerdFontMono-Regular.ttf) for an example of a font that includes the braille symbols.

* See comments by @sgleizes [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-684036827) and @XenHat [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-691585587) in issue #100 for possible solutions.

* If text is misaligned and you use Konsole or Yakuake, turning off "Bi-Directional text rendering" is a possible fix.

* Characters clipping into each other or text/border misalignments are not bugs caused by btop, but most likely a fontconfig or terminal problem where the braille characters making up the graphs aren't rendered correctly.

* Look to the creators of the terminal emulator you use to fix these issues if the previously mentioned fixes don't work for you.

## Screenshots

#### Main UI showing details for a selected process

![Screenshot 1](Img/normal.png)

#### Main UI in TTY mode

![Screenshot 2](Img/tty.png)

#### Main UI with custom options

![Screenshot 3](Img/alt.png)

#### Main-menu

![Screenshot 3](Img/main-menu.png)

#### Options-menu

![Screenshot 4](Img/options-menu.png)

#### Help-menu

![Screenshot 5](Img/help-menu.png)

## Installation

**Binaries for Linux are statically compiled with musl and work on kernel releases 2.6.39 and newer**

1. **Download btop-(VERSION)-(ARCH)-(PLATFORM).tbz from [latest release](https://github.com/aristocratos/btop/releases/latest) and unpack to a new folder**

   **Notice! Use x86_64 for 64-bit x86 systems, i486 and i686 are 32-bit!**

2. **Install (from created folder)**

   * **Run:**

   ```bash
   # use "make install PREFIX=/target/dir" to set target, default: /usr/local
   # only use "sudo" when installing to a NON user owned directory
   sudo make install
   ```

3. **(Optional/Required for Intel GPU and CPU wattage) Set extended capabilities or suid bit to btop**

   Enables signal sending to any process without starting with `sudo` and can prevent /proc read permissions problems on some systems.

   Is required for Intel GPU support and CPU wattage monitoring.

   * **Run:**

   ```bash
   # run after make install and use same PREFIX if any was used at install
   sudo make setcap
   ```
   * **or**

   ```bash
   # run after make install and use same PREFIX if any was used at install
   # set SU_USER and SU_GROUP to select user and group, default is root:root
   sudo make setuid
   ```

* **Uninstall**

  * **Run:**

   ```bash
   sudo make uninstall
   ```

* **Show help**

   ```bash
   make help
   ```

**Binary release (from native os repo)**

* **openSUSE**
  * **Tumbleweed:**
    ```bash
    sudo zypper in btop
    ```
  * For all other versions, see [openSUSE Software: btop](https://software.opensuse.org/package/btop)
* **Fedora**
    ```bash
    sudo dnf install btop
	```
* **RHEL/Rocky/AlmaLinux 8+**
    ```bash
    sudo dnf install epel-release
	sudo dnf install btop
	```
* **FreeBSD**
	```sh
	pkg install btop
	```
* **NetBSD**
	```sh
	pkg_add btop
	```


**Binary release on Homebrew (macOS (x86_64 & ARM64) / Linux (x86_64))**

* **[Homebrew](https://formulae.brew.sh/formula/btop)**
  ```bash
  brew install btop
  ```

## Compilation Linux

   Requires at least GCC 14 or Clang 19.

   The Makefile also needs GNU `coreutils` and `sed` (should already be installed on any modern distribution).

   ### GPU compatibility

   Btop++ supports Nvidia and AMD GPUs and Intel IGPUs out of the box on Linux x86_64, provided you have the correct drivers and libraries.

   Gpu support for Nvidia or AMD will not work when static linking glibc (or musl, etc.)!

   For x86_64 Linux the flag `GPU_SUPPORT` is automatically set to `true`, to manually disable gpu support set the flag to false, like:

   `make GPU_SUPPORT=false` (or `cmake -DBTOP_GPU=false` with CMake)

 * **NVIDIA**

    You must use an official NVIDIA driver, both the closed-source and [open-source](https://github.com/NVIDIA/open-gpu-kernel-modules) ones have been verified to work.

    In addition to that you must also have the `nvidia-ml` dynamic library installed, which should be included with the driver package of your distribution.

 * **AMD**

    AMDGPU data is queried using the [ROCm SMI](https://github.com/rocm/rocm_smi_lib) library, which may or may not be packaged for your distribution. If your distribution doesn't provide a package, btop++ is statically linked to ROCm SMI with the `RSMI_STATIC=true` make flag.

    This flag expects the ROCm SMI source code in `lib/rocm_smi_lib`, and compilation will fail if it's not there. The latest tested version is 5.6.x, which can be obtained with the following command:

   ```bash
   git clone https://github.com/rocm/rocm_smi_lib.git --depth 1 -b rocm-5.6.x lib/rocm_smi_lib
   ```

<details>
<summary>

### With Make
</summary>

1. **Install dependencies (example for Ubuntu 24.04 Noble)**

   ```bash
   sudo apt install coreutils sed git build-essential lowdown
   ```

2. **Clone repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   ```bash
   make
   ```

   Options for make:

   | Flag                            | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `VERBOSE=true`                  | To display full compiler/linker commands                                |
   | `STATIC=true`                   | For static compilation                                                  |
   | `QUIET=true`                    | For less verbose output                                                 |
   | `STRIP=true`                    | To force stripping of debug symbols (adds `-s` linker flag)             |
   | `DEBUG=true`                    | Sets OPTFLAGS to `-O0 -g` and enables more verbose debug logging        |
   | `ARCH=<architecture>`           | To manually set the target architecture                                 |
   | `GPU_SUPPORT=<true\|false>`     | Enable/disable GPU support (Enabled by default on X86_64 Linux)         |
   | `RSMI_STATIC=true`              | To statically link the ROCm SMI library used for querying AMDGPU        |
   | `ADDFLAGS=<flags>`              | For appending flags to both compiler and linker                         |
   | `CXX=<compiler>`                | Manually set which compiler to use                                       |

   Example: `make ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

   Notice! If using LDAP Authentication, usernames will show as UID number for LDAP users if compiling statically with glibc.

4. **Install**

   ```bash
   sudo make install
   ```

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

5. **(Optional/Required for Intel GPU support and CPU wattage) Set extended capabilities or suid bit to btop**

   No need for `sudo` to enable signal sending to any process and to prevent /proc read permissions problems on some systems.

   Also required for Intel GPU monitoring and CPU wattage monitoring.

   Run after make install and use same PREFIX if any was used at install.

   ```bash
   sudo make setcap
   ```

   or

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `root`

   ```bash
   sudo make setuid
   ```

* **Uninstall**

   ```bash
   sudo make uninstall
   ```

* **Remove any object files from source dir**

   ```bash
   make clean
   ```

* **Remove all object files, binaries and created directories in source dir**

   ```bash
   make distclean
   ```

* **Show help**

   ```bash
   make help
   ```

</details>
<details>
<summary>

### With CMake (Community maintained)
</summary>

1. **Install build dependencies**

   Requires Clang / GCC, CMake, Ninja, Lowdown and Git

   For example, with Debian Bookworm:

   ```bash
   sudo apt install cmake git g++ ninja-build lowdown
   ```

2. **Clone the repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git && cd btop
   ``````

3. **Compile**

   ```bash
   # Configure
   cmake -B build -G Ninja
   # Build
   cmake --build build
   ```

   This will automatically build a release version of btop.

   Some useful options to pass to the configure step:

   | Configure flag                  | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `-DBTOP_STATIC=<ON\|OFF>`       | Enables static linking (OFF by default)                                 |
   | `-DBTOP_LTO=<ON\|OFF>`          | Enables link time optimization (ON by default)                          |


   | `-DBTOP_GPU=<ON\|OFF>`          | Enable GPU support (ON by default)                                      |
   | `-DBTOP_RSMI_STATIC=<ON\|OFF>`  | Build and link the ROCm SMI library statically (OFF by default)         |
   | `-DCMAKE_INSTALL_PREFIX=<path>` | The installation prefix ('/usr/local' by default)                       |

   To force any other compiler, run `CXX=<compiler> cmake -B build -G Ninja`

4. **Install**

   ```bash
   cmake --install build
   ```

   May require root privileges

5. **Uninstall**

   CMake doesn't generate an uninstall target by default. To remove installed files, run
   ```
   cat build/install_manifest.txt | xargs rm -irv
   ```

6. **Cleanup build directory**

   ```bash
   cmake --build build -t clean
   ```

</details>

## Compilation macOS OSX

   Requires at least GCC 14 or Clang 19.

   The Makefile also needs GNU coreutils and `sed`.

   Install and use Homebrew or MacPorts package managers for easy dependency installation

<details>
<summary>

### With Make
</summary>

1. **Install dependencies (example for Homebrew)**

   ```bash
   brew install coreutils make gcc@15 lowdown
   ```

2. **Clone repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```
3. **Compile**

   ```bash
   gmake
   ```

   Options for make:

   | Flag                            | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `VERBOSE=true`                  | To display full compiler/linker commands                                |
   | `STATIC=true`                   | For static compilation (only libgcc and libstdc++)                      |
   | `QUIET=true`                    | For less verbose output                                                 |
   | `STRIP=true`                    | To force stripping of debug symbols (adds `-s` linker flag)             |
   | `DEBUG=true`                    | Sets OPTFLAGS to `-O0 -g` and enables more verbose debug logging        |
   | `ARCH=<architecture>`           | To manually set the target architecture                                 |
   | `ADDFLAGS=<flags>`              | For appending flags to both compiler and linker                         |
   | `CXX=<compiler>`                | Manually set which compiler to use                                       |

   Example: `gmake ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

4. **Install**

   ```bash
   sudo gmake install
   ```

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

5. **(Recommended) Set suid bit to make btop always run as root (or other user)**

   ```bash
   sudo gmake setuid
   ```

   No need for `sudo` to see information for non user owned processes and to enable signal sending to any process.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `wheel`

* **Uninstall**

   ```bash
   sudo gmake uninstall
   ```

* **Remove any object files from source dir**

   ```bash
   gmake clean
   ```

* **Remove all object files, binaries and created directories in source dir**

   ```bash
   gmake distclean
   ```

* **Show help**

   ```bash
   gmake help
   ```

</details>
<details>
<summary>

### With CMake (Community maintained)
</summary>

1. **Install build dependencies**

   Requires Clang, CMake, Ninja, Lowdown and Git

   ```bash
   brew update --quiet
   brew install cmake git llvm ninja lowdown
   ```

2. **Clone the repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git && cd btop
   ```

3. **Compile**

   ```bash
   # Configure
   export LLVM_PREFIX="$(brew --prefix llvm)"
   export CXX="$LLVM_PREFIX/bin/clang++"
   export CPPFLAGS="-I$LLVM_PREFIX/include"
   export LDFLAGS="-L$LLVM_PREFIX/lib -L$LLVM_PREFIX/lib/c++ -Wl,-rpath,$LLVM_PREFIX/lib/c++ -fuse-ld=$LLVM_PREFIX/bin/ld64.lld"
   cmake -B build -G Ninja
   # Build
   cmake --build build
   ```

   This will automatically build a release version of btop.

   Some useful options to pass to the configure step:

   | Configure flag                  | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `-DBTOP_LTO=<ON\|OFF>`          | Enables link time optimization (ON by default)                          |


   | `-DCMAKE_INSTALL_PREFIX=<path>` | The installation prefix ('/usr/local' by default)                       |

   To force any specific compiler, run `CXX=<compiler> cmake -B build -G Ninja`

4. **Install**

   ```bash
   cmake --install build
   ```

   May require root privileges

5. **Uninstall**

   CMake doesn't generate an uninstall target by default. To remove installed files, run
   ```
   cat build/install_manifest.txt | xargs rm -irv
   ```

6. **Cleanup build directory**

   ```bash
   cmake --build build -t clean
   ```

</details>

## Compilation FreeBSD

   Requires at least Clang 19 (default) or GCC 14.

   Note that GNU make (`gmake`) is required to compile on FreeBSD.

<details>
<summary>

### With gmake
</summary>

1. **Install dependencies**

   ```bash
   sudo pkg install gmake coreutils git lowdown
   ```

2. **Clone repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   ```bash
   gmake
   ```

   Options for make:

   | Flag                            | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `VERBOSE=true`                  | To display full compiler/linker commands                                |
   | `STATIC=true`                   | For static compilation (only libgcc and libstdc++)                      |
   | `QUIET=true`                    | For less verbose output                                                 |
   | `STRIP=true`                    | To force stripping of debug symbols (adds `-s` linker flag)             |
   | `DEBUG=true`                    | Sets OPTFLAGS to `-O0 -g` and enables more verbose debug logging        |
   | `ARCH=<architecture>`           | To manually set the target architecture                                 |
   | `ADDFLAGS=<flags>`              | For appending flags to both compiler and linker                         |
   | `CXX=<compiler>`                | Manually set which compiler to use                                       |

   Example: `gmake ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

4. **Install**

   ```bash
   sudo gmake install
   ```

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

5. **(Recommended) Set suid bit to make btop always run as root (or other user)**

   ```bash
   sudo gmake setuid
   ```

   No need for `sudo` to see information for non user owned processes and to enable signal sending to any process.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `wheel`

* **Uninstall**

   ```bash
   sudo gmake uninstall
   ```

* **Remove any object files from source dir**

   ```bash
   gmake clean
   ```

* **Remove all object files, binaries and created directories in source dir**

   ```bash
   gmake distclean
   ```

* **Show help**

   ```bash
   gmake help
   ```

</details>
<details>
<summary>

### With CMake (Community maintained)
</summary>

1. **Install build dependencies**

   Requires Clang / GCC, CMake, Ninja, Lowdown and Git

   ```bash
   pkg install cmake ninja lowdown
   ```

2. **Clone the repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git && cd btop
   ```

3. **Compile**

   ```bash
   # Configure
   cmake -B build -G Ninja
   # Build
   cmake --build build
   ```

   This will automatically build a release version of btop.

   Some useful options to pass to the configure step:

   | Configure flag                  | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `-DBTOP_STATIC=<ON\|OFF>`       | Enables static linking (OFF by default)                                 |
   | `-DBTOP_LTO=<ON\|OFF>`          | Enables link time optimization (ON by default)                          |


   | `-DCMAKE_INSTALL_PREFIX=<path>` | The installation prefix ('/usr/local' by default)                       |

   _**Note:** Static linking does not work with GCC._

   To force any other compiler, run `CXX=<compiler> cmake -B build -G Ninja`

4. **Install**

   ```bash
   cmake --install build
   ```

   May require root privileges

5. **Uninstall**

   CMake doesn't generate an uninstall target by default. To remove installed files, run
   ```
   cat build/install_manifest.txt | xargs rm -irv
   ```

6. **Cleanup build directory**

   ```bash
   cmake --build build -t clean
   ```

</details>

## Compilation NetBSD

   Requires at least GCC 14.

   Note that GNU make (`gmake`) is required to compile on NetBSD.

<details>
<summary>

### With gmake
</summary>

1. **Install dependencies**

   ```bash
   /usr/sbin/pkg_add pkgin
   pkgin install -y coregutils gcc14 git gmake
   ```

2. **Clone repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   ```bash
   CXX=/usr/pkg/gcc14/bin/g++ gmake CXXFLAGS="-DNDEBUG"
   ```

   Options for make:

   | Flag                            | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `VERBOSE=true`                  | To display full compiler/linker commands                                |
   | `STATIC=true`                   | For static compilation (only libgcc and libstdc++)                      |
   | `QUIET=true`                    | For less verbose output                                                 |
   | `STRIP=true`                    | To force stripping of debug symbols (adds `-s` linker flag)             |
   | `DEBUG=true`                    | Sets OPTFLAGS to `-O0 -g` and enables more verbose debug logging        |
   | `ARCH=<architecture>`           | To manually set the target architecture                                 |
   | `ADDFLAGS=<flags>`              | For appending flags to both compiler and linker                         |
   | `CXX=<compiler>`                | Manually set which compiler to use                                      |

   Example: `gmake ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

4. **Install**

   ```bash
   sudo gmake install
   ```

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

5. **(Recommended) Set suid bit to make btop always run as root (or other user)**

   ```bash
   sudo gmake setuid
   ```

   No need for `sudo` to see information for non user owned processes and to enable signal sending to any process.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `wheel`

* **Uninstall**

   ```bash
   sudo gmake uninstall
   ```

* **Remove any object files from source dir**

   ```bash
   gmake clean
   ```

* **Remove all object files, binaries and created directories in source dir**

   ```bash
   gmake distclean
   ```

* **Show help**

   ```bash
   gmake help
   ```

</details>
<details>
<summary>

### With CMake (Community maintained)
</summary>

1. **Install build dependencies**

   Requires GCC, CMake, Ninja and Git

   ```bash
   /usr/sbin/pkg_add pkgin
   pkgin install cmake ninja-build gcc14 git
   ```

2. **Clone the repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git && cd btop
   ```

3. **Compile**

   ```bash
   # Configure
   CXX="/usr/pkg/gcc14/bin/g++" cmake -B build -G Ninja
   # Build
   cmake --build build
   ```

   This will automatically build a release version of btop.

   Some useful options to pass to the configure step:

   | Configure flag                  | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `-DBTOP_LTO=<ON\|OFF>`          | Enables link time optimization (ON by default)                          |


   | `-DCMAKE_INSTALL_PREFIX=<path>` | The installation prefix ('/usr/local' by default)                       |

   To force any other compiler, run `CXX=<compiler> cmake -B build -G Ninja`

4. **Install**

   ```bash
   cmake --install build
   ```

   May require root privileges

5. **Uninstall**

   CMake doesn't generate an uninstall target by default. To remove installed files, run
   ```
   cat build/install_manifest.txt | xargs rm -irv
   ```

6. **Cleanup build directory**

   ```bash
   cmake --build build -t clean
   ```

</details>

## Compilation OpenBSD

   Note that GNU make (`gmake`) is required to compile on OpenBSD.

<details>
<summary>

### With gmake
</summary>

1. **Install dependencies**

   ```bash
   pkg_add coreutils git gmake lowdown
   ```

2. **Clone repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   ```bash
   gmake
   ```

   Options for make:

   | Flag                            | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `VERBOSE=true`                  | To display full compiler/linker commands                                |
   | `STATIC=true`                   | For static compilation (only libgcc and libstdc++)                      |
   | `QUIET=true`                    | For less verbose output                                                 |
   | `STRIP=true`                    | To force stripping of debug symbols (adds `-s` linker flag)             |
   | `DEBUG=true`                    | Sets OPTFLAGS to `-O0 -g` and enables more verbose debug logging        |
   | `ARCH=<architecture>`           | To manually set the target architecture                                 |
   | `ADDFLAGS=<flags>`              | For appending flags to both compiler and linker                         |
   | `CXX=<compiler>`                | Manually set which compiler to use                                       |

   Example: `gmake ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

4. **Install**

   ```bash
   sudo gmake install
   ```

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

5. **(Recommended) Set suid bit to make btop always run as root (or other user)**

   ```bash
   sudo gmake setuid
   ```

   No need for `sudo` to see information for non user owned processes and to enable signal sending to any process.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `wheel`

* **Uninstall**

   ```bash
   sudo gmake uninstall
   ```

* **Remove any object files from source dir**

   ```bash
   gmake clean
   ```

* **Remove all object files, binaries and created directories in source dir**

   ```bash
   gmake distclean
   ```

* **Show help**

   ```bash
   gmake help
   ```

</details>
<details>
<summary>

### With CMake (Community maintained)
</summary>

1. **Install build dependencies**

   Requires GCC, CMake, Ninja, Lowdown and Git

   _**Note:** LLVM's libc++ shipped with OpenBSD 7.4 is too old and cannot compile btop._

   ```bash
   pkg_add cmake git ninja lowdown
   ```

2. **Clone the repository**

   ```bash
   git clone https://github.com/aristocratos/btop.git && cd btop
   ```

3. **Compile**

   ```bash
   # Configure
   cmake -B build -G Ninja
   # Build
   cmake --build build
   ```

   This will automatically build a release version of btop.

   Some useful options to pass to the configure step:

   | Configure flag                  | Description                                                             |
   |---------------------------------|-------------------------------------------------------------------------|
   | `-DBTOP_LTO=<ON\|OFF>`          | Enables link time optimization (ON by default)                          |
   | `-DCMAKE_INSTALL_PREFIX=<path>` | The installation prefix ('/usr/local' by default)                       |

   To force any other compiler, run `CXX=<compiler> cmake -B build -G Ninja`

4. **Install**

   ```bash
   cmake --install build
   ```

   May require root privileges

5. **Uninstall**

   CMake doesn't generate an uninstall target by default. To remove installed files, run
   ```
   cat build/install_manifest.txt | xargs rm -irv
   ```

6. **Cleanup build directory**

   ```bash
   cmake --build build -t clean
   ```

</details>

## Testing

Testing requires [CMake](cmake.org). Tests are build by default and can be run with `ctest --test-dir <build>`.

If you want to disable building tests, pass `-DBUILD_TESTING=OFF` to the configure step.

## Installing the snap
[![btop](https://snapcraft.io/btop/badge.svg)](https://snapcraft.io/btop)

### Note: there are now two snaps available: `btop` and `btop-desktop`. The desktop version is much larger and includes the desktop entries needed to allow for launching `btop` with a click.

 * **Install the snap**

    ```bash
    sudo snap install btop
    or
    sudo snap install btop-desktop
    ```
 * **Install the latest snap from the edge channel**
   ```
   sudo snap install btop --edge
   or
   sudo snap install btop-desktop --edge
   ```

 * **Connect the interface**

    ```bash
	sudo snap connect btop:removable-media
	or
	sudo snap connect btop-desktop:removable-media
	```

## Configurability

All options changeable from within UI.
Config and log files stored in `$XDG_CONFIG_HOME/btop` or `$HOME/.config/btop` folder

#### btop.conf: (auto generated if not found)

```toml
#? Config file for btop v.1.4.5

#* Name of a btop++/bpytop/bashtop formatted ".theme" file, "Default" and "TTY" for builtin themes.
#* Themes should be placed in "../share/btop/themes" relative to binary or "$HOME/.config/btop/themes"
color_theme = "Default"

#* If the theme set background should be shown, set to False if you want terminal background transparency.
theme_background = true

#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false.
truecolor = true

#* Set to true to force tty mode regardless if a real tty has been detected or not.
#* Will force 16-color mode and TTY theme, set all graph symbols to "tty" and swap out other non tty friendly symbols.
force_tty = false

#* Define presets for the layout of the boxes. Preset 0 is always all boxes shown with default settings. Max 9 presets.
#* Format: "box_name:P:G,box_name:P:G" P=(0 or 1) for alternate positions, G=graph symbol to use for box.
#* Use whitespace " " as separator between different presets.
#* Example: "cpu:0:default,mem:0:tty,proc:1:default cpu:0:braille,proc:0:tty"
presets = "cpu:1:default,proc:0:default cpu:0:default,mem:0:default,net:0:default cpu:0:block,net:0:tty"

#* Set to True to enable "h,j,k,l,g,G" keys for directional control in lists.
#* Conflicting keys for h:"help" and k:"kill" is accessible while holding shift.
vim_keys = false

#* Rounded corners on boxes, is ignored if TTY mode is ON.
rounded_corners = true

#* Use terminal synchronized output sequences to reduce flickering on supported terminals.
terminal_sync = true

#* Default symbols to use for graph creation, "braille", "block" or "tty".
#* "braille" offers the highest resolution but might not be included in all fonts.
#* "block" has half the resolution of braille but uses more common characters.
#* "tty" uses only 3 different symbols but will work with most fonts and should work in a real TTY.
#* Note that "tty" only has half the horizontal resolution of the other two, so will show a shorter historical view.
graph_symbol = "braille"

# Graph symbol to use for graphs in cpu box, "default", "braille", "block" or "tty".
graph_symbol_cpu = "default"

# Graph symbol to use for graphs in cpu box, "default", "braille", "block" or "tty".
graph_symbol_mem = "default"

# Graph symbol to use for graphs in cpu box, "default", "braille", "block" or "tty".
graph_symbol_net = "default"

# Graph symbol to use for graphs in cpu box, "default", "braille", "block" or "tty".
graph_symbol_proc = "default"

#* Manually set which boxes to show. Available values are "cpu mem net proc" and "gpu0" through "gpu5", separate values with whitespace.
shown_boxes = "cpu mem net proc"

#* Update time in milliseconds, recommended 2000 ms or above for better sample times for graphs.
update_ms = 2000

#* Processes sorting, "pid" "program" "arguments" "threads" "user" "memory" "cpu lazy" "cpu direct",
#* "cpu lazy" sorts top process over time (easier to follow), "cpu direct" updates top process directly.
proc_sorting = "cpu lazy"

#* Reverse sorting order, True or False.
proc_reversed = false

#* Show processes as a tree.
proc_tree = false

#* Use the cpu graph colors in the process list.
proc_colors = true

#* Use a darkening gradient in the process list.
proc_gradient = true

#* If process cpu usage should be of the core it's running on or usage of the total available cpu power.
proc_per_core = false

#* Show process memory as bytes instead of percent.
proc_mem_bytes = true

#* Show cpu graph for each process.
proc_cpu_graphs = true

#* Use /proc/[pid]/smaps for memory information in the process info box (very slow but more accurate)
proc_info_smaps = false

#* Show proc box on left side of screen instead of right.
proc_left = false

#* (Linux) Filter processes tied to the Linux kernel(similar behavior to htop).
proc_filter_kernel = false

#* Should the process list follow the selected process when detailed view is open.
proc_follow_detailed = true

#* In tree-view, always accumulate child process resources in the parent process.
proc_aggregate = false

#* Should cpu and memory usage display be preserved for dead processes when paused.
keep_dead_proc_usage = false

#* Sets the CPU stat shown in upper half of the CPU graph, "total" is always available.
#* Select from a list of detected attributes from the options menu.
cpu_graph_upper = "Auto"

#* Sets the CPU stat shown in lower half of the CPU graph, "total" is always available.
#* Select from a list of detected attributes from the options menu.
cpu_graph_lower = "Auto"

#* Toggles if the lower CPU graph should be inverted.
cpu_invert_lower = true

#* Set to True to completely disable the lower CPU graph.
cpu_single_graph = false

#* Show cpu box at bottom of screen instead of top.
cpu_bottom = false

#* Shows the system uptime in the CPU box.
show_uptime = true

#* Shows the CPU package current power consumption in watts. Requires running `make setcap` or `make setuid` or running with sudo.
show_cpu_watts = true

#* Show cpu temperature.
check_temp = true

#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors.
cpu_sensor = "Auto"

#* Show temperatures for cpu cores also if check_temp is True and sensors has been found.
show_coretemp = true

#* Set a custom mapping between core and coretemp, can be needed on certain cpus to get correct temperature for correct core.
#* Use lm-sensors or similar to see which cores are reporting temperatures on your machine.
#* Format "x:y" x=core with wrong temp, y=core with correct temp, use space as separator between multiple entries.
#* Example: "4:0 5:1 6:3"
cpu_core_map = ""

#* Which temperature scale to use, available values: "celsius", "fahrenheit", "kelvin" and "rankine".
temp_scale = "celsius"

#* Use base 10 for bits/bytes sizes, KB = 1000 instead of KiB = 1024.
base_10_sizes = false

#* Show CPU frequency.
show_cpu_freq = true

#* How to calculate CPU frequency, available values: "first", "range", "lowest", "highest" and "average".
freq_mode = "first"

#* Draw a clock at top of screen, formatting according to strftime, empty string to disable.
#* Special formatting: /host = hostname | /user = username | /uptime = system uptime
clock_format = "%X"

#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort.
background_update = true

#* Custom cpu model name, empty string to disable.
custom_cpu_name = ""

#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with whitespace " ".
#* Only disks matching the filter will be shown. Prepend exclude= to only show disks not matching the filter. Examples: disk_filter="/boot /home/user", disks_filter="exclude=/boot /home/user"
disks_filter = ""

#* Show graphs instead of meters for memory values.
mem_graphs = true

#* Show mem box below net box instead of above.
mem_below_net = false

#* Count ZFS ARC in cached and available memory.
zfs_arc_cached = true

#* If swap memory should be shown in memory box.
show_swap = true

#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk.
swap_disk = true

#* If mem box should be split to also show disks info.
show_disks = true

#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar.
only_physical = true

#* Read disks list from /etc/fstab. This also disables only_physical.
use_fstab = true

#* Setting this to True will hide all datasets, and only show ZFS pools. (IO stats will be calculated per-pool)
zfs_hide_datasets = false

#* Set to true to show available disk space for privileged users.
disk_free_priv = false

#* Toggles if io activity % (disk busy time) should be shown in regular disk usage view.
show_io_stat = true

#* Toggles io mode for disks, showing big graphs for disk read/write speeds.
io_mode = false

#* Set to True to show combined read/write io graphs in io mode.
io_graph_combined = false

#* Set the top speed for the io graphs in MiB/s (100 by default), use format "mountpoint:speed" separate disks with whitespace " ".
#* Example: "/mnt/media:100 /:20 /boot:1".
io_graph_speeds = ""

#* Set fixed values for network graphs in Mebibits. Is only used if net_auto is also set to False.
net_download = 100

net_upload = 100

#* Use network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest.
net_auto = true

#* Sync the auto scaling for download and upload to whichever currently has the highest scale.
net_sync = true

#* Starts with the Network Interface specified here.
net_iface = ""

#* "True" shows bitrates in base 10 (Kbps, Mbps). "False" shows bitrates in binary sizes (Kibps, Mibps, etc.). "Auto" uses base_10_sizes.
base_10_bitrate = "Auto"

#* Show battery stats in top right if battery is present.
show_battery = true

#* Which battery to use if multiple are present. "Auto" for auto detection.
selected_battery = "Auto"

#* Show power stats of battery next to charge indicator.
show_battery_watts = true

#* Set loglevel for "~/.local/state/btop.log" levels are: "ERROR" "WARNING" "INFO" "DEBUG".
#* The level set includes all lower levels, i.e. "DEBUG" will show all logging info.
log_level = "WARNING"
```

#### Command line options

```text
Usage: btop [OPTIONS]

Options:
  -c, --config <file>     Path to a config file
  -d, --debug             Start in debug mode with additional logs and metrics
  -f, --filter <filter>   Set an initial process filter
      --force-utf         Override automatic UTF locale detection
  -l, --low-color         Disable true color, 256 colors only
  -p, --preset <id>       Start with a preset (0-9)
  -t, --tty               Force tty mode with ANSI graph symbols and 16 colors only
      --no-tty            Force disable tty mode
  -u, --update <ms>       Set an initial update rate in milliseconds
      --default-config    Print default config to standard output
  -h, --help              Show this help message and exit
  -V, --version           Show a version message and exit (more with --version)
```

## LICENSE

[Apache License 2.0](LICENSE)
