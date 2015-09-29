ACLOCAL_AMFLAGS = -I m4 -I scripts/m4

libdir = $(prefix)/lib
lib_LTLIBRARIES = libmodule_pdrouting.la

libmodule_pdrouting_la_SOURCES  = pdrouting.cpp
libmodule_pdrouting_la_CXXFLAGS = -Wall -I$(top_builddir) @ROBOTKERNEL_CFLAGS@
libmodule_pdrouting_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(srcdir)/share/ln/* $(prefix)/share/ln/

