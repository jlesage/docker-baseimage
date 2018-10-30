#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

source "$(dirname "$0")/common.sh"

DOCKER_CMD_OPTS=" "

usage() {
    if [ "$*" ]; then
        echo "$*"
        echo
    fi

    echo "usage: $( basename $0 ) [OPTION]... NAME ARCH BASEIMAGE DOCKERFILE [VERSION]

Arguments:
  NAME                  Name of the Docker image to build.
  ARCH                  Docker image architecture to build.  Valid values are:
$(echo "$ALL_DOCKER_ARCHS" | sed 's/^/                          /')
  BASEIMAGE             Docker baseimage to use.
  DOCKERFILE            Dockerfile to use.
  VERSION               Version of the Docker image to build.

Options:
  -c, --without-cache   Do not use the Docker cache when building.
  -r, --register-binfmt Register binfmt interpreters.  This needs to be done
                        only once on the same machine.
  -h, --help            Display this help.
"
}

# Parse arguments.
while [[ $# > 0 ]]
do
    key="$1"

    case $key in
        -c|--without-cache)
            DOCKER_CMD_OPTS="$DOCKER_CMD_OPTS --no-cache --pull"
            ;;
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
DOCKER_IMAGE_NAME="${1:-UNSET}"
[ "$DOCKER_IMAGE_NAME" = "UNSET" ] && usage "ERROR: A Docker image name must be specified." && exit 1

# Get the Docker image architecture.
DOCKER_IMAGE_ARCH="${2:-UNSET}"
[ "$DOCKER_IMAGE_ARCH" = "UNSET" ] && usage "ERROR: A Docker image architecture must be specified." && exit 1
if ! echo "$ALL_DOCKER_ARCHS" | grep -wq "${DOCKER_IMAGE_ARCH}"; then
    usage "ERROR: Invalid Docker image architecture '$DOCKER_IMAGE_ARCH'."
    exit 1
fi

# Get the Docker baseimage.
DOCKER_BASEIMAGE="${3:-UNSET}"
[ "$DOCKER_BASEIMAGE" = "UNSET" ] && usage "ERROR: A Docker baseimage must be specified." && exit 1

# Get the Dockerfile.
DOCKERFILE="${4:-UNSET}"
[ "$DOCKERFILE" = "UNSET" ] && usage "ERROR: A Dockerfile must be specified." && exit 1
if [ ! -f "$BASE_DIR"/"$DOCKERFILE" ]; then
    echo "ERROR: Dockerfile '$BASE_DIR/$DOCKERFILE' not found."
    exit 1
fi

# Get the Docker image version.
DOCKER_IMAGE_VERSION="${5:-dev}"

# Adjust the Docker image version.
if [[ "$DOCKER_IMAGE_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)[0-9]*)?$ ]]; then
    # Remove the leading 'v' from the version.
    DOCKER_IMAGE_VERSION="${DOCKER_IMAGE_VERSION:1}"
elif  [[ "$DOCKER_IMAGE_VERSION" = "master" ]]; then
    # Special case for Travis/development builds.
    DOCKER_IMAGE_VERSION="dev"
fi

# Register binfmt handlers if needed.
if [ "${REGISTER_BINFMT_HANDLERS:-0}" -eq 1 ]; then
    register_binfmt_handlers
fi

# Prepare the build directory.
rm -rf "$BUILD_DIR"
cp -r "$BASE_DIR" "$BUILD_DIR"
cp "$BUILD_DIR"/"$DOCKERFILE" "$BUILD_DIR"/Dockerfile.actual
case $DOCKER_IMAGE_ARCH in
    amd64|i386)
        # QEMU handler not needed.
        ;;
    *)
        # QEMU static binary needs to be copied into the container.
        QEMU_HANDLER="$(get_qemu_handler "$DOCKER_IMAGE_ARCH")"
        cp "$QEMU_HANDLER" "$BUILD_DIR/helpers/"
        ;;
esac

# Check if Docker experimental features are enabled.
if [ "$(docker version -f '{{.Server.Experimental}}')" = "true" ]; then
    DOCKER_CMD_OPTS="$DOCKER_CMD_OPTS --squash"
else
    echo "WARNING: Docker experimental features not enabled!  Image size won't be optimized."
    echo "         See https://github.com/docker/docker-ce/blob/master/components/cli/experimental/README.md"
    echo "         to enable experimental features."
fi

# Build image.
echo "Starting build of Docker image '$DOCKER_IMAGE_NAME'..."
docker build $DOCKER_CMD_OPTS \
         --build-arg BASEIMAGE="$DOCKER_BASEIMAGE" \
         --build-arg IMAGE_VERSION="${DOCKER_IMAGE_VERSION}" \
         -f "$BUILD_DIR"/Dockerfile.actual \
         -t "$DOCKER_IMAGE_NAME" \
         "$BUILD_DIR"

# vim:ts=4:sw=4:et:sts=4
