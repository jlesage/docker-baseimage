#!/usr/bin/with-contenv sh

#
# Take ownership of files and directories under /config.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

if [ "${TAKE_CONFIG_OWNERSHIP:-1}" -eq 1 ]; then
    if ! chown -R $USER_ID:$GROUP_ID /config; then
        # Failed to take ownership of /config.  This could happen when,
        # for example, the folder is mapped to a network share.
        # Continue if we have write permission, else fail.
        if su-exec $USER_ID [ ! -w /config ]; then
            echo "ERROR: Failed to take ownership and no write permission on /config."
            exit 1
        fi
    fi

    find /config -mindepth 1 -exec chown $USER_ID:$GROUP_ID {} \;
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
