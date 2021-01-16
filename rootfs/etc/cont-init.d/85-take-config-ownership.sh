#!/bin/sh

#
# Take ownership of files and directories under /config.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ "${TAKE_CONFIG_OWNERSHIP:-1}" -eq 0 ]; then
    echo "not taking ownership of /config"
    exit 0
fi

# Take ownership of /config.
if ! chown $USER_ID:$GROUP_ID /config; then
    # Failed to take ownership of /config.  This could happen when,
    # for example, the folder is mapped to a network share.
    # Continue if we have write permission, else fail.
    if su-exec $USER_ID [ ! -w /config ]; then
        echo "ERROR: Failed to take ownership and no write permission on /config."
        exit 1
    fi
fi

# Take ownership of all files and folders under /config.
if [ -n "$(ls -A /config)" ]; then
    find /config -maxdepth 1 -mindepth 1 -print0 | xargs -0 chown -R $USER_ID:$GROUP_ID
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
