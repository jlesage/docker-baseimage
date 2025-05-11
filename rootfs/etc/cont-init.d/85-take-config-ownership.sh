#!/bin/sh
#
# Take ownership of files and directories under /config.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if is-bool-val-false "${TAKE_CONFIG_OWNERSHIP:-1}"; then
    echo "not taking ownership of /config"
    exit 0
fi

take-ownership /config

# vim:ft=sh:ts=4:sw=4:et:sts=4
