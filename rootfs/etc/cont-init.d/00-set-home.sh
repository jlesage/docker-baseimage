#!/usr/bin/with-contenv sh

#
# Handle configured niceness value of the application.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

# Export variable.
echo > /var/run/s6/container_environment/HOME

# vim: set ft=sh :
