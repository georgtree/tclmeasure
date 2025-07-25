# Content

- [Emulation of measure command in SPICE](https://georgtree.github.io/tclmeasure/index-tclmeasure.html)

# Installation and dependencies

For building you need:
- [Tcl9](https://www.tcl.tk/software/tcltk/9.0.html) or [Tcl8.6.15](https://www.tcl.tk/software/tcltk/8.6.html)
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

# Supported platforms

Any OS that has tcl8.6/tcl9.0 (Linux, Windows, FreeBSD).

# Documentation

You can find some documentation [here](https://georgtree.github.io/tclmeasure).
Also on Linux you can open manpages installed after run of `make install`.
