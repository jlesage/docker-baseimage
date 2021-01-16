#!/bin/sh

#
# Clean the /var/run/logmonitor directory.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

rm -rf /var/run/logmonitor

# vim:ft=sh:ts=4:sw=4:et:sts=4
