# A minimal docker baseimage to ease creation of long-lived application containers
[![Docker Automated build](https://img.shields.io/docker/automated/jlesage/baseimage.svg)](https://hub.docker.com/r/jlesage/baseimage/) [![Build Status](https://travis-ci.org/jlesage/docker-baseimage.svg?branch=master)](https://travis-ci.org/jlesage/docker-baseimage)

This is a docker baseimage that can be used to create containers for any
long-lived application.

## Images
Different docker images are available:

| Base distribution  | Tag              | Size |
|--------------------|------------------|------|
| [Alpine 3.5]       | alpine-3.5       | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.5 "Get your own image badge on microbadger.com") |
| [Alpine 3.6]       | alpine-3.6       | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.6 "Get your own image badge on microbadger.com") |
| [Alpine 3.5]       | alpine-3.5-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.5-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.6]       | alpine-3.6-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.6-glibc "Get your own image badge on microbadger.com") |
| [Debian 8]         | debian-8         | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-8.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-8/ "Get your own image badge on microbadger.com") |
| [Ubuntu 16.04 LTS] | ubuntu-16.04     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:ubuntu-16.04.svg)](http://microbadger.com/#/images/jlesage/baseimage:ubuntu-16.04 "Get your own image badge on microbadger.com") |

[Alpine 3.5]: https://alpinelinux.org
[Alpine 3.6]: https://alpinelinux.org
[Debian 8]: https://www.debian.org/releases/jessie/
[Ubuntu 16.04 LTS]: http://releases.ubuntu.com/16.04/

Due to its size, the `Alpine` image is recommended.  However, it may be harder
to integrate your application (especially third party ones without source code),
because:
 1. Packages repository may not be as complete as `Ubuntu`/`Debian`.
 2. Third party applications may not support `Alpine`.
 3. The `Alpine` distribution uses the [musl] C standard library instead of
 GNU C library ([glibc]).
  * NOTE: Using the `Alpine` image with glibc integrated (`alpine-3.5-glibc`
    tag) may ease integration of applications.

The next choice is to use the `Debian` image.  It provides a great compatibility
and its size is smaller than the `Ubuntu` one.  Finally, if for any reason you
prefer an `Ubuntu` image, one based on the stable `16.04 LTS` version is
provided.

[musl]: https://www.musl-libc.org/
[glibc]: https://www.gnu.org/software/libc/

### Content
Here are the main components of the baseimage:
  * [S6-overlay], a process supervisor for containers.

[S6-overlay]: https://github.com/just-containers/s6-overlay

### Versioning

Images are versioned.  Version number is in the form `MAJOR.MINOR.PATCH`, where
an increment of the:
  - MAJOR version indicates that a backwards-incompatible change has been done.
  - MINOR version indicates that functionality has been added in a backwards-compatible manner.
  - PATCH version indicates that a bug fix has been done in a backwards-compatible manner.

### Tags

For each distribution-specific image, multiple tags are available:

| Tag | Description |
|-----|-------------|
| distro-vX.Y.Z | Exact version of the image. |
| distro-vX.Y   | Latest version of a specific minor version of the image. |
| distro-vX     | Latest version of a specific major version of the image. |
| distro        | Latest version of the image. |

## Getting started
The `Dockerfile` for your application can be very simple, as only three things
are required:

  * Instructions to install the application.
  * A script that starts the application (stored at `/startapp.sh` in
    container).
  * The name of the application.

Here is an example of a docker file that would be used to run a simple web
NodeJS server.

In ``Dockerfile``:
```
# Pull base image.
FROM jlesage/baseimage:alpine-3.6

# Install http-server.
RUN apk --no-cache add nodejs-npm && \
    npm install http-server -g

# Copy the start script.
COPY startapp.sh /startapp.sh

# Set the name of the application.
ENV APP_NAME="http-server"

# Expose ports.
EXPORT 8080
```

In `startapp.sh`:
```
#!/bin/sh
exec /usr/bin/http-server
```

Then, build your docker image:

    docker build -t docker-http-server .

And run it:

    docker run --rm -p 8080:8080 docker-http-server

You should be able to access the HTTP server by opening in a web browser:

`http://[HOST IP ADDR]:8080`

## Environment Variables

Some environment variables can be set to customize the behavior of the container
and its application.  The following list give more details about them.

Environment variables can be set directly in your `Dockerfile` via the `ENV`
instruction or dynamically by adding one or more arguments `-e "<VAR>=<VALUE>"`
to the `docker run` command.

| Variable       | Description                                  | Default |
|----------------|----------------------------------------------|---------|
|`APP_NAME`| Name of the application. | `DockerApp` |
|`USER_ID`| ID of the user the application runs as.  See [User/Group IDs](#usergroup-ids) to better understand when this should be set. | `1000` |
|`GROUP_ID`| ID of the group the application runs as.  See [User/Group IDs](#usergroup-ids) to better understand when this should be set. | `1000` |
|`SUP_GROUP_IDS`| Comma-separated list of supplementary group IDs of the application. | (unset) |
|`UMASK`| Mask that controls how file permissions are set for newly created files. The value of the mask is in octal notation.  By default, this variable is not set and the default umask of `022` is used, meaning that newly created files are readable by everyone, but only writable by the owner. See the following online umask calculator: http://wintelguy.com/umask-calc.pl | (unset) |
|`TZ`| [TimeZone] of the container.  Timezone can also be set by mapping `/etc/localtime` between the host and the container. | `Etc/UTC` |
|`KEEP_APP_RUNNING`| When set to `1`, the application will be automatically restarted if it crashes or if user quits it. | `0` |
|`APP_NICENESS`| Priority at which the application should run.  A niceness value of -20 is the highest priority and 19 is the lowest priority.  By default, niceness is not set, meaning that the default niceness of 0 is used.  **NOTE**: A negative niceness (priority increase) requires additional permissions.  In this case, the container should be run with the docker option `--cap-add=SYS_NICE`. | (unset) |
|`TAKE_CONFIG_OWNERSHIP`| When set to `1`, owner and group of `/config` (including all its files and subfolders) are automatically set during container startup to `USER_ID` and `GROUP_ID` respectively. | `1` |

## Config Directory
Inside the container, the application's configuration should be stored in the
`/config` directory.


**NOTE**: By default, during the container startup, the user which runs the
application (i.e. user defined by `USER_ID`) will claim ownership of the
entire content of this directory.  This behavior can be changed via the
`TAKE_CONFIG_OWNERSHIP` environment variable.  See the
[Environment Variables](#environment-variables) section for more details.


## User/Group IDs

When using data volumes (`-v` flags), permissions issues can occur between the
host and the container.  For example, the user within the container may not
exists on the host.  This could prevent the host from properly accessing files
and folders on the shared volume.

To avoid any problem, you can specify the user the application should run as.

This is done by passing the user ID and group ID to the container via the
`USER_ID` and `GROUP_ID` environment variables.

To find the right IDs to use, issue the following command on the host, with the
user owning the data volume on the host:

    id <username>

Which gives an output like this one:
```
uid=1000(myuser) gid=1000(myuser) groups=1000(myuser),4(adm),24(cdrom),27(sudo),46(plugdev),113(lpadmin)
```

The value of `uid` (user ID) and `gid` (group ID) are the ones that you should
be given the container.

## Locales
The default locale of the container is set to `POSIX`.  If this cause issues
with your application, the proper locale can be set via your `Dockerfile`, by adding these two lines:
```
ENV LANG=en_US.UTF-8
RUN locale-gen en_CA.UTF-8
```

**NOTE**: Locales are not supported by `musl` C standard library on `Alpine`.
See:
  * http://wiki.musl-libc.org/wiki/Open_Issues#C_locale_conformance
  * https://github.com/gliderlabs/docker-alpine/issues/144

## Security
TBD

## Building A Container

This section provides useful tips for building containers based on this
baseimage.

### Selecting Baseimage Tag

Properly select the baseimage tag to use.  For a better control and prevent
breaking your container, use a tag for an exact version of the baseimage
(e.g. `alpine-3.6-v2.0.0`).  Using the latest version of the baseimage
(`alpine-3.6`) is not recommended, since automatically upgrading between major
versions will probably break your container build/execution.

### Referencing Linux User/Group

Reference the Linux user/group under which the application is running by its ID
(`USER_ID`/`GROUP_ID`) instead of its name.  Name could change in different
baseimage versions while the ID won't.

### Default Configuration Files

Default configuration files should be stored in `/defaults` in the container.

### Modifying Baseimage Content

Try to minimize modifications to files provided by the baseimage.  This
minimizes to risk of breaking your container after using a new baseimage
version.

### Application's Data

Applications often needs to write configuration, data, logs, etc.  Always
make sure they are all written under `/config`.  This directory is a volume
intended to be mapped to a folder on the host.  The goal is to write stuff
outside the container and keep these data persistent.

### $HOME Environment Variable

The application is run under a user having its own UID.  This user can't be used
to login with, has no password, no valid login shell and no home directory.  It
is effectively a kind of user used by daemons.

Thus, by default, the `$HOME` environment variable is not set.  While this
should be fine in most case, some applications may expect the `$HOME`
environment variable to be set (since normally the application is run by a
logged user) and may not behave correctly otherwise.

To make the application happy, the home directory can be set at the beginning
of the `startapp.sh` script:
```
export HOME=/config
```

Adjust the location of the home directory to fit your needs.  However, if the
application uses the home directory to write stuff, make sure it is done in a
volume mapped to the host (e.g. `/config`),

Note that the same technique can be used by services, by exporting the home
directory into their `run` script.

### Service Dependencies

When running multiple services, service `srvB` may need to start only after
service `SrvA`.

This can be easily achieved by adding a call to `s6-waitdeps` at the beginning
of the `run` script of the service.

Dependencies are defined by touching file in the service's directory, its name
being the name of the dependent service with the `.dep` extension.  For example,
touching the file:

    /etc/services.d/srvB/srvA.dep

indicates that service `srvB` depends on service `srvA`.

### Service Readiness

By default, a service is considered as ready when it has been running for 1
second (as determined by its supervisor).

A custom way of determining service readiness can be implemented in a script
placed in the service's directory (e.g. `/etc/services.d/myservice/`).  The
script should be named `ready` and should have execution permission.

Note that this is used only when service dependencies are used.

### S6 Overlay Documentation
* Make sure to read the [S6 overlay documentation].  It contains information
that can help building your image.  For example, the S6 overlay allows you to
easily add initialization scripts and services.

[S6 overlay documentation]: https://github.com/just-containers/s6-overlay/blob/master/README.md

[TimeZone]: http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
