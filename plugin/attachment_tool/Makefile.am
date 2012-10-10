libdir = $(plugindir)

lib_LTLIBRARIES = attachment_tool.la

attachment_tool_la_SOURCES = attachment_tool.c
attachment_tool_la_LDFLAGS = -module -no-undefined -avoid-version

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libsylph \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS)

attachment_tool_la_LIBADD = \
	$(GTK_LIBS) \
	$(top_builddir)/src/libsylpheed-plugin-0.la \
	$(top_builddir)/libsylph/libsylph-0.la

install:
install-plugin: install-am
