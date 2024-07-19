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
        cp -a /defaults/repositories /etc/apk/repositories
        sed-patch "s|^https://dl-cdn.alpinelinux.org/alpine/|$PACKAGES_MIRROR/|g" /etc/apk/repositories
        ;;
    debian)
        echo "setting packages mirror to '$PACKAGES_MIRROR'..."
        if [ -f /defaults/debian.sources ]; then
            cp -a /defaults/debian.sources /etc/apt/sources.list.d/debian.sources
            sed-patch "s|^URIs: http://deb.debian.org/debian\$|URIs: $PACKAGES_MIRROR|g" /etc/apt/sources.list.d/debian.sources
        else
            cp -a /defaults/sources.list /etc/apt/sources.list
            sed-patch "s|^deb http://deb.debian.org/debian |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
        fi
        ;;
    ubuntu)
        echo "setting packages mirror to '$PACKAGES_MIRROR'..."
        if [ -f /defaults/ubuntu.sources ]; then
            cp -a /defaults/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources
            sed-patch "s|^URIs: http://archive.ubuntu.com/ubuntu/\$|URIs: $PACKAGES_MIRROR|g" /etc/apt/sources.list.d/ubuntu.sources
        else
            cp -a /defaults/sources.list /etc/apt/sources.list
            if grep -q "http://ports.ubuntu.com/ubuntu-ports/" /etc/apt/sources.list
            then
                # For archs other than i386/i686/x86_64.
                sed-patch "s|^deb http://ports.ubuntu.com/ubuntu-ports/ |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
            else
                sed-patch "s|^deb http://archive.ubuntu.com/ubuntu/ |deb $PACKAGES_MIRROR |g" /etc/apt/sources.list
            fi
        fi
        ;;
    *)
        echo "ERROR: unknown OS '$ID'."
        exit 1
        ;;
esac

# vim:ft=sh:ts=4:sw=4:et:sts=4
