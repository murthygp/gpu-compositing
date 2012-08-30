
-include rules.make
plugin_LTLIBRARIES = libgstgpuvsink.la

AM_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
AM_LIBS = $(GST_BASE_LIBS)

noinst_HEADERS = \
	gst_buffer_manager.h  \
	gst_render_bridge.h

libgstgpuvsink_la_SOURCES = \
	gst_buffer_manager.c \
	gst_render_bridge.c  \
	gstsink_plugin.c

CMEM_LIB     ?= $(CMEM_DIR)/lib/cmem.a470MV
GST_PLUGIN_BASE = $(GSTREAMER_SRC_DIR)/gst-plugins-base-0.10.30/

# check order of CFLAGS and LIBS, shouldn't the order be the other way around
# (like in AM_CFLAGS)?
libgstgpuvsink_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DLINUX \
	-I$(CMEM_DIR)/include \
	-I$(top_srcdir)/khronos \
	-I$(top_srcdir)/module \
	-I$(GST_PLUGIN_BASE)/gst-libs/

libgstgpuvsink_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) \
        -L$(TGTFS_PATH)/lib  -lgstvideo-0.10

AM_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(libgstgpuvsink_la_CFLAGS) $(libgstgpuvsink_la_LIBADD)
libgstgpuvsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgpuvsink_la_LDFLAGS += -Wl,$(CMEM_LIB) 
