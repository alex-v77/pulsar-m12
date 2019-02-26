#
# Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
# Rok Papez
# Hribovska pot 17
# 1231 Ljubljana - Crnuce
# EUROPE, Slovenia
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
SUB_DIRS = src man support_files doc
AUTOCONF_FILES = config.cache config.log config.status configure autom4te.cache

.PHONY: all all-recursive install clean distclean \
  install-recursive clean-recursive clean-non-recursive \
  distclean-recursive

all: ./src/Makefile all-recursive

configure: configure.in
	autoconf

./src/Makefile: configure
	./configure --enable-debug
rpm:
	@set -e; cd ./support_files; $(MAKE) rpm ; cd ..

install: \
  all    \
  install-recursive

clean:                \
  ./src/Makefile      \
  clean-non-recursive \
  clean-recursive

distclean:            \
  ./src/Makefile      \
  clean-non-recursive \
  distclean-recursive
	@rm -rf $(AUTOCONF_FILES)

all-recursive:
	@set -e; for i in $(SUB_DIRS); do cd $$i; $(MAKE) ; cd .. ; done

install-recursive:
	@set -e; for i in $(SUB_DIRS); do cd $$i; $(MAKE) install ; cd .. ; done

clean-recursive:
	@set -e; for i in $(SUB_DIRS); do cd $$i; $(MAKE) clean ; cd .. ; done

clean-non-recursive:
	@rm -f *~

distclean-recursive:
	@set -e; for i in $(SUB_DIRS); do cd $$i; $(MAKE) distclean ; cd .. ; done
