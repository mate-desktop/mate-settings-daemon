NULL =

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = 			\
	po 			\
	mate-settings-daemon	\
	plugins			\
	data			\
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-compile-warnings=no \
	CFLAGS='-Wno-deprecated-declarations'

EXTRA_DIST = \
	autogen.sh		\
	ChangeLog		\
	README			\
	its			\
	meson.build		\
	meson_options.txt	\
	data/meson.build	\
	mate-settings-daemon/meson.build \
	plugins/meson.build	\
	plugins/a11y-keyboard/meson.build \
	plugins/a11y-settings/meson.build \
	plugins/background/meson.build \
	plugins/clipboard/meson.build \
	plugins/common/meson.build \
	plugins/datetime/meson.build \
	plugins/dummy/meson.build \
	plugins/housekeeping/meson.build \
	plugins/keybindings/meson.build \
	plugins/keyboard/meson.build \
	plugins/media-keys/meson.build \
	plugins/mouse/meson.build \
	plugins/mpris/meson.build \
	plugins/rfkill/meson.build \
	plugins/smartcard/meson.build \
	plugins/sound/meson.build \
	plugins/typing-break/meson.build \
	plugins/wlrandr/meson.build \
	plugins/xrandr/meson.build \
	plugins/xrdb/meson.build \
	plugins/xsettings/meson.build \
	po/meson.build		\
	$(NULL)

DISTCLEANFILES = \
	$(NULL)

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
