# syntax=docker/dockerfile:1.4
#
# baseimage Dockerfile
#
# https://github.com/jlesage/docker-baseimage
#

ARG BASEIMAGE=unknown
ARG BASEIMAGE_COMMON=unknown

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

# Common stuff of the baseimage.
FROM ${BASEIMAGE_COMMON} AS baseimage-common

# Pull base image.
FROM ${BASEIMAGE}
ARG TARGETPLATFORM

# Define working directory.
WORKDIR /tmp

# Install common software.
COPY --link --from=baseimage-common / /

# Install system packages.
ARG ALPINE_PKGS
ARG DEBIAN_PKGS
RUN \
    if [ -n "$(which apk)" ]; then \
        /opt/base/bin/add-pkg ${ALPINE_PKGS}; \
    else \
        /opt/base/bin/add-pkg ${DEBIAN_PKGS}; \
    fi

# Load our RC file when logging in to the container.
RUN \
    if [ -f /root/.profile ]; then \
        echo "# Include Docker container definitions." >> /root/.profile && \
        echo ". /root/.docker_rc" >> /root/.profile; \
    fi

# Make sure all required directory exists.
RUN \
    mkdir -p \
        /defaults \
        /opt/base/etc/logrotate.d \
        /etc/services.d \
        /etc/cont-env.d \
        /etc/cont-init.d \
        /etc/cont-finish.d \
        /etc/cont-logrotate.d \
    && true

# Keep a copy of default packages repository.
RUN \
    if [ -f /etc/apk/repositories ]; then \
        cp /etc/apk/repositories /defaults/; \
    elif [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then \
        cp /etc/apt/sources.list.d/ubuntu.sources /defaults/; \
    elif [ -f /etc/apt/sources.list.d/debian.sources ]; then \
        cp /etc/apt/sources.list.d/debian.sources /defaults/; \
    else \
        cp /etc/apt/sources.list /defaults/; \
    fi

# Set internal environment variables.
RUN \
    /opt/base/bin/set-cont-env DOCKER_IMAGE_PLATFORM "${TARGETPLATFORM:-}" && \
    true

# Set environment variables.
ENV \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/base/sbin:/opt/base/bin \
    ENV=/root/.docker_rc \
    USER_ID=1000 \
    GROUP_ID=1000 \
    SUP_GROUP_IDS= \
    UMASK=0022 \
    LANG=en_US.UTF-8 \
    TZ=Etc/UTC \
    KEEP_APP_RUNNING=0 \
    APP_NICENESS=0 \
    INSTALL_PACKAGES= \
    PACKAGES_MIRROR= \
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
