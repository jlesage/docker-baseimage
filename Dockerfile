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
"

ARG DEBIAN_PKGS="\
    # For timezone support
    tzdata \
"

# Common stuff of the baseimage.
FROM ${BASEIMAGE_COMMON} AS baseimage-common

# Pull base image.
FROM ${BASEIMAGE} AS baseimage
ARG TARGETPLATFORM

# Define working directory.
WORKDIR /tmp

# Install common software.
COPY --link --from=baseimage-common / /

# Configure package manager.
RUN \
    if command -v apt-get > /dev/null; then \
        echo 'Dir::Log "/dev/null";' > /etc/apt/apt.conf.d/docker-log-dir && \
        echo 'APT::Sandbox::User "root";' > /etc/apt/apt.conf.d/docker-disable-sandbox && \
        \
        /opt/base/bin/sed-patch 's|/var/log/dpkg.log|/dev/null|' /etc/dpkg/dpkg.cfg && \
        \
        dpkg-divert --add --local --rename --divert /usr/bin/update-alternatives.real /usr/bin/update-alternatives && \
        echo '#!/bin/sh' > /usr/bin/update-alternatives && \
        echo 'exec /usr/bin/update-alternatives.real --log /dev/null "$@"' >> /usr/bin/update-alternatives && \
        chmod +x /usr/bin/update-alternatives && \
        true; \
    elif command -v apk > /dev/null; then \
        if apk add --help 2>&1 | grep -q logfile; then \
            mkdir -p /usr/local/sbin; \
            echo '#!/bin/sh' > /usr/local/sbin/apk; \
            echo 'exec /sbin/apk --logfile=no "$@"' >> /usr/local/sbin/apk; \
            chmod +x /usr/local/sbin/apk; \
        fi && \
        true; \
    fi

# Upgrade existing packages.
RUN /opt/base/bin/upg-pkg all

# Install system packages.
ARG ALPINE_PKGS
ARG DEBIAN_PKGS
RUN \
    case "$(awk -F= '/^ID=/ {print $2}' /etc/os-release)" in \
        alpine) \
            /opt/base/bin/add-pkg ${ALPINE_PKGS}; \
            ;; \
        debian|ubuntu) \
            /opt/base/bin/add-pkg ${DEBIAN_PKGS}; \
            ;; \
        *) \
            echo "ERROR: unknown os ID '$(awk -F= '/^ID=/ {print $2}' /etc/os-release)'"; \
            exit 1; \
            ;; \
    esac

# Install shadow-rs if needed.
# Some old distros include shadow-utils (useradd, passwd, groupadd, etc.) that
# doesn't work when user and group databases are symlinks.
RUN \
    DISTRO="$(cat /etc/os-release | sed -n 's/PRETTY_NAME="\(.*\)"/\1/p')" && \
    case "$DISTRO" in \
        Ubuntu\ 22.*) cp -a /shadow-rs/* / ;; \
        Ubuntu\ 20.*) cp -a /shadow-rs/* / ;; \
        Ubuntu\ 18.*) cp -a /shadow-rs/* / ;; \
        Debian\ GNU/Linux\ 11\ *) cp -a /shadow-rs/* / ;; \
    esac && \
    rm -rf /shadow-rs

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

# Setup variable data files directories.
# https://refspecs.linuxfoundation.org/FHS_3.0/fhs/ch05.html
RUN \
    for dir in $(find /var -mindepth 1 -maxdepth 1 -type d -or -type l); do \
        case "$dir" in \
            /var/backups) rm -rf "$dir" ;; \
            /var/cache)   ;; \
            /var/empty)   ;; \
            /var/lib)     ;; \
            /var/local)   rm -rf "$dir" ;; \
            /var/lock)    rm -rf "$dir" ; ln -s /run/lock /var/lock ;; \
            /var/log)     rm -rf "$dir" ; ln -s /config/log /var/log ;; \
            /var/mail)    rm -rf "$dir" ;; \
            /var/opt)     rm -rf "$dir" ;; \
            /var/run)     rm -rf "$dir" ; ln -sf /run /var/run ;; \
            /var/spool)   rm -rf "$dir" ;; \
            /var/tmp)     rm -rf "$dir" ; ln -s /config/var/tmp /var/tmp ;; \
            *) echo "ERROR: unknown directory: $dir"; exit 1 ;; \
        esac \
    done

# Setup the run-time variable data folder.
RUN \
    rm -rf /run  && \
    ln -sf /tmp/run /run && \
    true

# User management files are stored outside the container's file system.
RUN \
    ln -sf /tmp/.passwd /etc/passwd && \
    ln -sf /tmp/.group /etc/group && \
    ln -sf /tmp/.shadow /etc/shadow  && \
    if [ -f /etc/gshadow ]; then \
        ln -sf /tmp/.gshadow /etc/gshadow; \
    fi && \
    true

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

FROM scratch

COPY --from=baseimage / /

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
