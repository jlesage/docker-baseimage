#!/usr/bin/with-contenv sh

#
# Make sure the startapp.sh has execution permission.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ -e /startapp.sh ] ; then
    chmod 755 /startapp.sh
    sync
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
