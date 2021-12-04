#!/bin/sh

#
# Install extra package(s), if requested.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

is_pkg_installed_alpine() {
    apk -e info "$1" > /dev/null
}

is_pkg_installed_debian() {
    dpkg --status "$1" 2>&1 | grep -q "^Status: install ok installed"
}

is_pkg_installed() {
    if [ -n "$(which apk)" ]; then
        is_pkg_installed_alpine "$@"
    else
        is_pkg_installed_debian "$@"
    fi
}

if [ -n "${INSTALL_PACKAGES:-}" ]; then
    echo "installing requested package(s)..."
    for PKG in $INSTALL_PACKAGES; do
        if is_pkg_installed "$PKG"; then
            echo "package '$PKG' already installed"
        else
            echo "installing '$PKG'..."
            add-pkg "$PKG"
        fi
    done
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
