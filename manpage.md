% btop(1) | User Commands
%
% "January  4 2024"

# NAME

btop - Resource monitor that shows usage and stats for processor, memory, disks, network, and processes.

# SYNOPSIS

**btop** [**-lc**] [**-t** | **+t**] [**-p** _id_] [**\-\-utf-force**]
         [**\-\-debug**] [{**-h** | **\-\-help**} | {**-v** | **\-\-version**}]

# DESCRIPTION

**btop** is a program that shows usage and stats for processor, memory, disks, network, and processes.

# OPTIONS

The program follows the usual GNU command line syntax, with long options
starting with two dashes ('-'). A summary of options is included below.

**-lc**, **\-\-low-color**
:   Disable truecolor, converts 24-bit colors to 256-color.

**-t**, **\-\-tty_on**
:   Force (ON) tty mode, max 16 colors and tty-friendly graph symbols.

**+t**, **\-\-tty_off**
:   Force (OFF) tty mode.

**-p**, **\-\-preset _id_**
:   Start with preset, integer value between 0-9.

**\-\-utf-force**
:   Force start even if no UTF-8 locale was detected.

**\-\-debug**
:   Start in DEBUG mode: shows microsecond timer for information collect and screen draw functions and sets loglevel to DEBUG.

**-h**, **\-\-help**
:   Show summary of options.

**-v**, **\-\-version**
:   Show version of program.

# BUGS

The upstream bug tracker can be found at https://github.com/aristocratos/btop/issues.

# SEE ALSO

**top**(1), **htop**(1)

# AUTHOR

**btop** was written by Jakob P. Liljenberg a.k.a. "Aristocratos".
