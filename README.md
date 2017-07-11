# A minimal docker baseimage to ease creation of long-lived application containers
[![Docker Automated build](https://img.shields.io/docker/automated/jlesage/baseimage.svg)](https://hub.docker.com/r/jlesage/baseimage/) [![Build Status](https://travis-ci.org/jlesage/docker-baseimage.svg?branch=master)](https://travis-ci.org/jlesage/docker-baseimage)

This is a docker baseimage that can be used to create containers for any
long-lived application.

## Images
Different docker images are available:

| Base distribution  | Tag            | Size |
|--------------------|----------------|------|
| [Alpine 3.5]       | alpine-3.5     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.5 "Get your own image badge on microbadger.com") |
| [Alpine 3.6]       | alpine-3.6     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.6 "Get your own image badge on microbadger.com") |
| [Alpine 3.5]       | alpine-3.5-glibc     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.5-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.6]       | alpine-3.6-glibc     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.6-glibc "Get your own image badge on microbadger.com") |
| [Debian 8]         | debian-8       | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-8.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-8/ "Get your own image badge on microbadger.com") |
| [Ubuntu 16.04 LTS] | ubuntu-16.04   | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:ubuntu-16.04.svg)](http://microbadger.com/#/images/jlesage/baseimage:ubuntu-16.04 "Get your own image badge on microbadger.com") |

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

- **APP_NAME**
  Name of the application.  Default value is `DockerApp`.
- **KEEP_APP_RUNNING**
  When set to `0`, the container terminates when the application exits. When set
  to `1`, the application is automatically restarted when it terminates.
  Default is `0`.
- **TZ**
  Timezone of the container.  For example: `America/Montreal`.  The complete
  list can be found on [Wikipedia].
- **USER_ID**
  ID of the user the application run as.  Default is `1000`.  See
  [User/Group IDs](#usergroup-ids) to better understand when this should be set.
- **GROUP_ID**
  ID of the group the application run as.  Default is `1000`.  See
  [User/Group IDs](#usergroup-ids) to better understand when this should be set.
- **UMASK**
  Mask that controls how file permissions are set for newly created files. The
  value of the mask is in octal notation.  By default, this variable is not set
  and the default umask of `022` is used, meaning that newly created files are
  readable by everyone, but only writable by the owner.
  See the following online umask calculator: http://wintelguy.com/umask-calc.pl
- **SUP_GROUP_IDS**
  Comma-separated list of supplementary group IDs of the application.  By
  default, this variable is  not set.
- **APP_NICENESS**
  Priority at which the application should run.  A niceness value of −20 is
  the highest priority and 19 is the lowest priority.  By default, niceness is
  not set, meaning that the default niceness of 0 is used.
  **NOTE**: A negative niceness (priority increase) usually requires additional
  permissions.  In this case, the container should be run with the docker option
  `--cap-add=SYS_NICE`.


## Config Directory
Inside the container, the application's configuration should be stored in the
`/config` directory.

**NOTE**: During the container startup, the user which runs the application
(i.e. user defined by `USER_ID`) will claim ownership of the entire content of
this directory.

[Wikipedia]: http://en.wikipedia.org/wiki/List_of_tz_database_time_zones

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

## Notes
* Make sure to read the [S6 overlay documentation].  It contains information
that can help building your image.  For example, the S6 overlay allows you to
easily add initialization scripts and services.

[S6 overlay documentation]: https://github.com/just-containers/s6-overlay/blob/master/README.md
