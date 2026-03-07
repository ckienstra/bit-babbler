# Bit-Babbler (Modified)

This is a modified distribution of the **Bit-Babbler** package, originally developed by Ron Lee. Bit-Babbler provides tools and daemons for managing and utilizing hardware random number generators (TRNGs).

This repository is a fork of the `0.9` source release downloaded from the official Bit-Babbler website: <http://www.bitbabbler.org/what.html>.

---

## Modifications

- A compatibility fix to resolve errors when building the package with `gcc-14` and `glibc-2.41`.
- The `GPLv2` license file has been added due to the language in [debian/copyright](debian/copyright) and the headers of source files.
- Various workspace files for my personal convenience have been added.

---

## Installation

### Dependencies

Original instructions live in [doc/](doc/).

I used the following packages to build it on my Debian Trixie machine:
- `build-essential`
- `pkg-config`
- `libusb-1.0-0-dev`
- `libudev-dev`
- `autoconf`
- `automake`
- `bison`
- `flex`
- `gettext`
- `git`
- `debhelper`
- `fakeroot`
- `libjson-xs-perl`

There are additional packages for optional integration dependencies, but I have not tried them:
- `munin-node`: Used for the included Munin plugins.
- `libvirt-clients`: For hotplug support in virtual machines.
- `acl`: For managing device permissions.

### Build

Enter the repository directory, then:

```bash
./configure && make && make install
```
or, for Debian machines:
```bash
dpkg-buildpackage && dpkg --install ../bit-babbler_0.9_amd64.deb
```

### Check

Once your BitBabbler device is plugged in, you can ensure it works by using the `bbcheck` utility:
```bash
bbcheck --all-results --verbose --bytes=100
```
> If you get a `Resource busy` error, you can stop the `seedd` service and try again:
> `systemctl stop seedd.service`

### Services

The installation will install the `seedd.service` systemd file.

To start the service and check its status, run:
```bash
systemctl daemon-reload
systemctl enable seedd.service
systemctl start seedd.service
systemctl status seedd.service
```

At this point, you should be all set to use your BitBabbler. More interesting details about the `seedd` service can be found on the [original author's blog](http://bitbabbler.org/blog.html#:~:text=Two%20households,%20both%20alike%20in%20dignity). After the seedd daemon is running, you can also compare the quality of the BitBabbler's entropy against your other sources with the `bbctl` utility:

```bash
bbctl --stats
```

---

## Development and IDE Integration

For the best development experience with IntelliSense in VS Code, it is recommended to generate a `compile_commands.json` file. This file provides the editor with compiler commands used to build the project.

This project is configured to use the C/C++ Extension for VS Code, which will automatically use this file if it exists.

### Prerequisites

-   [bear](https://github.com/rizsotto/bear): A tool to generate compilation databases for `make`-based build systems.
-   On Debian Trixie, this should be as easy as `sudo apt install bear`

### Generating the Compilation Database

To generate the `compile_commands.json` file, run the following commands:

```bash
# Ensure the project is clean from the root directory
make clean

# Enter the ./build directory whose output is excluded from git using .gitignore
cd ./build

# Run the configure script if it hasn't been run
../configure

# Use bear to wrap the build process
bear -- make
```

A compilation database will be created at `build/compile_commands.json`. It's a good idea to open the file and ensure it's populated. One fuzzy way to check if IntelliSense is reading the file propertly is to open `c_cpp_properties.json` in VSCode and ensure it shows no erroneous squiggles on the `compileCommands` attribute.

---

## Acknowledgements & Licensing

This project is licensed under the **GNU General Public License v2 (GPLv2)**. The complete text of the license can be found in the [LICENSE](LICENSE) file in the root of this repository.

**Original Copyright:** Copyright (C) 2003 - 2021, Ron Lee

---

## Third-Party Code

The implementation of `poz()` and `pochisq()` in [include/bit-babbler/chisq.h](include/bit-babbler/chisq.h) is released into the public domain, based heavily on public domain code from John "Random" Walker's ENT test suite and Gary Perlman of the Wang Institute.
