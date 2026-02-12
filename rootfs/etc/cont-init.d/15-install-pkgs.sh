#!/bin/sh
#
# Install extra package(s), if requested.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ -n "${INSTALL_PACKAGES:-}" ] || [ -n "${INSTALL_PACKAGES_INTERNAL:-}" ]; then
    echo "installing requested package(s)..."
    for PKG in ${INSTALL_PACKAGES:-} ${INSTALL_PACKAGES_INTERNAL:-}; do
        if check-pkg "${PKG}"; then
            echo "package '${PKG}' already installed"
        else
            echo "installing '${PKG}'..."
            add-pkg "${PKG}"
        fi
    done
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
