---
icon: lucide/arrow-down-to-line
---

# Installation

## :lucide-package-open: Using Package Managers

=== ":simple-alpinelinux: Alpine Linux"

	```sh
	apk add btop
	```

=== ":simple-archlinux: Arch Linux"

	```sh
	pacman -S btop
	```

=== ":simple-debian: Debian / :simple-ubuntu: Ubuntu"

	```sh
	apt install btop
	```

=== ":simple-fedora: Fedora"

	```sh
	dnf install btop
	```

=== ":simple-freebsd: FreeBSD"

	```sh
	pkg install btop
	```

=== ":simple-gentoo: Gentoo"

	```sh
	emerge -av sys-process/btop
	```

=== ":simple-homebrew: Homebrew"

	```sh
	brew install btop
	```

=== ":simple-macports: MacPorts"

	```sh
	port install btop
	```

=== ":simple-netbsd: NetBSD"

	```sh
	pkgin install btop
	```

=== "Spack"

	```sh
	spack install btop
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
