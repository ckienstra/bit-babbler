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

At this point, you should be all set to use your BitBabbler. More interesting details about the `seedd` service can be found on the [original author's blog](http://bitbabbler.org/blog.html#:~:text=Two%20households,%20both%20alike%20in%20dignity).

---

## Acknowledgements & Licensing

This project is licensed under the **GNU General Public License v2 (GPLv2)**. The complete text of the license can be found in the [LICENSE](LICENSE) file in the root of this repository.

**Original Copyright:** Copyright (C) 2003 - 2021, Ron Lee

---

## Third-Party Code

The implementation of `poz()` and `pochisq()` in [include/bit-babbler/chisq.h](include/bit-babbler/chisq.h) is released into the public domain, based heavily on public domain code from John "Random" Walker's ENT test suite and Gary Perlman of the Wang Institute.
