% btop(1) | User Commands
%
% 2025-05-01

# NAME

btop - Resource monitor that shows usage and stats for processor, memory, disks, network, and processes.

# SYNOPSIS

**btop** [**-c**] [**-d**] [**-l**] [**-t**] [**-p** _id_] [**-u** _ms_] [**\-\-force-utf**]

**btop** [{**-h** | **\-\-help**} | {**-V** | **\-\-version**}]

# DESCRIPTION

**btop** is a program that shows usage and stats for processor, memory, disks, network, and processes.

# OPTIONS

The program follows the usual GNU command line syntax, with long options
starting with two dashes ('-'). A summary of options is included below.

**-c**, **\-\-config _file_**
:   Path to a config file.

**-d**, **\-\-debug**
:   Start in debug mode with additional logs and metrics.

**\-\-force-utf**
:   Force start even if no UTF-8 locale was detected.

**-l**, **\-\-low-color**
:   Disable true color, 256 colors only.

**-p**, **\-\-preset _id_**
:   Start with a preset (0-9).

**-t**, **\-\-tty-on**
:   Force tty mode with ANSI graph symbols and 16 colors only.

**\-\-tty-off**
:   Force disable tty mode.

**-u**, **\-\-update _ms_**
:   Set an initial update rate in milliseconds.

**-h**, **\-\-help**
:   Show summary of options.

**-V**, **\-\-version**
:   Show version of program.

# BUGS

The upstream bug tracker can be found at https://github.com/aristocratos/btop/issues.

# SEE ALSO

**top**(1), **htop**(1)

# AUTHOR

**btop** was written by Jakob P. Liljenberg a.k.a. "Aristocratos".
