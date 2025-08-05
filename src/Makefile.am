lib_LTLIBRARIES = libmodule_pdrouting.la

libmodule_pdrouting_la_SOURCES  = pdrouting.cpp
libmodule_pdrouting_la_CXXFLAGS = -Wall -fno-strict-aliasing -I$(srcdir) -I$(builddir) \
                                  @ROBOTKERNEL_CFLAGS@ @SERVICE_PROVIDER_PROCESS_DATA_INSPECTION_CFLAGS@
libmodule_pdrouting_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)


