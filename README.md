# Content

[![Tests: Linux, Windows UCRT64 and MSVC](https://img.shields.io/github/actions/workflow/status/georgtree/tclmeasure/tests.yml?branch=main&label=Tests%3A%20Linux%2C%20Windows%20UCRT64%20and%20MSVC)](https://github.com/georgtree/tclmeasure/actions/workflows/tests.yml)

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

Read-only RBC vectors are accepted as inputs. Output replacement requires writable real vectors;
tclmeasure never unlocks a vector. Use RBC 0.8.1 or newer headers and runtime.

Even an RBC-enabled build can load and process lists without RBC installed. The `rbc::vector` package is loaded
only when vector input or output is requested (including configuring a vector output default). Tk initialization is not required for vector measurements; the installed
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
implementation without creating a temporary vector. Returned values retain their existing forms by default.
`-between` and cumulative integration additionally support vector output; `-cross all` and scalar measurements
retain their existing result types. Interpolation and measurement semantics are unchanged.

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

You can find some documentation [here](https://georgtree.github.io/tclmeasure). Also on Linux you can open manpages
installed after run of `make install`.

## Vector output and package defaults

`-between` and `-integ {... -cum}` accept the outer option `-output vector`. Their result remains a dictionary
with `x` and `y` keys, but each value is a fully qualified real RBC vector name. Both list and vector inputs
support either output representation. Other measurement results are unchanged by `-output`, including a global
vector default.

```tcl
set data [dict create time {0 1 2 3} current {0 2 4 6}]
set result [::tclmeasure::measure -data $data -xname time \
    -between {-vec current -from 0.5 -to 2.5} -output vector \
    -namespace ::results -names {x clippedTime y #auto}]
[dict get $result y] range 0 end
# -> 1.0 2.0 4.0 5.0
rbc::vector destroy {*}[dict values $result]

set saved [::tclmeasure::configure]
::tclmeasure::configure -output vector -ifexists replace
set integral [::tclmeasure::measure -data $data -xname time \
    -integ {-vec current -cum} -namespace ::results -names {x integralTime y integral}]
::tclmeasure::configure {*}$saved
rbc::vector destroy {*}[dict values $integral]
```

`configure` is public but not namespace-exported. With no arguments it returns
`-output list -ifexists error` initially; with one option it returns that option's value. Updates are atomic,
interpreter-local and support unambiguous option prefixes and `-help`. Per-call options override defaults.
There is no `-input` setting: supplying `-data` continues to select list input; omitting it selects vector input.

| Option | Meaning |
| --- | --- |
| `-output list|vector` | Representation of the two supported x/y results. |
| `-names dictionary` | Destination names for `x` and/or `y`; omitted entries use `#auto`. |
| `-name name` | Shorthand for the `y` destination; cannot also specify `-names {y ...}`. |
| `-namespace name` | Destination namespace, relative to the caller; missing parents are created. |
| `-ifexists error|replace` | Reject collisions, or update existing writable real vectors in place. |

Without `-namespace`, relative destinations use the caller's namespace. Fully qualified names override the
selected namespace; their parent namespace must exist. `#auto` (also `namespace::#auto`) uses RBC's allocator;
`auto` is an ordinary name. Explicit names are literal, including parentheses. Empty names/namespaces,
unknown fields and duplicate destinations are rejected. Naming options require an eligible vector result.
No mapped array variable is created. The caller owns the vectors and must destroy them when no longer needed.

Replacement retains existing vector commands and graph clients. Complex or read-only vectors, aliases and
unrelated commands cannot be replaced. Both results are computed and copied before publication, making reuse
of input vectors as destinations safe. New vectors are removed on publication failure; already replaced
existing vectors are not rolled back if an application notification callback causes a later failure.
The current numerical kernels still produce their usual result lists internally; C publishes numeric copies
through RBC's API without Tcl loops over individual samples.

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

## Memory regression tests

Run `make test` with an ordinary Tcl build for the functional tests. The `test/whenMemory.test` tests call the native
measurement command directly to check result lifetime and error handling independently of argparse. Live allocation
checks additionally require Tcl built with `--enable-symbols=mem`; these checks are automatically skipped on ordinary
Tcl builds. They cover all six WHEN/FIND/DERIV modes and early, partial-result and no-match errors.

## Interactive measurement help

Use `measure -help` for general options. Add measurement selectors to see the options for that mode:

```tcl
measure -avg -help
measure -trig -targ -help
measure -find -when -help
measure -deriv -at -help
measure -integ -help
```

Mode help requires no `-xname`, data dictionary, or RBC package. It accepts selectors in either order and unambiguous
prefixes, such as `measure -tr -ta -help`. `-h` also requests help.  Use selector-only calls for contextual help; calls
containing actual option values retain general help.  Incompatible selector combinations are errors. Without `-help`,
bare selectors still require their normal values.
