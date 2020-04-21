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

