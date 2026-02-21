# Bit-Babbler (Modified)

This is a modified distribution of the **Bit-Babbler** package, originally developed by Ron Lee. Bit-Babbler provides tools and daemons for managing and utilizing hardware random number generators (TRNGs).

This repository is a fork of the `0.9` source release downloaded from the official Bit-Babbler website: http://www.bitbabbler.org/what.html.

## Modifications

There are currently only minor modifications:

- A compatibility fix to resolve errors when building the package with gcc-14 and glibc-2.41.
- The GPLv2 license file has been added due to the language in debian/copyright and the headers of source files.
- Various workspace files for my personal convenience have been added.

## Acknowledgements & Licensing

This project is licensed under the GNU General Public License v2 (GPLv2). The complete text of the license can be found in the LICENSE file in the root of this repository.

Original Copyright: Copyright (C) 2003 - 2021, Ron Lee

## Third-Party Code

The implementation of poz() and pochisq() in include/bit-babbler/chisq.h is released into the public domain, based heavily on public domain code from John "Random" Walker's ENT test suite and Gary Perlman of the Wang Institute.
