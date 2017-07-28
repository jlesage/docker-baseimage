#!/usr/bin/with-contenv sh
#
# This script do some cleaning during the startup.
#

# Make sure to remove the container shutting down indication.  Can still be
# laying around when the container is restarted.
rm -f /var/run/s6/container_environment/CONTAINER_SHUTTING_DOWN

# vim: set ft=sh :
