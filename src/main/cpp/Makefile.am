
libdir = $(prefix)/lib
lib_LTLIBRARIES = libmodule_pdrouting.la

libmodule_pdrouting_la_SOURCES  = pdrouting.cpp
libmodule_pdrouting_la_CXXFLAGS = -Wall -I$(srcdir) -I$(builddir) \
                                  @ROBOTKERNEL_CFLAGS@ @LIBSTRING_UTIL_CFLAGS@ @YAML_CPP_CFLAGS@
libmodule_pdrouting_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_srcdir)/share/ln/* $(prefix)/share/ln/

