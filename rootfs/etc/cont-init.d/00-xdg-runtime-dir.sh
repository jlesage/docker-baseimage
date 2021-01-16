#!/bin/sh

#
# Setup the XDG runtime directory.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

rm -rf "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_RUNTIME_DIR"
chown $USER_ID:$GROUP_ID "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

# vim:ft=sh:ts=4:sw=4:et:sts=4
