#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.


AC_INIT([tclmeasure],[0.5])
TEA_INIT()
AC_CONFIG_AUX_DIR(tclconfig)
TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG
TEA_PREFIX
TEA_SETUP_COMPILER
TEA_ADD_SOURCES([tclmeasure.c])
TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([])
TEA_ADD_LIBS([])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([tclmeasure.tcl])
TEA_PUBLIC_TCL_HEADERS
TEA_ENABLE_THREADS
TEA_ENABLE_SHARED
TEA_CONFIG_CFLAGS
TEA_ENABLE_SYMBOLS
TEA_MAKE_LIB
TEA_PROG_TCLSH
AC_SUBST(TCLLIBPATH)
AC_CONFIG_FILES([Makefile pkgIndex.tcl])
AC_OUTPUT
