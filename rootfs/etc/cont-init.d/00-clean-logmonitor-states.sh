#!/usr/bin/with-contenv sh

#
# Clean the /var/run/logmonitor directory.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

rm -rf /var/run/logmonitor

# vim: set ft=sh :
