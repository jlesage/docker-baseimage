#!/bin/sh
#
# Make sure permissions on the /tmp directory are properly set.
#
# We have seen cases on QNAP NAS where, for some reason, permissions were
# incorrect. See https://github.com/jlesage/docker-jdownloader-2/issues/98.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

chmod 1777 /tmp

# vim:ft=sh:ts=4:sw=4:et:sts=4
