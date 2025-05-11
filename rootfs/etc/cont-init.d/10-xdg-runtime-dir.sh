#!/bin/sh
#
# Setup the XDG runtime directory.
#
# According to https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html:
#
# ${XDG_RUNTIME_DIR} defines the base directory relative to which user-specific
# non-essential runtime files and other file objects (such as sockets, named
# pipes, ...) should be stored. The directory MUST be owned by the user, and he
# MUST be the only one having read and write access to it. Its Unix access mode
# MUST be 0700.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

# Make sure all folders leading to XDG_RUNTIME_DIR have the correct
# permission.
dirs=
subdir="$(dirname "${XDG_RUNTIME_DIR}")"
while [ "${subdir}" != "/tmp" ]; do
    dirs="${subdir} ${dirs}"
    subdir="$(dirname "${subdir}")"
done
for dir in ${dirs}; do
    if [ -d "${dir}" ]; then
        chmod 755 "${dir}"
    else
        mkdir --mode=755 "${dir}"
    fi
done

# XDG_RUNTIME_DIR has special permissions and ownership.
rm -rf "${XDG_RUNTIME_DIR}"
mkdir --mode=700 "${XDG_RUNTIME_DIR}"
chown "${USER_ID}:${GROUP_ID}" "${XDG_RUNTIME_DIR}"

# vim:ft=sh:ts=4:sw=4:et:sts=4
