NULL =

noinst_LTLIBRARIES = libgvc.la

INCLUDES =					\
	$(WARN_CFLAGS)				\
	$(NULL)

libgvc_la_LIBADD =		\
	$(NULL)

libgvc_la_SOURCES =			\
	$(NULL)

if HAVE_PULSE
INCLUDES +=					\
	$(VOLUME_CONTROL_CFLAGS)		\
	$(PULSE_CFLAGS)

libgvc_la_LIBADD +=		\
	$(VOLUME_CONTROL_LIBS)	\
	$(PULSE_LIBS)

libgvc_la_SOURCES +=		\
	gvc-mixer-stream.h		\
	gvc-mixer-stream.c		\
	gvc-channel-map.h		\
	gvc-channel-map.c		\
	gvc-mixer-card.c		\
	gvc-mixer-card.h		\
	gvc-mixer-sink.h		\
	gvc-mixer-sink.c		\
	gvc-mixer-source.h		\
	gvc-mixer-source.c		\
	gvc-mixer-sink-input.h		\
	gvc-mixer-sink-input.c		\
	gvc-mixer-source-output.h	\
	gvc-mixer-source-output.c	\
	gvc-mixer-event-role.h		\
	gvc-mixer-event-role.c		\
	gvc-mixer-control.h		\
	gvc-mixer-control.c
endif

if HAVE_GSTREAMER
INCLUDES +=					\
	$(SETTINGS_PLUGIN_CFLAGS)		\
	$(AM_CFLAGS)				\
	$(GST_CFLAGS)

libgvc_la_LIBADD +=		\
	$(GST_LIBS)

libgvc_la_SOURCES +=			\
	gvc-gstreamer-acme-vol.c	\
	gvc-gstreamer-acme-vol.h
endif

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in
