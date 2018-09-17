#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
BASE_DIR="$(cd "$SCRIPT_DIR/../" && pwd)"
DATA_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/docker-baseimage"
BUILD_DIR="$DATA_DIR/build"

ALL_DOCKER_FLAVORS="$("$SCRIPT_DIR"/defs.py list-flavors)"
ALL_DOCKER_ARCHS="$("$SCRIPT_DIR"/defs.py list-archs)"

MANIFEST_TOOL_VERSION=0.7.0
MANIFEST_TOOL_URL=https://github.com/estesp/manifest-tool/releases/download/v${MANIFEST_TOOL_VERSION}/manifest-tool-linux-amd64

QEMU_HANDLER_VERSION=2.12.0

# Make sure the data directory exits.
mkdir -p "$DATA_DIR"

register_binfmt_handlers() {
    echo "Registering binfmt handlers..."
    docker run --rm --privileged multiarch/qemu-user-static:register
}

get_qemu_handler() {
    QEMU_ARCH="$("$SCRIPT_DIR"/defs.py get-qemu-arch "$1")"
    QEMU_HANDLER="$DATA_DIR"/qemu-$QEMU_ARCH-static
    if [ ! -f "$QEMU_HANDLER" ]; then
        >&2 echo "Downloading QEMU handler for $DOCKER_IMAGE_ARCH..."
        QEMU_HANDLER_URL=https://github.com/multiarch/qemu-user-static/releases/download/v$QEMU_HANDLER_VERSION/x86_64_qemu-$QEMU_ARCH-static.tar.gz
        curl -# -L "$QEMU_HANDLER_URL" | tar xz -C "$DATA_DIR"
    fi
    echo "$DATA_DIR/qemu-$QEMU_ARCH-static"
}

# vim:ts=4:sw=4:et:sts=4
