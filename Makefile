#
# Makefile for man page to HTML conversion program.
#
# Copyright © 2022 by Michael R Sweet.
#
# Licensed under Apache License v2.0.
# <https://opensource.org/licenses/Apache-2.0>
#

VERSION	=	2022.1005
prefix	=	$(DESTDIR)/usr/local
bindir	=	$(prefix)/bin
mandir	=	$(prefix)/share/man

CC	=	gcc
CFLAGS	=	$(OPTIM) $(CPPFLAGS) -Wall
CPPFLAGS =	'-DVERSION="$(VERSION)"'
LDFLAGS	=	$(OPTIM)
LIBS	=
OBJS	=	mantohtml.o
OPTIM	=	-Os -g
TARGETS	=	mantohtml mantohtml.html

.SUFFIXES:	.c .o
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<


all:	$(TARGETS)


clean:
	rm -f $(TARGETS) $(OBJS)


install:	$(TARGETS)
	mkdir -p $(bindir)
	cp mantohtml $(bindir)
	mkdir -p $(mandir)/man1
	cp mantohtml.1 $(mandir)/man1


sanitizer:
	$(MAKE) clean
	$(MAKE) OPTIM="-g -fsanitize=address" all


# Analyze code with the Clang static analyzer <https://clang-analyzer.llvm.org>
clang:
	clang $(CPPFLAGS) --analyze $(OBJS:.o=.c) 2>clang.log
	rm -rf $(OBJS:.o=.plist)
	test -s clang.log && (echo "$(GHA_ERROR)Clang detected issues."; echo ""; cat clang.log; exit 1) || exit 0


# Analyze code using Cppcheck <http://cppcheck.sourceforge.net>
cppcheck:
	cppcheck $(CPPFLAGS) --template=gcc --addon=cert.py --suppressions-list=.cppcheck $(OBJS:.o=.c) 2>cppcheck.log
	test -s cppcheck.log && (echo "$(GHA_ERROR)Cppcheck detected issues."; echo ""; cat cppcheck.log; exit 1) || exit 0


# Make various bits...
mantohtml:	mantohtml.o
	$(CC) $(LDFLAGS) -o $@ mantohtml.o $(LIBS)

$(OBJS):	Makefile

mantohtml.html:	mantohtml.1 mantohtml
	./mantohtml --author "Michael R Sweet" --copyright "Copyright © 2022 by Michael R Sweet" --title "mantohtml Documentation" mantohtml.1 >mantohtml.html
