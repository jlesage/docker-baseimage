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

# Take ownership of /config.  Special care is done on /config and its children
# directories, because these could be mapped to a network share.
if [ -n "$(ls -A /config)" ]; then
    for DIR in $(find /config -maxdepth 1 -type d)
    do
        if ! chown $USER_ID:$GROUP_ID $DIR 2>/dev/null
        then
            # Failed to take ownership of directory.  This could happen when, for
            # example, the folder is mapped to a network share.  Continue if we
            # have write permission, else fail.
            TMPFILE="$(su-exec $USER_ID mktemp "$DIR"/.test_XXXXXX 2>/dev/null)"
            if [ $? -eq 0 ]; then
              # Success, we were able to write file.
                su-exec $USER_ID rm "$TMPFILE"
            else
                echo "ERROR: Failed to take ownership and no write permission on $DIR."
                exit 1
            fi
        fi

        # Take ownership of files and directories under the current directory.
        if [ "$DIR" = "/config" ]; then
            find $DIR -maxdepth 1 -mindepth 1 -type f -print0 | xargs -0 chown $USER_ID:$GROUP_ID
        else
            find $DIR -maxdepth 1 -mindepth 1 -print0 | xargs -0 chown -R $USER_ID:$GROUP_ID
        fi
    done
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
