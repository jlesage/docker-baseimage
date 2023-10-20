#!/bin/sh

#
# Set the configured packages mirror.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ -z "${PACKAGES_MIRROR:-}" ]; then
    # Nothing to do.
    return
fi

. /etc/os-release

case "$ID" in
    alpine)
        echo "setting packages mirror to '$PACKAGES_MIRROR'..."
        cp /defaults/repositories /etc/apk/repositories
        sed-patch "s|^https://dl-cdn.alpinelinux.org/alpine/|$PACKAGES_MIRROR/|g" /etc/apk/repositories
        ;;
    debian)
        echo "setting packages mirror to '$PACKAGES_MIRROR'..."
        cp /defaults/sources.list /etc/apt/sources.list
        sed-patch "s|^deb http://deb.debian.org/debian |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
        ;;
    ubuntu)
        echo "setting packages mirror to '$PACKAGES_MIRROR'..."
        cp /defaults/sources.list /etc/apt/sources.list
        if grep -q "http://ports.ubuntu.com/ubuntu-ports/" /etc/apt/sources.list
        then
            sed-patch "s|^deb http://ports.ubuntu.com/ubuntu-ports/ |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
        else
            sed-patch "s|^deb http://archive.ubuntu.com/ubuntu/ |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
        fi
        ;;
    *)
        echo "ERROR: unknown OS '$ID'."
        exit 1
        ;;
esac

# vim:ft=sh:ts=4:sw=4:et:sts=4
