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
