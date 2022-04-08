#!/bin/sh

#
# Print information about the container.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

printf "╭―――――――――――――――――――――――――――――――――――――――――――――――――――╮\n"
printf "│                                                   │\n"
printf "│ Application:          %-27s │\n" "$(echo "$APP_NAME" | sed 's/\(.\{24\}\).*/\1.../')"
printf "│ Application Version:  %-27s │\n" "${APP_VERSION:-n/a}"
printf "│ Docker Image Version: %-27s │\n" "${DOCKER_IMAGE_VERSION:-n/a}"
printf "│                                                   │\n"
printf "╰―――――――――――――――――――――――――――――――――――――――――――――――――――╯\n"

# vim:ft=sh:ts=4:sw=4:et:sts=4
