#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
)

# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-settings-daemon
requires+=(
	autoconf-archive
	dbus-glib
	gcc
	gettext
	git
	glib2-devel
	libcanberra
	libmatekbd
	libmatemixer
	libnotify
	make
	mate-common
	mate-desktop
	nss
	polkit
	python-packaging
	which
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
