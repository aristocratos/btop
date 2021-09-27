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
