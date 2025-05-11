#!/bin/sh
#
# Helper script that builds uuidgen from util-linux as a static binary.
#
# NOTE: This script is expected to be run under Alpine Linux.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

# Define software versions.
# Use the same versions has Alpine 3.20.
UTIL_LINUX_VERSION=2.40.4

# Define software download URLs.
UTIL_LINUX_URL=https://www.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-${UTIL_LINUX_VERSION}.tar.xz

# Set same default compilation flags as abuild.
export CFLAGS="-Os -fomit-frame-pointer -Wno-expansion-to-defined"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="$CFLAGS"
export LDFLAGS="-fuse-ld=lld -Wl,--as-needed,-O1,--sort-common --static -static -Wl,--strip-all"

export CC=xx-clang
export CXX=xx-clang++

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

log() {
    echo ">>> $*"
}

#
# Install required packages.
#
HOST_PKGS="\
    curl \
    build-base \
    clang \
    lld \
    pkgconfig \
"

TARGET_PKGS="\
    g++ \
    linux-headers \
"

log "Installing required Alpine packages..."
apk --no-cache add $HOST_PKGS
xx-apk --no-cache --no-scripts add $TARGET_PKGS

#
# Build util-linux.
#
mkdir /tmp/util-linux
log "Downloading util-linux..."
curl -# -L -f ${UTIL_LINUX_URL} | tar xJ --strip 1 -C /tmp/util-linux

log "Configuring util-linux..."
(
    cd /tmp/util-linux
    ./configure \
        --build=$(TARGETPLATFORM= xx-clang --print-target-triple) \
        --host=$(xx-clang --print-target-triple) \
        --prefix=/usr \
        --disable-shared \
        --enable-static \
        --disable-all-programs \
        --enable-uuidgen \
        --enable-libuuid \
)

log "Compiling util-linux..."
make -C /tmp/util-linux -j$(nproc)

log "Installing util-linux..."
make DESTDIR=/tmp/util-linux-install -C /tmp/util-linux install
