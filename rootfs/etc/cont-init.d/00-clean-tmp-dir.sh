#!/usr/bin/with-contenv sh

#
# Clean the /tmp directory.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ "${CLEAN_TMP_DIR:-1}" -eq 1 ]; then
    rm -rf /tmp/*
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
