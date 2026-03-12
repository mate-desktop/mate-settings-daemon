#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autopoint
	gcc
	g++
	git
	libcanberra-gtk3-dev
	libdconf-dev
	libfontconfig1-dev
	libglib2.0-dev
	libgtk-3-dev
	libmate-desktop-dev
	libmatekbd-dev
	libmatemixer-dev
	libnotify-dev
	libnss3-dev
	libpolkit-agent-1-dev
	libpolkit-gobject-1-dev
	libpulse-dev
	libstartup-notification0-dev
	libx11-dev
	libxext-dev
	libxi-dev
	libxklavier-dev
	libxrandr-dev
	libxt-dev
	make
	mate-common
	x11proto-kb-dev
)

infobegin "Update system"
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
