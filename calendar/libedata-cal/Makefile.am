NULL =

lib_LTLIBRARIES = libedata-cal-1.2.la

libedata_cal_1_2_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DLIBEDATA_CAL_COMPILATION \
	-DG_LOG_DOMAIN=\"libedata-cal\" \
	-DBACKENDDIR=\"$(ecal_backenddir)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/calendar \
	-I$(top_srcdir)/calendar/libegdbus \
	-I$(top_srcdir)/private \
	-I$(top_builddir) \
	-I$(top_builddir)/calendar \
	-I$(top_builddir)/private \
	$(EVOLUTION_CALENDAR_CFLAGS) \
	$(CAMEL_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(NULL)

libedata_cal_1_2_la_SOURCES = \
	e-cal-backend.c \
	e-cal-backend-cache.c \
	e-cal-backend-factory.c \
	e-cal-backend-intervaltree.c \
	e-cal-backend-sexp.c \
	e-cal-backend-sync.c \
	e-cal-backend-util.c \
	e-cal-backend-store.c \
	e-data-cal.c \
	e-data-cal-factory.c \
	e-data-cal-view.c \
	$(NULL)

libedata_cal_1_2_la_LIBADD = \
	$(top_builddir)/calendar/libecal/libecal-1.2.la \
	$(top_builddir)/calendar/libegdbus/libegdbus-cal.la \
	$(top_builddir)/libedataserver/libedataserver-1.2.la \
	$(top_builddir)/libebackend/libebackend-1.2.la \
	$(EVOLUTION_CALENDAR_LIBS) \
	$(CAMEL_LIBS) \
	$(NULL)

libedata_cal_1_2_la_LDFLAGS = \
	-version-info $(LIBEDATACAL_CURRENT):$(LIBEDATACAL_REVISION):$(LIBEDATACAL_AGE) $(NO_UNDEFINED) \
	$(CODE_COVERAGE_LDFLAGS) \
	$(NULL)

libedata_calincludedir = $(privincludedir)/libedata-cal

libedata_calinclude_HEADERS = \
	libedata-cal.h \
	e-cal-backend.h \
	e-cal-backend-cache.h \
	e-cal-backend-factory.h \
	e-cal-backend-intervaltree.h \
	e-cal-backend-sync.h \
	e-cal-backend-util.h \
	e-cal-backend-sexp.h \
	e-data-cal.h \
	e-data-cal-factory.h \
	e-cal-backend-store.h \
	e-data-cal-view.h \
	$(NULL)

%-$(API_VERSION).pc: %.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libedata-cal-$(API_VERSION).pc

DISTCLEANFILES = $(pkgconfig_DATA)

EXTRA_DIST = \
	e-data-cal-view.xml \
	$(pkgconfig_DATA:-$(API_VERSION).pc=.pc.in) \
	$(NULL)

-include $(top_srcdir)/git.mk
