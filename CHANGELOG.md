## v1.2.9

* Fixed: Memory values not clearing properly when not in graph mode in mem box

* Changed: kyli0x theme color update, by @kyli0x

* Added: Elementarish theme, by @dennismayr

* Added: key "?" to see help, by @mohi001

* Added: solarized_light theme, by @Fingerzam

* Changed: Made ZFS stats collection compatible with zfs_pools_only option, by @simplepad

* Changed: Rewrite of process sorting and tree generation including fixes for tree sorting and mouse support

* Added: Option to hide the small cpu graphs for processes

* Changed: Small graphs now show colors for each character

* Fixed: Getting selfpath on macos (fix for finding theme folder)

## v1.2.8

* Added: Support for ZFS pool io stats monitoring, by @simplepad

* Added: Filtering of kernel processes, by @0xJoeMama

* Added: New theme everforest-dark-hard, by @iambeingtracked

* Added: New theme tomorrow-night, by @appuchias

* Changed: Disable battery monitoring if it fails instead of exiting

## v1.2.7

* Fixed: Disk IO stats for individual partitions instead of whole disk (Linux)

* Added: Case insensitive process filtering, by @abrasumente233

* Added: Include ZFS ARC in cached/available memory on Linux, by @mattico

* Added: Desktop entry and icons, by @yonatan8070

* Fixed: Net sync scale bug

* Added: tokyo-night & tokyo-storm themes, by @Schievel1

## v1.2.6

* Fixed: Wrong memory unit when shorten and size is less than 10, by @mohi001

* Fixed: Use cpu cores avarage temp if missing cpu package temp for FreeBSD

* Changed: Enter symbol to a more common variant

## v1.2.5

* Fixed: Fallback to less accurate UTF8 char count if conversion to wstring fails

* Fixed: Small ui fixes for mem and disks

* Added: New theme HotPurpleTrafficLight, by @pallebone

* Fixed: title_left symbol between auto and zero in the net box is not displayed, by @mrdotx

* Fixed: Mouse mappings for net box

## v1.2.4

* Optimization: Proc::draw()

* Fixed: Ignore duplicate disks with same mountpoint

* Changed: Restrict command line for processes to 1000 characters to fix utf8 conversion errors

* Added: add "g" and "G" to vim keys, by @mohi001

## v1.2.3

* Changed: floating_humanizer() now show fractions when shortened and value is < 10

* Fixed: Process tree not redrawing properly

* Fixed: string to wstring conversion crash when string is too big

## v1.2.2

* Changed: Reverted uncolor() back to using regex to fix delay in opening menu when compiled with musl

* Added: Toggle for showing free disk space for privileged or normal users

* Added: Clarification on signal screen that number can be manually entered

## v1.2.1

* Added: Arrow only after use of "f" when filtering processes, by @NavigationHazard

* Fixed: Fx::uncolor not removing all escapes

* Fixed: Text alignment for popup boxes

* Fixed: Terminal resize warning getting stuck

* Removed: Unnecessary counter for atomic_lock

* Added: Percentage progress to Makefile

* Fixed: Alignment of columns in proc box when wide UTF8 characters are used

* Fixed: Battery meter draw fix

## v1.2.0

* Added: Support for FreeBSD, by @joske and @aristocratos

* Fixed (again): Account for system rolling over net speeds in Net::collect()

* Added: Theme gruvbox_material_dark, by @marcoradocchia

* Added: Option for base 10 bytes/bits

## v1.1.5

* Fixed: Account for system rolling over net speeds in Net::collect()

## v1.1.4

* Fixed: Create dependency files in build directory when compiling, by @stwnt

* Fixed: fix CPU temp fallback on macOS, by @joske

* Changed: From rng::sort() to rng::stable_sort() for more stability

* Fixed: in_avail() can always be zero, by @pg83

## v1.1.3

* Added: New theme ayu, by @AlphaNecron

* Added: New theme gruvbox_dark_v2, by @pietryszak

* Fixed: Macos cpu coretemp for Intel, by @joske

* Added: New theme OneDark, by @vtmx

* Fixed: Fixed network graph scale int rollover

* Fixed: Suspected possibility of very rare stall in Input::clear()

## v1.1.2

* Fixed: SISEGV on macos Mojave, by @mgradowski

* Fixed: Small optimizations and fixes to Mem::collect() and Input::get()

* Fixed: Wrong unit for net_upload and net_download in config menu

* Fixed: UTF-8 detection on macos

* Fixed: coretemp iteration due to missing tempX_input, by @KFilipek

* Fixed: coretemp ordering

## v1.1.1

* Added: Partial static build (libgcc, libstdc++) for macos

* Changed: Continuous build macos switched to OSX 11.6 (Big Sur) and partial static build

* Changed: Release binaries for macos switched to OSX 12 (Monterey) and partial static build

## v1.1.0

* Added: Support for OSX, by @joske and @aristocratos

## v1.0.24

* Changed: Collection ordering

* Fixed: Restore all escape seq mouse modes on exit

* Fixed: SIGINT not cleaning up on exit

## v1.0.23

* Fixed: Config parser missing first value when not including version header

* Fixed: Vim keys menu lists selection

* Fixed: Stall when clearing input queue on exit and queue is >1

* Fixed: Inconsistent behaviour of "q" key in the menus

## v1.0.22

* Fixed: Bad values for disks and network on 32-bit

## v1.0.21

* Fixed: Removed extra spaces in cpu name

* Added: / as alternative bind for filter

* Fixed: Security issue when running with SUID bit set

## v1.0.20

* Added: Improved cpu sensor detection for Ryzen Mobile, by @adnanpri

* Changed: Updated makefile

* Changed: Regex for Fx::uncolor() changed to string search and replace

* Changed: Removed all use of regex with dedicated string functions

## v1.0.19

* Fixed: Makefile now tests compiler flag compatibility

## v1.0.18

* Fixed: Makefile g++ -dumpmachine failure to get platform on some distros

## v1.0.17

* Changed: Reverted mutexes back to custom atomic bool based locks

* Added: Static binaries switched to building with musl + more platforms, by @jan-guenter

* Fixed: Improved battery detection, by @jan-guenter

* Added: Displayed battery selectable in options menu

* Fixed: Battery error if non existent battery named is entered

## v1.0.16

* Fixed: atomic_wait() and atomic_lock{} use cpu pause instructions instead of thread sleep

* Fixed: Swapped from atomic bool spinlocks to mutexes to fix rare deadlock

* Added: Continuous Build workflow for OSX branch, by @ShrirajHegde

* Changed: Reverted thread mutex lock to atomic bool with wait and timeout

* Changed: Removed unnecessary async threads in Runner thread

* Added: Try to restart secondary thread in case of stall and additional error checks for ifstream in Proc::collect()

* Fixed: change [k]ill to [K]ill when enabling vim keys, by @jlopezcur

## v1.0.15

* Fixed: Extra "root" partition when running in snap

* Changed: Limit atomic_wait() to 1000ms to fix rare stall

* Fixed: Removed unneeded lock in Runner::run()

* Added: Toggle in options for enabling directional vim keys "h,j,k,l"

## v1.0.14

* Changed: Total system memory is checked at every update instead of once at start

* Added: Continuous Build workflow, by @ShrirajHegde

* Fixed: Uid -> User fallback to getpwuid() if failure for non static builds

* Fixed: snap root disk and changed to compiler flags instead of env variables for detection

* Added: Development branch for OSX, by @joske

## v1.0.13

* Changed: Graph empty symbol is now regular whitespace

## v1.0.12

* Fixed: Exception handling for faulty net download/upload speed

* Fixed: Cpu percent formatting if over 10'000

## v1.0.11

* Changed: atomic_wait to use while loop instead of wait() because of rare stall when a signal handler is triggered while waiting

* Fixed: Get real / mountpoint when running inside snap

* Fixed: UTF8 set LANG and LC_ALL to empty before UTF8 search and fixed empty error msg on exit before signal handler init

* Changed: Init will continue with a warning if UTF-8 locale are detected and it fails to set the locale

## v1.0.10

* Added: Wait for terminal size properties to be available at start

* Changed: Stop second thread before updating terminal size variables

* Changed: Moved check for valid terminal dimensions to before platform init

* Added: Check for empty percentage deques

* Changed: Cpu temp values check for existing values

* Fixed: Cpu percent cutting off above 1000 percent and added scaling with "k" prefix above 10'000

* Fixed: Crash when rapidly resizing terminal at start

## v1.0.9

* Added: ifstream check and try-catch for stod() in Tools::system_uptime()

* Fixed: Freeze on cin.ignore()

## v1.0.8

* Fixed: Additional NULL checks in UTF-8 detection

* Changed: Makefile: Only look for g++-11 if CXX=g++

* Fixed: Missing NULL check for ttyname

* Changed: Only log tty name if known

## v1.0.7

* Fixed: Crash when opening menu at too small size

* Fixed: Cores not constrained to cpu box and core numbers above 100 cut off

* Fixed: Scrollbar position incorrect in small lists and selection not working when filtering

## v1.0.6

* Fixed: Check that getenv("LANG") is not NULL in UTF-8 check

* Fixed: Processes not completely hidden when collapsed in tree mode

* Fixed: Changed wrong filename error.log to btop.log

## v1.0.5

* Fixed: Load AVG sizing when hiding temperatures

* Fixed: Sizing constraints bug on start and boxes can be toggled from size error screen

* Fixed: UTF-8 check crashing if LANG was set to non existant locale

## v1.0.4

* Fixed: Use /proc/pid/statm if RSS memory from /proc/pid/stat is faulty

## v1.0.3

* Fixed: stoi 0 literal pointer to nullptr and added more clamping for gradient array access

## v1.0.2

* Fixed: ARCH detection in Makefile

* Fixed: Color gradient array out of bounds, added clamp 0-100 for cpu percent values

* Fixed: Menu size and preset size issues and added warnings for small terminal size

* Fixed: Options menu page selection alignment

## v1.0.1

* Fixed: UTF-8 check to include UTF8

* Fixed: Added thread started check before joining in clean_quit()

* Fix documentation of --utf-force in README and --help. by @purinchu

## v1.0.0

* First release for Linux
