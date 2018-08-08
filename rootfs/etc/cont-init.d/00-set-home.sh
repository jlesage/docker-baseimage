#!/usr/bin/with-contenv sh

#
# Clear the HOME environment variable.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

# Export variable.
echo > /var/run/s6/container_environment/HOME

# vim:ft=sh:ts=4:sw=4:et:sts=4
