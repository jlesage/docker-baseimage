#!/bin/sh
#
# Helper script that builds logrotate as a static binary.
#
# NOTE: This script is expected to be run under Alpine Linux.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Define software versions.
LOGROTATE_VERSION=3.21.0
POPT_VERSION=1.19

# Define software download URLs.
LOGROTATE_URL=https://github.com/logrotate/logrotate/releases/download/${LOGROTATE_VERSION}/logrotate-${LOGROTATE_VERSION}.tar.xz
POPT_URL=https://ftp.osuosl.org/pub/rpm/popt/releases/popt-1.x/popt-${POPT_VERSION}.tar.gz

# Set same default compilation flags as abuild.
export CFLAGS="-Os -fomit-frame-pointer"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="$CFLAGS"
export LDFLAGS="-fuse-ld=lld -Wl,--as-needed --static -static -Wl,--strip-all"

export CC=xx-clang
export CXX=xx-clang++

log() {
    echo ">>> $*"
}

#
# Install required packages.
#
log "Installing required Alpine packages..."
apk --no-cache add \
    curl \
    build-base \
    clang \
    lld \

xx-apk --no-cache --no-scripts add \
    g++ \

#
# Compile popt.
# The static library is not provided by Alpine repository, so we need to build
# it ourself.
#

mkdir /tmp/popt
log "Downloading popt..."
curl -# -L -f ${POPT_URL} | tar -xz --strip 1 -C /tmp/popt

log "Configuring popt..."
(
    cd /tmp/popt && ./configure \
        --build=$(TARGETPLATFORM= xx-clang --print-target-triple) \
        --host=$(xx-clang --print-target-triple) \
        --prefix=/usr \
        --disable-nls \
        --disable-shared \
        --enable-static \
)

log "Compiling popt..."
make -C /tmp/popt -j$(nproc)

log "Installing popt..."
make DESTDIR=$(xx-info sysroot) -C /tmp/popt install

#
# Compile logrotate.
#

mkdir /tmp/logrotate
log "Downloading logrotate..."
curl -# -L -f ${LOGROTATE_URL} | tar -xJ --strip 1 -C /tmp/logrotate

log "Patching logrotate..."
patch -p1 -d /tmp/logrotate < "$SCRIPT_DIR"/messages-fix.patch

log "Configuring logrotate..."
(
    cd /tmp/logrotate && ./configure \
        --build=$(TARGETPLATFORM= xx-clang --print-target-triple) \
        --host=$(xx-clang --print-target-triple) \
        --prefix=/usr \
)

log "Compiling logrotate..."
make -C /tmp/logrotate -j$(nproc)

log "Installing logrotate..."
make DESTDIR=/tmp/logrotate-install -C /tmp/logrotate install

