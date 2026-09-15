---
icon: lucide/arrow-down-to-line
---

# Installation

## :lucide-package-open: Using Package Managers

### :simple-alpinelinux: Alpine Linux

[![Alpine Linux 3.23 package](https://repology.org/badge/version-for-repo/alpine_3_23/btop.svg)](https://repology.org/project/btop/versions)
[![Alpine Linux Edge package](https://repology.org/badge/version-for-repo/alpine_edge/btop.svg)](https://repology.org/project/btop/versions)

```sh
apk add btop
```

### :simple-archlinux: Arch Linux

[![Arch Linux package](https://repology.org/badge/version-for-repo/arch/btop.svg)](https://repology.org/project/btop/versions)

```sh
pacman -S btop
```

### :simple-debian: Debian / :simple-ubuntu: Ubuntu

[![Debian 13 package](https://repology.org/badge/version-for-repo/debian_13/btop.svg)](https://repology.org/project/btop/versions)
[![Debian 14 package](https://repology.org/badge/version-for-repo/debian_14/btop.svg)](https://repology.org/project/btop/versions)
[![Ubuntu 26.04 package](https://repology.org/badge/version-for-repo/ubuntu_26_04/btop.svg)](https://repology.org/project/btop/versions)

```sh
apt install btop
```

### :simple-fedora: Fedora / :simple-redhat: EPEL

[![EPEL 10 package](https://repology.org/badge/version-for-repo/epel_10/btop.svg)](https://repology.org/project/btop/versions)
[![Fedora 43 package](https://repology.org/badge/version-for-repo/fedora_43/btop.svg)](https://repology.org/project/btop/versions)
[![Fedora Rawhide package](https://repology.org/badge/version-for-repo/fedora_rawhide/btop.svg)](https://repology.org/project/btop/versions)

```sh
dnf install btop
```

### :simple-freebsd: FreeBSD

[![FreeBSD port](https://repology.org/badge/version-for-repo/freebsd/btop.svg)](https://repology.org/project/btop/versions)

```sh
pkg install btop
```

### :simple-gentoo: Gentoo

[![Gentoo package](https://repology.org/badge/version-for-repo/gentoo/btop.svg)](https://repology.org/project/btop/versions)

```sh
emerge -av sys-process/btop
```

### :simple-homebrew: Homebrew

[![Homebrew package](https://repology.org/badge/version-for-repo/homebrew/btop.svg)](https://repology.org/project/btop/versions)

```sh
brew install btop
```

### :simple-macports: MacPorts

[![MacPorts package](https://repology.org/badge/version-for-repo/macports/btop.svg)](https://repology.org/project/btop/versions)

```sh
port install btop
```

### :simple-netbsd: NetBSD

[![pkgsrc-2026Q1 package](https://repology.org/badge/version-for-repo/pkgsrc_2026_1/btop.svg)](https://repology.org/project/btop/versions)
[![pkgsrc current package](https://repology.org/badge/version-for-repo/pkgsrc_current/btop.svg)](https://repology.org/project/btop/versions)

```sh
pkgin install btop
```

### :simple-opensuse: openSUSE

[![openSUSE Leap 15.6 package](https://repology.org/badge/version-for-repo/opensuse_leap_15_6/btop.svg)](https://repology.org/project/btop/versions)
[![openSUSE Tumbleweed package](https://repology.org/badge/version-for-repo/opensuse_tumbleweed/btop.svg)](https://repology.org/project/btop/versions)

```sh
zypper install btop
```

### :lucide-package-2: Spack

[![Spack package](https://repology.org/badge/version-for-repo/spack/btop.svg)](https://repology.org/project/btop/versions)

```sh
spack install btop
```

### :simple-voidlinux: Void Linux

[![Void Linux x86_64 package](https://repology.org/badge/version-for-repo/void_x86_64/btop.svg)](https://repology.org/project/btop/versions)

```sh
xbps-install -Su btop
```

## :simple-git: From Releases

## :lucide-code: Building From Source

=== ":simple-cmake: CMake"

    This will install `btop` in `~/.local/bin/btop`:

	```shell
	cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build
	cmake --install build --prefix ~/.local
	```

=== ":simple-gnu: Make"

    ```shell
    make PREFIX=~/.local btop install
    ```

    !!! tip

        To see all available `make` commands, type

		```shell
		make help
		```
