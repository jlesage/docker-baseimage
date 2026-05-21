#!/bin/sh

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

export CC=xx-clang
export CXX=xx-clang++

export RUSTFLAGS="-C target-feature=+crt-static"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

log() {
    echo ">>> $*"
}

SHADOW_RS_URL="$1"

if [ -z "$SHADOW_RS_URL" ]; then
    log "ERROR: shadow-rs URL missing."
    exit 1
fi

#
# Install required packages.
#
apk --no-cache add \
    bash \
    curl \
    clang \
    make \
    patch \
    pkgconf \

xx-apk --no-cache --no-scripts add \
    musl-dev \
    gcc \

# Install Rust.
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | bash -s -- -y
source /root/.cargo/env

# Fix the xx-cargo setup. See https://github.com/tonistiigi/xx/issues/104.
# When building linux/arm/v6, there is a mismatch in triples:
#   - cargo: arm-unknown-linux-musleabi
#   - xx-info: armv6-alpine-linux-musleabihf.
if xx-info is-cross; then
    xx-cargo --setup-target-triple
    if [ ! -e "/$(xx-cargo --print-target-triple)" ]; then
        ln -s "$(xx-info)" "/$(xx-cargo --print-target-triple)"
    fi

    for d in $(find $(xx-info sysroot) -name "$(xx-info)" -type d); do
        cargo_d="$(dirname "$d")/$(xx-cargo --print-target-triple)"
        [ ! -e "$cargo_d" ] || continue

        log "xx-cargo setup fix: creating symlink '$cargo_d', pointing '$(xx-info)'."
        ln -s "$(xx-info)" "$cargo_d"
    done
fi

#
# Download sources.
#

log "Downloading shadow-rs..."
mkdir /tmp/shadow-rs
curl -# -L -f ${SHADOW_RS_URL} | tar xz --strip 1 -C /tmp/shadow-rs

#
# Compile shadow-rs.
#

# Create cargo profile.
# https://github.com/johnthagen/min-sized-rust
echo "" >> /tmp/shadow-rs/.cargo/config.toml
echo "[profile.release]" >> /tmp/shadow-rs/.cargo/config.toml
echo "strip = true" >> /tmp/shadow-rs/.cargo/config.toml
echo "debug = false" >> /tmp/shadow-rs/.cargo/config.toml
echo "overflow-checks = true" >> /tmp/shadow-rs/.cargo/config.toml
echo "opt-level = 's'" >> /tmp/shadow-rs/.cargo/config.toml
echo "lto = 'thin'" >> /tmp/shadow-rs/.cargo/config.toml
echo "panic = 'abort'" >> /tmp/shadow-rs/.cargo/config.toml
echo "codegen-units = 1" >> /tmp/shadow-rs/.cargo/config.toml

log "Patching shadow-rs..."
PATCHES="
    build-fix.patch
    override-login-defs-argument.patch
"
for PATCH in $PATCHES; do
    log "Applying $PATCH..."
    patch  -p1 -d /tmp/shadow-rs < "$SCRIPT_DIR"/"$PATCH"
done

log "Compiling shadow-rs..."
(
    cd /tmp/shadow-rs
    xx-cargo build --release --bin shadow-rs --no-default-features --features feat_common_core
)

log "Installing shadow-rs..."
mkdir /tmp/shadow-rs-install
CARGO_BUILD_TARGET=$(xx-cargo --print-target-triple) DESTDIR=/tmp/shadow-rs-install make -C /tmp/shadow-rs install-multicall PREFIX=/usr
chmod 755 /tmp/shadow-rs-install/usr/sbin/shadow-rs
