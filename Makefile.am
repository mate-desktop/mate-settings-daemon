NULL =

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = 			\
	mate-settings-daemon	\
	plugins			\
	data			\
	po 			\
	$(NULL)

EXTRA_DIST = \
	autogen.sh		\
	ChangeLog		\
	README			\
	$(NULL)

DISTCLEANFILES = \
	$(NULL)

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		(GIT_DIR=$(top_srcdir)/.git \
		 ./missing --run git log --stat -M -C --name-status --date=short --no-color) | \
		 fmt --split-only > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi
