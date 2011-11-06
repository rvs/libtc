COMPILE = echo '  CC      $<' && $(CC) $(DEFS) $(DEFAULT_INCLUDES) \
	$(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LEXCOMPILE = echo '  LEX     $<' && $(LEX) $(LFLAGS) $(AM_LFLAGS)
LIBTOOL = case $@ in 					\
	install*) echo "  INSTALL $$p";;	       	\
	*.lo) echo '  CC      $<';;			\
	*) echo '  LD      $@';; 			\
	esac; true " >/dev/null " && @LIBTOOL@ --quiet

lib_LTLIBRARIES = libtc.la
libtc_la_SOURCES = list.c hash.c gethostaddr.c gethostname.c pathfind.c \
		   strtotime.c conf.c conf-parse.l tree.c alloc.c \
		   prioq.c math.c string.c regex.c mkpath.c mpool.c
libtc_la_LDFLAGS = -version-info 16:0:0
libtc_la_LIBADD = @LTLIBOBJS@
INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include
EXTRA_DIST = tcc-internal.h

bin_PROGRAMS = tcconfdump
tcconfdump_SOURCES = confdump.c
tcconfdump_LDFLAGS = libtc.la

.SILENT:
