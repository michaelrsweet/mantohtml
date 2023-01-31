mantohtml - Man Page to HTML Converter
======================================

![Version](https://img.shields.io/github/v/release/michaelrsweet/mantohtml?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/mantohtml)
![Build](https://github.com/michaelrsweet/mantohtml/workflows/Build/badge.svg)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/NNNNN.svg)](https://scan.coverity.com/projects/michaelrsweet-mantohtml)

mantohtml is an all-new implementation of a man page to HTML conversion program.
It supports all of the common man/roff macros and can combine multiple man pages
in a single HTML output file.


Requirements
------------

mantohtml requires a C99 compiler such as GCC or Clang and a POSIX-compliant
"make" utility like GNU make.


Building and Installing
-----------------------

Run `make` to build the software.

Run `sudo make install` to install the software to "/usr/local".  To install
to a different location, set the "prefix" variable, e.g.:

    sudo make install prefix=/some/other/directory


Documentation and Examples
--------------------------

Documentation can be found in the "mantohtml.1" file.  Normally you just run the
program with the name of a man file and the HTML output is sent to the standard
output:

    mantohtml mantohtml.1 >mantohtml.html

The `--help` option lists the available options:

    mantohtml --help


Legal Stuff
-----------

mantohtml is Copyright © 2022-2023 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0.  See the files
"LICENSE" and "NOTICE" for more information.
