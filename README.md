# ![btop++](Img/logo.png)

![Linux](https://img.shields.io/badge/-Linux-grey?logo=linux)
![Usage](https://img.shields.io/badge/Usage-System%20resource%20monitor-yellow)
![c++20](https://img.shields.io/badge/cpp-c%2B%2B20-green)
![btop_version](https://img.shields.io/github/v/tag/aristocratos/btop?label=version)
[![Donate](https://img.shields.io/badge/-Donate-yellow?logo=paypal)](https://paypal.me/aristocratos)
[![Sponsor](https://img.shields.io/badge/-Sponsor-red?logo=github)](https://github.com/sponsors/aristocratos)
[![Coffee](https://img.shields.io/badge/-Buy%20me%20a%20Coffee-grey?logo=Ko-fi)](https://ko-fi.com/aristocratos)

## Index

* [News](#news)
* [Documents](#documents)
* [Description](#description)
* [Features](#features)
* [Themes](#themes)
* [Support and funding](#support-and-funding)
* [Prerequisites](#prerequisites) (Read this if you are having issues!)
* [Dependencies](#dependencies)
* [Screenshots](#screenshots)
* [Installation](#installation)
* [Configurability](#configurability)
* [License](#license)

## News

### Under development
##### 5 May 2021

This project is gonna take some time until it has complete feature parity with bpytop, since all system information gathering will have to be written from scratch without any external libraries.
And will need some help in the form of code contributions to get complete support for BSD and OSX.

If you got suggestions of C++ libraries that are multi-platform and are as extensive as [psutil](https://github.com/giampaolo/psutil) are for python, feel free to open up a new thread in Discussions, it could help speed up the development a lot.

## Documents

#### [CHANGELOG.md](CHANGELOG.md)

#### [CONTRIBUTING.md](CONTRIBUTING.md)

#### [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

## Description

Resource monitor that shows usage and stats for processor, memory, disks, network and processes.

C++ version and continuation of [bashtop](https://github.com/aristocratos/bashtop) and [bpytop](https://github.com/aristocratos/bpytop).

## Features (at release)

* Easy to use, with a game inspired menu system.
* Full mouse support, all buttons with a highlighted key is clickable and mouse scroll works in process list and menu boxes.
* Fast and responsive UI with UP, DOWN keys process selection.
* Function for showing detailed stats for selected process.
* Ability to filter processes, multiple filters can be entered.
* Easy switching between sorting options.
* Send SIGTERM, SIGKILL, SIGINT to selected process.
* UI menu for changing all config file options.
* Auto scaling graph for network usage.
* Shows message in menu if new version is available
* Shows current read and write speeds for disks

## Themes

Btop++ uses the same theme files as bpytop and bashtop (some color values missing in bashtop themes) .

See [themes](https://github.com/aristocratos/btop/tree/master/themes) folder for available themes.

The `make install` command places the default themes in `[/usr/local]/share/btop/themes`.
User created themes should be placed in `$XDG_CONFIG_HOME/btop/themes` or `$HOME/.config/btop/themes`.

Let me know if you want to contribute with new themes.

## Support and funding

You can sponsor this project through github, see [my sponsors page](https://github.com/sponsors/aristocratos) for options.

Or donate through [paypal](https://paypal.me/aristocratos) or [ko-fi](https://ko-fi.com/aristocratos).

Any support is greatly appreciated!

## Prerequisites

For correct display, a terminal with support for:

* 24-bit truecolor ([See list of terminals with truecolor support](https://gist.github.com/XVilka/8346728))
* 256-color terminals are supported through 24-bit to 256-color conversion when setting "truecolor" to False in the options or with "-lc/--low-color" argument.
* Wide characters (Are sometimes problematic in web-based terminals)

Also needs a UTF8 locale and a font that covers:

* Unicode Block “Braille Patterns” U+2800 - U+28FF
* Unicode Block “Geometric Shapes” U+25A0 - U+25FF
* Unicode Block "Box Drawing" and "Block Elements" U+2500 - U+259F

#### Notice (Text rendering issues)

If you are having problems with the characters in the graphs not looking like they do in the screenshots,
it's likely a problem with your systems configured fallback font not having support for braille characters.

See [Terminess Powerline](https://github.com/ryanoasis/nerd-fonts/tree/master/patched-fonts/Terminus/terminus-ttf-4.40.1) for an example of a font that includes the braille symbols.

See comments by @sgleizes [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-684036827) and @XenHat [link](https://github.com/aristocratos/bpytop/issues/100#issuecomment-691585587) in issue #100 for possible solutions.

If text are misaligned and you are using Konsole or Yakuake, turning off "Bi-Directional text rendering" is a possible fix.

Characters clipping in to each other or text/border misalignments is not bugs caused by bpytop, but most likely a fontconfig or terminal problem where the braille characters making up the graphs aren't rendered correctly.
Look to the creators of the terminal emulator you use to fix these issues if the previous mentioned fixes don't work for you.

## Dependencies

None

But will need G++ 11 if compiling from source.

## Screenshots

Main UI showing details for a selected process.
![Screenshot 1]()

Main UI in mini mode.
![Screenshot 2]()

Main menu.
![Screenshot 3]()

Options menu.
![Screenshot 4]()

## Installation

#### Manual compilation and installation

Requires GCC/G++ 11!

>Clone and compile

``` bash
git clone https://github.com/aristocratos/btop.git
cd btop
make
```

>to install

``` bash
sudo make install
```

>to uninstall

``` bash
sudo make uninstall
```

>to remove any compiled files

```bash
make clean
```

## Configurability

All options changeable from within UI.
Config and log files stored in `$XDG_CONFIG_HOME/btop` or `$HOME/.config/btop` folder

#### btop.cfg: (auto generated if not found)

```bash
#? Config file for btop v. 0.0.1


```

#### Command line options:

```text
usage: btop [-v]
```

## LICENSE

[Apache License 2.0](LICENSE)
