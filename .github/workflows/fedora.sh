#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	dconf-devel
	desktop-file-utils
	gcc
	git
	gtk3-devel
	iso-codes-devel
	libSM-devel
	libcanberra-devel
	libmatekbd-devel
	libmatemixer-devel
	libnotify-devel
	make
	mate-common
	mate-desktop-devel
	nss-devel
	polkit-devel
	pulseaudio-libs-devel
	redhat-rpm-config
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
