#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

source "$(dirname "$0")/common.sh"

usage() {
    if [ "$*" ]; then
        echo "$*"
        echo
    fi

    echo "usage: $( basename $0 ) [OPTION]... NAME ARCH

Arguments:
  NAME                  Name of the Docker image to test.
  ARCH                  Architecture of the Docker image to test.  Valid values are:
$(echo "$ALL_DOCKER_ARCHS" | sed 's/^/                          /')

Options:
  -r, --register-binfmt Register binfmt interpreters.  This needs to be done
                        once.
  -h, --help            Display this help.
"
}

# Parse arguments.
while [[ $# > 0 ]]
do
    key="$1"

    case $key in
        -r|--register-binfmt)
            REGISTER_BINFMT_HANDLERS=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            usage "ERROR: Unknown argument: $key"
            exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done

# Get the Docker image name.
DOCKER_IMAGE="${1:-UNSET}"
[ "$DOCKER_IMAGE" = "UNSET" ] && usage "ERROR: At Docker image name must be specified." && exit 1

# Get the Docker image architecture.
DOCKER_IMAGE_ARCH="${2:-UNSET}"
[ "$DOCKER_IMAGE_ARCH" = "UNSET" ] && usage "ERROR: A Docker image architecture must be specified." && exit 1
if ! echo "$ALL_DOCKER_ARCHS" | grep -wq "$DOCKER_IMAGE_ARCH"; then
    usage "ERROR: Invalid Docker image architecture '$DOCKER_IMAGE_ARCH'."
    exit 1
fi

# Register binfmt handlers if needed.
if [ "${REGISTER_BINFMT_HANDLERS:-0}" -eq 1 ]; then
    register_binfmt_handlers
fi

# Handle QEMU handler.
QEMU_HANDLER=UNSET
case $DOCKER_IMAGE_ARCH in
    amd64|i386)
        # QEMU handler not needed.
        ;;
    *)
        QEMU_HANDLER="$(get_qemu_handler "$DOCKER_IMAGE_ARCH")"
        ;;
esac

echo "Starting tests of Docker image $DOCKER_IMAGE..."
env DOCKER_IMAGE="$DOCKER_IMAGE" QEMU_HANDLER="$QEMU_HANDLER" bats tests

# vim:ts=4:sw=4:et:sts=4
