# Overview

SPICE-style measurements of sampled data, with Tcl list and optional RBC vector input and output.

[![Tests: Linux, Windows UCRT64 and MSVC](https://img.shields.io/github/actions/workflow/status/georgtree/tclmeasure/tests.yml?branch=main&label=Tests%3A%20Linux%2C%20Windows%20UCRT64%20and%20MSVC)](https://github.com/georgtree/tclmeasure/actions/workflows/tests.yml)

- [User manual and command reference](https://georgtree.github.io/tclmeasure/tclmeasure-tclmeasure.html)

# Installation and dependencies

For building you need:
- [Tcl9](https://www.tcl.tk/software/tcltk/9.0.html)
- [gcc compiler](https://gcc.gnu.org/)
- [make tool](https://www.gnu.org/software/make/)

For run you also need:
- [argparse](https://github.com/georgtree/argparse) 0.70 or newer
- [Tcllib](https://www.tcl.tk/software/tcllib/)

To build run 
```bash
./configure
make
sudo make install
```
If you have different versions of Tcl on the same machine, you can set the path to this version with `-with-tcl=path`
flag to configure script.

For Windows build it is strongly recommended to use [MSYS64 UCRT64 environment](https://www.msys2.org/), the above
steps are identical if you run it from UCRT64 shell.

There are prebuilt packages that contains .so/.dll files, tcl code and tests for Windows and Linux.

## Continuous integration and binary packages

Pushes build and test tclmeasure with direct RBC vector support on Linux GCC, Windows UCRT64 and Windows MSVC (x64),
using Tcl 9.1.0. Each workflow resolves the `latest` release of both
[RBC](https://github.com/georgtree/rbc-tk9/releases/tag/latest) and
[argparse](https://github.com/georgtree/argparse/releases/tag/latest) once for all three jobs. Each runner uses the
native RBC archive and the standalone argparse archive for its toolchain. The cache tracks the tag target, asset ID and
SHA-256 for each archive separately. Each download is verified before use.

Run **Actions → Build packages → Run workflow** to publish all three tested archives and their checksums to the [rolling
release](https://github.com/georgtree/tclmeasure/releases/tag/latest). Tcl, RBC, argparse and Tcllib are supplied
separately.

For a local MSVC build, use an **x64 Native Tools Command Prompt for Visual Studio 2022**, a matching Tcl 9.1
installation with headers and stub library, and the extracted native MSVC RBC distribution. The repository bundles the
Tcl Nmake rules and helper under `win/`, so Tcl installations without `lib/nmake` are supported.

```bat
cd win
set "INSTALLDIR=C:\Tcl91"
set "RBC_DIR=C:\deps\rbc"
set "PATH=%INSTALLDIR%\bin;%PATH%"
nmake /f makefile.vc
```

To run the tests with separately extracted dependencies, set `TCLLIBPATH` to a Tcl list of their directories.
For example, with argparse 0.70, RBC 0.8.5, and a Tcllib source checkout:

```bat
set "TCLLIBPATH={C:/deps/argparse/lib/argparse0.70} {C:/deps/rbc/lib/rbc0.8.5} {C:/deps/tcllib/modules}"
nmake /f makefile.vc test
```

The test target adds the build directory to the package search path. Use `nmake /f makefile.vc install` to install
the extension into the Tcl prefix, or `nmake /f makefile.vc dist-zip` to produce a distribution.

## Optional RBC vector support

The default build supports Tcl lists and does not need RBC headers or libraries. To enable direct real-vector
input and vector output from [rbc-tk9](https://github.com/georgtree/rbc-tk9), build with Tcl 9 and the current RBC 0.8.1 API:

```bash
./configure --with-tcl=/path/to/tcl/lib --with-rbc=/path/to/rbc-tk9
make
```

`--with-rbc` accepts an RBC source tree, an installation prefix, or a directory containing `rbcVector.h`,
`rbcDecls.h` and `rbcStubLib.c`. The supplied stub client source is compiled into tclmeasure; the RBC shared
library is not linked into tclmeasure. Use matching RBC headers and runtime. `--without-rbc` explicitly selects
the default list-only build. Run `make clean` when changing this option in an existing build directory.

For vector input, output naming and package defaults, see the
[user manual](https://georgtree.github.io/tclmeasure/tclmeasure-tclmeasure.html).

# Supported platforms

Any OS that has tcl9.0 (Linux, Windows, FreeBSD).

# Documentation

You can find some documentation [here](https://georgtree.github.io/tclmeasure). Also on Linux you can open manpages
installed after run of `make install`.

## Installation layout and removal

Installation follows the rbc-tk9 layout and honors the directories selected by `configure`:

- The package library, Tcl scripts and `pkgIndex.tcl` go together in `$(libdir)/$(PACKAGE_NAME)$(PACKAGE_VERSION)`.
- Any public headers and stub client sources go in `$(includedir)`; executable binaries go in `$(bindir)`.
- Manpages go in `$(mandir)/mann`.
- HTML documentation, its image/static resources, and `LICENSE` go in `$(datadir)/$(PACKAGE_NAME)$(PACKAGE_VERSION)/doc`.

Use `--prefix`, `--libdir`, `--includedir`, `--datadir` and `--mandir` at configure time to change these locations.
All install and uninstall targets honor `DESTDIR` for staging:

```sh
./configure --prefix=/your/prefix
make
make install DESTDIR=/your/staging/root
make uninstall DESTDIR=/your/staging/root
```

`make uninstall` runs `uninstall-binaries`, `uninstall-libraries` and `uninstall-doc`. It removes the package-owned
runtime directory, but removes only this package's named files from shared binary, include and documentation paths.
Unrelated documentation files are retained; empty documentation directories are removed. Keep the configured build
and source tree to uninstall the corresponding installation, and use the same path overrides for install and uninstall.

`DOC_INSTALL_DIR` can override the complete HTML destination. As in rbc-tk9, its default already includes `DESTDIR`;
when overriding it explicitly, include the staging root yourself if needed.

## Installation archives

`make dist` follows rbc-tk9: it builds the package, stages `make install` under the build directory, and creates
`dist/$(PACKAGE_NAME)$(PACKAGE_VERSION).tar.gz`. `make dist-zip` creates the same payload as a ZIP file as well.
`make dist-clean` removes this package's staging directory and both archives.

```sh
make -j4 dist
make dist-zip
```

These are platform-specific installation archives. Their contents are relative to the configured installation prefix:
normally `lib/`, `include/`, and `share/`, without an enclosing package directory.  Extract or merge the archive
contents into the desired prefix. The payload uses the same install targets as normal installation, including the built
library, Tcl runtime files, public headers, manpages, HTML resources and license.  Tests, build files and examples that
are not installed by `make install` are not included.

Configured installation directories must be below `prefix`; `dist` reports an error if one is outside it.
When using `--with-tcl` and a custom prefix, set `--exec-prefix` to the same prefix if TEA would otherwise inherit
Tcl's execution prefix, for example `sh ./configure --prefix=/opt/mypackages --exec-prefix=/opt/mypackages`. Custom
subdirectories inside that prefix are preserved. `DESTDIR` is not included in archive paths. The staging install does
not write to the configured system prefix. `DIST_ROOT` and `DIST_NAME` may be overridden to choose the archive output
directory and name; `DIST_NAME` must be a single directory name. Run `dist-clean` separately, not alongside `dist`
or `dist-zip` in the same parallel make invocation.

## Testing

The test suite retains the list cases and repeats them with vector names and the same expected results when
`rbc::vector` and direct vector support are available. Run it against both build configurations.

### Memory regression tests

Run `make test` with an ordinary Tcl build for the functional tests. The `test/whenMemory.test` tests call the native
measurement command directly to check result lifetime and error handling independently of argparse. Live allocation
checks additionally require Tcl built with `--enable-symbols=mem`; these checks are automatically skipped on ordinary
Tcl builds. They cover all six WHEN/FIND/DERIV modes and early, partial-result and no-match errors.
