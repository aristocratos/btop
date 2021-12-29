# ![btop++](Img/logo.png)

<a href="https://repology.org/project/btop/versions">
    <img src="https://repology.org/badge/vertical-allrepos/btop.svg" alt="Packaging status" align="right">
</a>

![Linux](https://img.shields.io/badge/-Linux-grey?logo=linux)
![OSX](https://img.shields.io/badge/-OSX-black?logo=apple)
![FreeBSD](https://img.shields.io/badge/-FreeBSD-red?logo=freebsd)
![Usage](https://img.shields.io/badge/Usage-System%20resource%20monitor-yellow)
![c++20](https://img.shields.io/badge/cpp-c%2B%2B20-green)
![latest_release](https://img.shields.io/github/v/tag/aristocratos/btop?label=release)
[![Donate](https://img.shields.io/badge/-Donate-yellow?logo=paypal)](https://paypal.me/aristocratos)
[![Sponsor](https://img.shields.io/badge/-Sponsor-red?logo=github)](https://github.com/sponsors/aristocratos)
[![Coffee](https://img.shields.io/badge/-Buy%20me%20a%20Coffee-grey?logo=Ko-fi)](https://ko-fi.com/aristocratos)
[![btop](https://snapcraft.io/btop/badge.svg)](https://snapcraft.io/btop)
[![Continuous Build Linux](https://github.com/aristocratos/btop/actions/workflows/continuous-build-linux.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-linux.yml)
[![Continuous Build MacOS](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml/badge.svg)](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml)

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
* [Installation Linux/OSX](#installation)
* [Compilation Linux](#compilation-linux)
* [Compilation OSX](#compilation-osx)
* [Installing the snap](#installing-the-snap)
* [Configurability](#configurability)
* [License](#license)

## News

### Under development

##### 13 November 2021

Release v1.1.0 with OSX support. Binaries in [continuous-build-macos](https://github.com/aristocratos/btop/actions/workflows/continuous-build-macos.yml) are only x86 for now.
Macos binaries + installer are included for both x86 and ARM64 (Apple Silicon) in the releases.

Big thank you to [@joske](https://github.com/joske) who wrote the vast majority of the implementation!

<details>
<summary>More...</summary>

##### 30 October 2021

Work on the OSX and FreeBSD branches, both initiated and mostly worked on by [@joske](https://github.com/joske), will likely be completed in the coming weeks.
The OSX branch has some memory leaks that needs to be sorted out and both have some issues with the processes cpu usage calculation and other smaller issues that needs fixing.

If you want to help out, test for bugs/fix bugs or just try out the branches:

**OSX**
```bash
# Install and use Homebrew or MacPorts package managers for easy dependency installation
brew install coreutils make gcc@11
git clone https://github.com/aristocratos/btop.git
cd btop
git checkout OSX
gmake
```

**FreeBSD**
```bash
sudo pkg install gmake gcc11 coreutils git
git clone https://github.com/aristocratos/btop.git
cd btop
git checkout freebsd
gmake
```

Note that GNU make (`gmake`) is recommended but not required for OSX but it is required on FreeBSD.


##### 6 October 2021

OsX development have been started by [@joske](https://github.com/joske), big thanks :)
See branch [OSX](https://github.com/aristocratos/btop/tree/OSX) for current progress.

##### 18 September 2021

The Linux version of btop++ is complete. Released as version 1.0.0

I will be providing statically compiled binaries for a range of architectures in every release for those having problems compiling.

For compilation GCC 10 is required, GCC 11 preferred.

Please report any bugs to the [Issues](https://github.com/aristocratos/btop/issues/new?assignees=aristocratos&labels=bug&template=bug_report.md&title=%5BBUG%5D) page.

The development plan right now:

* 1.1.0 Mac OsX support
* 1.2.0 FreeBSD support
* 1.3.0 Support for GPU monitoring
* 1.X.0 Other platforms and features...

Windows support is not in the plans as of now, but if anyone else wants to take it on, I will try to help.

##### 5 May 2021

This project is gonna take some time until it has complete feature parity with bpytop, since all system information gathering will have to be written from scratch without any external libraries.
And will need some help in the form of code contributions to get complete support for BSD and OSX.

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
* Full mouse support, all buttons with a highlighted key is clickable and mouse scroll works in process list and menu boxes.
* Fast and responsive UI with UP, DOWN keys process selection.
* Function for showing detailed stats for selected process.
* Ability to filter processes.
* Easy switching between sorting options.
* Tree view of processes.
* Send any signal to selected process.
* UI menu for changing all config file options.
* Auto scaling graph for network usage.
* Shows IO activity and speeds for disks
* Battery meter
* Selectable symbols for the graphs
* Custom presets
* And more...

## Themes

Btop++ uses the same theme files as bpytop and bashtop (some color values missing in bashtop themes) .

See [themes](https://github.com/aristocratos/btop/tree/master/themes) folder for available themes.

The `make install` command places the default themes in `[$PREFIX or /usr/local]/share/btop/themes`.
User created themes should be placed in `$XDG_CONFIG_HOME/btop/themes` or `$HOME/.config/btop/themes`.

Let me know if you want to contribute with new themes.

## Support and funding

You can sponsor this project through github, see [my sponsors page](https://github.com/sponsors/aristocratos) for options.

Or donate through [paypal](https://paypal.me/aristocratos) or [ko-fi](https://ko-fi.com/aristocratos).

Any support is greatly appreciated!

## Prerequisites

For best experience, a terminal with support for:

* 24-bit truecolor ([See list of terminals with truecolor support](https://gist.github.com/XVilka/8346728))
* 256-color terminals are supported through 24-bit to 256-color conversion when setting "truecolor" to False in the options or with "-lc/--low-color" arguments.
* 16 color TTY mode will be activated if a real tty device is detected. Can be forced with "-t/--tty_on" arguments.
* Wide characters (Are sometimes problematic in web-based terminals)

Also needs a UTF8 locale and a font that covers:

* Unicode Block “Braille Patterns” U+2800 - U+28FF (Not needed in TTY mode or with graphs set to type: block or tty.)
* Unicode Block “Geometric Shapes” U+25A0 - U+25FF
* Unicode Block "Box Drawing" and "Block Elements" U+2500 - U+259F

### **Notice (Text rendering issues)**

* If you are having problems with the characters in the graphs not looking like they do in the screenshots, it's likely a problem with your systems configured fallback font not having support for braille characters.

* See [Terminess Powerline](https://github.com/ryanoasis/nerd-fonts/tree/master/patched-fonts/Terminus/terminus-ttf-4.40.1) for an example of a font that includes the braille symbols.

* See comments by @sgleizes [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-684036827) and @XenHat [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-691585587) in issue #100 for possible solutions.

* If text are misaligned and you are using Konsole or Yakuake, turning off "Bi-Directional text rendering" is a possible fix.

* Characters clipping in to each other or text/border misalignments is not bugs caused by btop, but most likely a fontconfig or terminal problem where the braille characters making up the graphs aren't rendered correctly.

* Look to the creators of the terminal emulator you use to fix these issues if the previous mentioned fixes don't work for you.

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

**Binaries for Linux are statically compiled with musl and works on kernel 2.6.39 and newer**

1. **Download btop-(VERSION)-(ARCH)-(PLATFORM).tbz from [latest release](https://github.com/aristocratos/btop/releases/latest) and unpack to a new folder**

   **Notice! Use x86_64 for 64-bit x86 systems, i486 and i686 are 32-bit!**

2. **Install (from created folder)**

   * **Run install.sh or:**

   ``` bash
   # use "make install PREFIX=/target/dir" to set target, default: /usr/local
   # only use "sudo" when installing to a NON user owned directory
   sudo make install
   ```

3. **(Optional) Set suid bit to make btop always run as root (or other user)**

   Enables signal sending to any process without starting with `sudo` and can prevent /proc read permissions problems on some systems.

   * **Run setuid.sh or:**

   ``` bash
   # run after make install and use same PREFIX if any was used at install
   # set SU_USER and SU_GROUP to select user and group, default is root:root
   sudo make setuid
   ```

* **Uninstall**

  * **Run uninstall.sh or:**

   ``` bash
   sudo make uninstall
   ```

* **Show help**

   ```bash
   make help
   ```

**Binary release (from native os repo)**

* **openSUSE**
  * **Add repo**
  ```bash
  sudo zypper ar --refresh obs://home:Werwolf2517 home:Werwolf2517
  ```
  * **Refresh metadata**
  ```bash
  sudo zypper ref
  ```
  * **Install package**
  ```bash
  sudo zypper in btop
  ```

**Binary release on Homebrew (macOS (x86_64 & ARM64) / Linux (x86_64))**

* **[Homebrew](https://formulae.brew.sh/formula/btop)**
  ```bash
  brew install btop
  ```

## Compilation Linux

   Needs GCC 10 or higher, (GCC 11 or above strongly recommended for better CPU efficiency in the compiled binary).

   The makefile also needs GNU coreutils and `sed` (should already be installed on any modern distribution).

   For a `cmake` based build alternative see the [fork](https://github.com/jan-guenter/btop/tree/main) by @jan-guenter

1. **Install dependencies (example for Ubuntu 21.04 Hirsute)**

   Use gcc-10 g++-10 if gcc-11 isn't available

   ``` bash
   sudo apt install coreutils sed git build-essential gcc-11 g++-11
   ```

2. **Clone repository**

   ``` bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   Append `STATIC=true` to `make` command for static compilation.

   Notice! If using LDAP Authentication, usernames will show as UID number for LDAP users if compiling statically with glibc.

   Append `QUIET=true` for less verbose output.

   Append `STRIP=true` to force stripping of debug symbols (adds `-s` linker flag).

   Append `ARCH=<architecture>` to manually set the target architecture.
   If omitted the makefile uses the machine triple (output of `-dumpmachine` compiler parameter) to detect the target system.

   Use `ADDFLAGS` variable for appending flags to both compiler and linker.

   For example: `ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

   If `g++` is linked to an older version of gcc on your system specify the correct version by appending `CXX=g++-10` or `CXX=g++-11`.

   ``` bash
   make
   ```

4. **Install**

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

   ``` bash
   sudo make install
   ```

5. **(Optional) Set suid bit to make btop always run as root (or other user)**

   No need for `sudo` to enable signal sending to any process and to prevent /proc read permissions problems on some systems.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `root`

   ``` bash
   sudo make setuid
   ```

* **Uninstall**

   ``` bash
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

## Compilation OSX

   Needs GCC 10 or higher, (GCC 11 or above strongly recommended for better CPU efficiency in the compiled binary).

   The makefile also needs GNU coreutils and `sed`.

   Install and use Homebrew or MacPorts package managers for easy dependency installation

1. **Install dependencies (example for Homebrew)**

   ``` bash
   brew install coreutils make gcc@11
   ```

2. **Clone repository**

   ``` bash
   git clone https://github.com/aristocratos/btop.git
   cd btop
   ```

3. **Compile**

   Append `STATIC=true` to `make` command for static compilation (only libgcc and libstdc++ will be static!).

   Append `QUIET=true` for less verbose output.

   Append `STRIP=true` to force stripping of debug symbols (adds `-s` linker flag).

   Append `ARCH=<architecture>` to manually set the target architecture.
   If omitted the makefile uses the machine triple (output of `-dumpmachine` compiler parameter) to detect the target system.

   Use `ADDFLAGS` variable for appending flags to both compiler and linker.

   For example: `ADDFLAGS=-march=native` might give a performance boost if compiling only for your own system.

   ``` bash
   gmake
   ```

4. **Install**

   Append `PREFIX=/target/dir` to set target, default: `/usr/local`

   Notice! Only use "sudo" when installing to a NON user owned directory.

   ``` bash
   sudo gmake install
   ```

5. **(Recommended) Set suid bit to make btop always run as root (or other user)**

   No need for `sudo` to see information for non user owned processes and to enable signal sending to any process.

   Run after make install and use same PREFIX if any was used at install.

   Set `SU_USER` and `SU_GROUP` to select user and group, default is `root` and `wheel`

   ``` bash
   sudo gmake setuid
   ```

* **Uninstall**

   ``` bash
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


## Installing the snap
[![btop](https://snapcraft.io/btop/badge.svg)](https://snapcraft.io/btop)

 * **Install the snap**

    ```bash
    sudo snap install btop
    ```

 * **Connect the interfaces**

    ```bash
	sudo snap connect btop:system-observe
    sudo snap connect btop:physical-memory-observe
    sudo snap connect btop:mount-observe
    sudo snap connect btop:hardware-observe
	sudo snap connect btop:network-observe
	sudo snap connect btop:process-control
	```


## Configurability

All options changeable from within UI.
Config and log files stored in `$XDG_CONFIG_HOME/btop` or `$HOME/.config/btop` folder

#### btop.cfg: (auto generated if not found)

```bash
#? Config file for btop v. 0.1.0

#* Name of a btop++/bpytop/bashtop formatted ".theme" file, "Default" and "TTY" for builtin themes.
#* Themes should be placed in "../share/btop/themes" relative to binary or "$HOME/.config/btop/themes"
color_theme = "Default"

#* If the theme set background should be shown, set to False if you want terminal background transparency.
theme_background = False

#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false.
truecolor = True

#* Set to true to force tty mode regardless if a real tty has been detected or not.
#* Will force 16-color mode and TTY theme, set all graph symbols to "tty" and swap out other non tty friendly symbols.
force_tty = False

#* Define presets for the layout of the boxes. Preset 0 is always all boxes shown with default settings. Max 9 presets.
#* Format: "box_name:P:G,box_name:P:G" P=(0 or 1) for alternate positions, G=graph symbol to use for box.
#* Use withespace " " as separator between different presets.
#* Example: "cpu:0:default,mem:0:tty,proc:1:default cpu:0:braille,proc:0:tty"
presets = "cpu:1:default,proc:0:default cpu:0:default,mem:0:default,net:0:default cpu:0:block,net:0:tty"

#* Rounded corners on boxes, is ignored if TTY mode is ON.
rounded_corners = True

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

#* Manually set which boxes to show. Available values are "cpu mem net proc", separate values with whitespace.
shown_boxes = "cpu mem net proc"

#* Update time in milliseconds, recommended 2000 ms or above for better sample times for graphs.
update_ms = 2000

#* Processes sorting, "pid" "program" "arguments" "threads" "user" "memory" "cpu lazy" "cpu responsive",
#* "cpu lazy" sorts top process over time (easier to follow), "cpu responsive" updates top process directly.
proc_sorting = "cpu lazy"

#* Reverse sorting order, True or False.
proc_reversed = False

#* Show processes as a tree.
proc_tree = False

#* Use the cpu graph colors in the process list.
proc_colors = True

#* Use a darkening gradient in the process list.
proc_gradient = True

#* If process cpu usage should be of the core it's running on or usage of the total available cpu power.
proc_per_core = True

#* Show process memory as bytes instead of percent.
proc_mem_bytes = True

#* Use /proc/[pid]/smaps for memory information in the process info box (very slow but more accurate)
proc_info_smaps = False

#* Show proc box on left side of screen instead of right.
proc_left = False

#* Sets the CPU stat shown in upper half of the CPU graph, "total" is always available.
#* Select from a list of detected attributes from the options menu.
cpu_graph_upper = "total"

#* Sets the CPU stat shown in lower half of the CPU graph, "total" is always available.
#* Select from a list of detected attributes from the options menu.
cpu_graph_lower = "total"

#* Toggles if the lower CPU graph should be inverted.
cpu_invert_lower = True

#* Set to True to completely disable the lower CPU graph.
cpu_single_graph = False

#* Show cpu box at bottom of screen instead of top.
cpu_bottom = False

#* Shows the system uptime in the CPU box.
show_uptime = True

#* Show cpu temperature.
check_temp = True

#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors.
cpu_sensor = "Auto"

#* Show temperatures for cpu cores also if check_temp is True and sensors has been found.
show_coretemp = True

#* Set a custom mapping between core and coretemp, can be needed on certain cpus to get correct temperature for correct core.
#* Use lm-sensors or similar to see which cores are reporting temperatures on your machine.
#* Format "x:y" x=core with wrong temp, y=core with correct temp, use space as separator between multiple entries.
#* Example: "4:0 5:1 6:3"
cpu_core_map = ""

#* Which temperature scale to use, available values: "celsius", "fahrenheit", "kelvin" and "rankine".
temp_scale = "celsius"

#* Show CPU frequency.
show_cpu_freq = True

#* Draw a clock at top of screen, formatting according to strftime, empty string to disable.
#* Special formatting: /host = hostname | /user = username | /uptime = system uptime
clock_format = "%H:%M"

#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort.
background_update = True

#* Custom cpu model name, empty string to disable.
custom_cpu_name = ""

#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with whitespace " ".
#* Begin line with "exclude=" to change to exclude filter, otherwise defaults to "most include" filter. Example: disks_filter="exclude=/boot /home/user".
disks_filter = "exclude=/boot"

#* Show graphs instead of meters for memory values.
mem_graphs = True

#* Show mem box below net box instead of above.
mem_below_net = False

#* If swap memory should be shown in memory box.
show_swap = True

#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk.
swap_disk = True

#* If mem box should be split to also show disks info.
show_disks = True

#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar.
only_physical = True

#* Read disks list from /etc/fstab. This also disables only_physical.
use_fstab = False

#* Toggles if io activity % (disk busy time) should be shown in regular disk usage view.
show_io_stat = True

#* Toggles io mode for disks, showing big graphs for disk read/write speeds.
io_mode = False

#* Set to True to show combined read/write io graphs in io mode.
io_graph_combined = False

#* Set the top speed for the io graphs in MiB/s (100 by default), use format "mountpoint:speed" separate disks with whitespace " ".
#* Example: "/mnt/media:100 /:20 /boot:1".
io_graph_speeds = ""

#* Set fixed values for network graphs in Mebibits. Is only used if net_auto is also set to False.
net_download = 100

net_upload = 100

#* Use network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest.
net_auto = True

#* Sync the auto scaling for download and upload to whichever currently has the highest scale.
net_sync = False

#* Starts with the Network Interface specified here.
net_iface = "br0"

#* Show battery stats in top right if battery is present.
show_battery = True

#* Set loglevel for "~/.config/btop/btop.log" levels are: "ERROR" "WARNING" "INFO" "DEBUG".
#* The level set includes all lower levels, i.e. "DEBUG" will show all logging info.
log_level = "DEBUG"
```

#### Command line options

```text
usage: btop [-h] [-v] [-/+t] [-p <id>] [--utf-force] [--debug]

optional arguments:
  -h, --help            show this help message and exit
  -v, --version         show version info and exit
  -lc, --low-color      disable truecolor, converts 24-bit colors to 256-color
  -t, --tty_on          force (ON) tty mode, max 16 colors and tty friendly graph symbols
  +t, --tty_off         force (OFF) tty mode
  -p, --preset <id>     start with preset, integer value between 0-9
  --utf-force           force start even if no UTF-8 locale was detected
  --debug               start in DEBUG mode: shows microsecond timer for information collect
                        and screen draw functions and sets loglevel to DEBUG
```

## LICENSE

[Apache License 2.0](LICENSE)
