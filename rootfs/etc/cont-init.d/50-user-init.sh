#!/usr/bin/with-contenv sh

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

CUSTOM_INIT_SCRIPT_PATH=/config/custom-init.sh

case "${CUSTOM_INIT_SCRIPT:-UNSET}" in
    1|true|on|enabled)
        if [ -f "$CUSTOM_INIT_SCRIPT_PATH" ]; then
            chmod +x "$CUSTOM_INIT_SCRIPT_PATH"
            "$CUSTOM_INIT_SCRIPT_PATH"
        else
            echo "WARNING: Custom init script '"$CUSTOM_INIT_SCRIPT_PATH"' not found."
        fi
        ;;
esac

# vim:ft=sh:ts=4:sw=4:et:sts=4
