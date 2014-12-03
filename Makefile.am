ACLOCAL_AMFLAGS = -I m4 

libdir = $(prefix)/lib
lib_LTLIBRARIES = libmodule_lntopics.la

libmodule_lntopics_la_SOURCES  = module_lntopics.cpp
libmodule_lntopics_la_CXXFLAGS = -Wall -I$(top_builddir) @ROBOTKERNEL_CFLAGS@
libmodule_lntopics_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(srcdir)/share/ln/* $(prefix)/share/ln/

