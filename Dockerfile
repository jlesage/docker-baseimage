#
# baseimage Dockerfile
#
# https://github.com/jlesage/docker-baseimage
#

ARG BASEIMAGE=unknown

ARG ALPINE_PKGS="\
    # For timezone support
    tzdata \
    # For 'groupmod' command
    shadow \
"

ARG DEBIAN_PKGS="\
    # For timezone support
    tzdata \
"

# Dockerfile cross-compilation helpers.
FROM --platform=$BUILDPLATFORM tonistiigi/xx AS xx

# Build UPX.
# NOTE: The latest official release of UPX (version 3.96) produces binaries that
# crash on ARM.  We need to manually compile it with all latest fixes.
FROM --platform=$BUILDPLATFORM alpine:3.15 AS upx
RUN apk --no-cache add build-base git bash perl ucl-dev zlib-dev zlib-static && \
    git clone https://github.com/upx/upx.git /tmp/upx && \
    git -C /tmp/upx checkout f75ad8b && \
    git -C /tmp/upx submodule init && \
    git -C /tmp/upx submodule update --recursive && \
    make LDFLAGS=-static CXXFLAGS_OPTIMIZE= -C /tmp/upx -j$(nproc) all && \
    cp -v /tmp/upx/src/upx.out /usr/bin/upx

# Build the init system and process supervisor.
FROM --platform=$BUILDPLATFORM alpine:3.15 AS cinit
ARG TARGETPLATFORM
COPY --from=xx / /
COPY src/cinit /tmp/cinit
RUN apk --no-cache add make clang
RUN xx-apk --no-cache add gcc musl-dev
RUN CC=xx-clang \
    make -C /tmp/cinit
RUN xx-verify --static /tmp/cinit/cinit
COPY --from=upx /usr/bin/upx /usr/bin/upx
RUN upx /tmp/cinit/cinit

# Build the log monitor.
FROM --platform=$BUILDPLATFORM alpine:3.15 AS logmonitor
ARG TARGETPLATFORM
ARG TARGETARCH
COPY --from=xx / /
COPY src/logmonitor /tmp/logmonitor
RUN apk --no-cache add make clang
RUN xx-apk --no-cache add gcc musl-dev linux-headers
RUN CC=xx-clang \
    make -C /tmp/logmonitor
RUN xx-verify --static /tmp/logmonitor/logmonitor
COPY --from=upx /usr/bin/upx /usr/bin/upx
RUN if [ "$TARGETARCH" != "arm64" ]; then upx /tmp/logmonitor/logmonitor; fi

# Build su-exec
FROM --platform=$BUILDPLATFORM alpine:3.15 AS su-exec
ARG TARGETPLATFORM
COPY --from=xx / /
RUN apk --no-cache add curl make clang
RUN xx-apk --no-cache add gcc musl-dev
RUN mkdir /tmp/su-exec
RUN curl -# -L https://github.com/ncopa/su-exec/archive/v0.2.tar.gz | tar xz --strip 1 -C /tmp/su-exec
RUN CC=xx-clang \
    CFLAGS="-Os -fomit-frame-pointer" \
    LDFLAGS="-static -Wl,--strip-all" \
    make -C /tmp/su-exec
RUN xx-verify --static /tmp/su-exec/su-exec
COPY --from=upx /usr/bin/upx /usr/bin/upx
RUN upx /tmp/su-exec/su-exec

# Pull base image.
FROM ${BASEIMAGE}
ARG TARGETPLATFORM

# Define working directory.
WORKDIR /tmp

# Copy helpers.
COPY helpers/* /usr/bin/

# Install the init system and process supervisor.
COPY --from=cinit /tmp/cinit/cinit /usr/sbin/

# Install the log monitor.
COPY --from=logmonitor /tmp/logmonitor/logmonitor /usr/bin/

# Install su-exec.
COPY --from=su-exec /tmp/su-exec/su-exec /usr/sbin/su-exec

# Install system packages.
ARG ALPINE_PKGS
ARG DEBIAN_PKGS
RUN \
    if [ -n "$(which apk)" ]; then \
        add-pkg ${ALPINE_PKGS}; \
    else \
        add-pkg ${DEBIAN_PKGS}; \
    fi

# Make sure all required directory exists.
RUN \
    mkdir -p \
        /defaults \
        /etc/cont-init.d \
        /etc/cont-finish.d \
        /etc/services.d \
        /etc/cont-env.d

# Add files.
COPY rootfs/ /

# Set internal environment variables.
RUN \
    set-cont-env DOCKER_IMAGE_PLATFORM "${TARGETPLATFORM:-}" && \
    true

# Set environment variables.
ENV \
    USER_ID=1000 \
    GROUP_ID=1000 \
    SUP_GROUP_IDS= \
    UMASK=0022 \
    TZ=Etc/UTC \
    KEEP_APP_RUNNING=0 \
    APP_NICENESS=0 \
    INSTALL_PACKAGES= \
    CONTAINER_DEBUG=0

# Define mountable directories.
VOLUME ["/config"]

# Define default command.
# Use the init system.
CMD ["/init"]

# Metadata.
ARG IMAGE_VERSION=unknown
LABEL \
      org.label-schema.name="baseimage" \
      org.label-schema.description="A minimal docker baseimage to ease creation of long-lived application containers" \
      org.label-schema.version="${IMAGE_VERSION}" \
      org.label-schema.vcs-url="https://github.com/jlesage/docker-baseimage" \
      org.label-schema.schema-version="1.0"
