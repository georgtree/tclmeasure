# Content

- [Emulation of measure command in SPICE](https://georgtree.github.io/tclmeasure/tclmeasure-tclmeasure.html)

# Installation and dependencies

For building you need:
- [Tcl9](https://www.tcl.tk/software/tcltk/9.0.html)
- [gcc compiler](https://gcc.gnu.org/)
- [make tool](https://www.gnu.org/software/make/)

For run you also need:
- [argparse](https://github.com/georgtree/argparse)
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


## Optional direct RBC vector input

The default build supports Tcl lists and does not need RBC headers or libraries. To enable direct real-vector
input from [rbc-tk9](https://github.com/georgtree/rbc-tk9), build with Tcl 9 and the current RBC 0.8.0 API:

```bash
./configure --with-tcl=/path/to/tcl/lib --with-rbc=/path/to/rbc-tk9
make
```

`--with-rbc` accepts an RBC source tree, an installation prefix, or a directory containing `rbcVector.h`,
`rbcDecls.h` and `rbcStubLib.c`. The supplied stub client source is compiled into tclmeasure; the RBC shared
library is not linked into tclmeasure. Use matching RBC headers and runtime. `--without-rbc` explicitly selects
the default list-only build. Run `make clean` when changing this option in an existing build directory.

Even an RBC-enabled build can load and process lists without RBC installed. The `rbc::vector` package is loaded
only when vector input is requested. Tk initialization is not required for vector measurements; the installed
RBC binary must still have its own system library dependencies available.

With `-data`, measurement inputs are lists looked up in that dictionary, as before. Omit `-data` to interpret
`-xname`, `-vec`, `-vec1`, `-vec2`, `-find` and `-deriv` as RBC vector command names:

```tcl
package require tclmeasure
package require rbc::vector
rbc::vector create time signal
time set {0 1 2 3}
signal set {0 2 4 6}
::tclmeasure::measure -xname time -trig {-at 0.5} -targ {-vec signal -val 3 -rise 1}
# -> xtrig 0.5 xtarg 1.5 xdelta 1.0
```

All measurement modes accept vectors. Names are resolved in the namespace of the caller of `measure`, with
normal Tcl command lookup; fully qualified names also work. Use the vector's actual command name, not a Tcl
alias or an imported command with a different name. Mapped array variables are not needed. Only real vectors
are supported, with equal sample counts and strictly increasing X values. External vector index offsets do
not affect sample alignment. Supply at least two samples, or three for derivative measurements.

The wrapper passes names directly to C; it does not extract the full vectors as lists. A shared read-only
accessor serves both input types. RMS squares samples while reading, before interpolation, matching the list
implementation without creating a temporary vector. Inputs are not modified. Returned values retain the
existing scalar, list and dictionary forms, including `-between`, `-integ -cum` and `-cross all`; no result
vectors are created. Existing interpolation and measurement semantics are unchanged.

The internal C commands retain their original list signatures. Their `Vectors` counterparts use the same
arguments with vector names in the data positions; unused `FindDerivWhenVectors` inputs remain empty strings.
`RmsVectors` uses the `IntegVectors` signature with the final cumulative flag set to false. These commands
initialize stubs in the current interpreter before borrowing sample storage and do not evaluate Tcl or process
events while reading it.

The test suite retains the list cases and repeats them with vector names and the same expected results when
`rbc::vector` and direct vector support are available. Run it against both build configurations.

# Supported platforms

Any OS that has tcl9.0 (Linux, Windows, FreeBSD).

# Documentation

You can find some documentation [here](https://georgtree.github.io/tclmeasure).
Also on Linux you can open manpages installed after run of `make install`.

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
