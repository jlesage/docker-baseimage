#!/bin/sh

#
# Setup the XDG runtime directory.
#
# According to https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html:
#
# $XDG_RUNTIME_DIR defines the base directory relative to which user-specific
# non-essential runtime files and other file objects (such as sockets, named
# pipes, ...) should be stored. The directory MUST be owned by the user, and he
# MUST be the only one having read and write access to it. Its Unix access mode
# MUST be 0700.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

rm -rf "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_RUNTIME_DIR"
chown $USER_ID:$GROUP_ID "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

# vim:ft=sh:ts=4:sw=4:et:sts=4
