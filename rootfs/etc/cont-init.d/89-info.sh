#!/bin/sh

#
# Print information about the container.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

printf ":::    ╭――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――╮\n"
printf ":::    │                                                                      │\n"
printf ":::    │ Application:           %-45s │\n" "$(echo "$APP_NAME" | sed 's/\(.\{42\}\).*/\1.../')"
printf ":::    │ Application Version:   %-45s │\n" "${APP_VERSION:-n/a}"
printf ":::    │ Docker Image Version:  %-45s │\n" "${DOCKER_IMAGE_VERSION:-n/a}"
printf ":::    │ Docker Image Platform: %-45s │\n" "${DOCKER_IMAGE_PLATFORM:-unknown}"
printf ":::    │                                                                      │\n"
printf ":::    ╰――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――╯\n"

# vim:ft=sh:ts=4:sw=4:et:sts=4
