#!/usr/bin/with-contenv sh

#
# Take ownership of files and directories under /config.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ "${TAKE_CONFIG_OWNERSHIP:-1}" -eq 1 ]; then
    chown -R $USER_ID:$GROUP_ID /config
fi

# vim: set ft=sh :
