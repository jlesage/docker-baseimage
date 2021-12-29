# A minimal docker baseimage to ease creation of long-lived application containers
[![Docker Automated build](https://img.shields.io/docker/automated/jlesage/baseimage.svg)](https://hub.docker.com/r/jlesage/baseimage/) [![Build Status](https://travis-ci.org/jlesage/docker-baseimage.svg?branch=master)](https://travis-ci.org/jlesage/docker-baseimage)

This is a docker baseimage that can be used to create containers for any
long-lived application.

## Table of Content

   * [A minimal docker baseimage to ease creation of long-lived application containers](#a-minimal-docker-baseimage-to-ease-creation-of-long-lived-application-containers)
      * [Table of Content](#table-of-content)
      * [Images](#images)
         * [Content](#content)
         * [Versioning](#versioning)
         * [Tags](#tags)
      * [Getting started](#getting-started)
      * [Environment Variables](#environment-variables)
      * [Config Directory](#config-directory)
      * [User/Group IDs](#usergroup-ids)
      * [Locales](#locales)
      * [Building A Container](#building-a-container)
         * [Selecting Baseimage Tag](#selecting-baseimage-tag)
         * [Referencing Linux User/Group](#referencing-linux-usergroup)
         * [Default Configuration Files](#default-configuration-files)
         * [Adding/Removing Packages](#addingremoving-packages)
         * [Modifying Files With Sed](#modifying-files-with-sed)
         * [Modifying Baseimage Content](#modifying-baseimage-content)
         * [Application's Data](#applications-data)
         * [$HOME Environment Variable](#home-environment-variable)
         * [Service Dependencies](#service-dependencies)
         * [Service Readiness](#service-readiness)
         * [Log Monitor](#log-monitor)
            * [Monitored Files](#monitored-files)
            * [Notification Definition](#notification-definition)
            * [Notification Backend](#notification-backend)
         * [S6 Overlay Documentation](#s6-overlay-documentation)

## Images
Different docker images are available:

| Base distribution  | Tag              | Size |
|--------------------|------------------|------|
| [Alpine 3.10]      | alpine-3.10      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.10.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.10 "Get your own image badge on microbadger.com") |
| [Alpine 3.11]      | alpine-3.11      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.11.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.11 "Get your own image badge on microbadger.com") |
| [Alpine 3.12]      | alpine-3.12      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.12.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.12 "Get your own image badge on microbadger.com") |
| [Alpine 3.13]      | alpine-3.13      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.13.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.13 "Get your own image badge on microbadger.com") |
| [Alpine 3.14]      | alpine-3.14      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.14.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.14 "Get your own image badge on microbadger.com") |
| [Alpine 3.15]      | alpine-3.15      | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.15.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.15 "Get your own image badge on microbadger.com") |
| [Alpine 3.5]       | alpine-3.5-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.5-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.5-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.6]       | alpine-3.6-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.6-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.6-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.7]       | alpine-3.7-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.7-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.7-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.8]       | alpine-3.8-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.8-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.8-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.9]       | alpine-3.9-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.9-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.9-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.10]       | alpine-3.10-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.10-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.10-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.11]       | alpine-3.11-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.11-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.11-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.12]       | alpine-3.12-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.12-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.12-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.13]       | alpine-3.13-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.13-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.13-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.14]       | alpine-3.14-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.14-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.14-glibc "Get your own image badge on microbadger.com") |
| [Alpine 3.15]       | alpine-3.15-glibc | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:alpine-3.15-glibc.svg)](http://microbadger.com/#/images/jlesage/baseimage:alpine-3.15-glibc "Get your own image badge on microbadger.com") |
| [Debian 8]         | debian-8         | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-8.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-8/ "Get your own image badge on microbadger.com") |
| [Debian 9]         | debian-9         | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-9.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-9/ "Get your own image badge on microbadger.com") |
| [Debian 10]        | debian-10        | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-10.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-10/ "Get your own image badge on microbadger.com") |
| [Debian 11]        | debian-11        | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:debian-11.svg)](http://microbadger.com/#/images/jlesage/baseimage:debian-11/ "Get your own image badge on microbadger.com") |
| [Ubuntu 16.04 LTS] | ubuntu-16.04     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:ubuntu-16.04.svg)](http://microbadger.com/#/images/jlesage/baseimage:ubuntu-16.04 "Get your own image badge on microbadger.com") |
| [Ubuntu 18.04 LTS] | ubuntu-18.04     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:ubuntu-18.04.svg)](http://microbadger.com/#/images/jlesage/baseimage:ubuntu-18.04 "Get your own image badge on microbadger.com") |
| [Ubuntu 20.04 LTS] | ubuntu-20.04     | [![](https://images.microbadger.com/badges/image/jlesage/baseimage:ubuntu-20.04.svg)](http://microbadger.com/#/images/jlesage/baseimage:ubuntu-20.04 "Get your own image badge on microbadger.com") |

[Alpine 3.10]: https://alpinelinux.org
[Alpine 3.11]: https://alpinelinux.org
[Alpine 3.12]: https://alpinelinux.org
[Alpine 3.13]: https://alpinelinux.org
[Alpine 3.14]: https://alpinelinux.org
[Alpine 3.15]: https://alpinelinux.org
[Debian 8]: https://www.debian.org/releases/jessie/
[Debian 9]: https://www.debian.org/releases/stretch/
[Debian 10]: https://www.debian.org/releases/buster/
[Debian 11]: https://www.debian.org/releases/bullseye/
[Ubuntu 16.04 LTS]: http://releases.ubuntu.com/16.04/
[Ubuntu 18.04 LTS]: http://releases.ubuntu.com/18.04/
[Ubuntu 20.04 LTS]: http://releases.ubuntu.com/20.04/

Due to its size, an `Alpine` image is recommended.  However, it may be harder
to integrate your application (especially third party ones without source code),
because:
 1. Packages repository may not be as complete as `Ubuntu`/`Debian`.
 2. Third party applications may not support `Alpine`.
 3. The `Alpine` distribution uses the [musl] C standard library instead of
 GNU C library ([glibc]).

Note that using an `Alpine` image with glibc integrated (`alpine-3.5-glibc`
tag) may ease integration of applications.

The next choice is to use a `Debian` image.  It provides a great compatibility
and its size is smaller than the `Ubuntu` one.  Finally, if for any reason you
prefer an `Ubuntu` image, stable `LTS` versions are provided.

[musl]: https://www.musl-libc.org/
[glibc]: https://www.gnu.org/software/libc/

### Content
Here are the main components of the baseimage:
  * [S6-overlay], a process supervisor for containers.
  * Useful tools to ease container building.
  * Environment to better support dockerized applications.

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
RUN add-pkg nodejs-npm && \
    npm install http-server -g

# Copy the start script.
COPY startapp.sh /startapp.sh

# Set the name of the application.
ENV APP_NAME="http-server"

# Expose ports.
EXPOSE 8080
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
|`KEEP_APP_RUNNING`| When set to `1`, the application will be automatically restarted if it crashes or if a user quits it. | `0` |
|`APP_NICENESS`| Priority at which the application should run.  A niceness value of -20 is the highest priority and 19 is the lowest priority.  By default, niceness is not set, meaning that the default niceness of 0 is used.  **NOTE**: A negative niceness (priority increase) requires additional permissions.  In this case, the container should be run with the docker option `--cap-add=SYS_NICE`. | (unset) |
|`TAKE_CONFIG_OWNERSHIP`| When set to `1`, owner and group of `/config` (including all its files and subfolders) are automatically set during container startup to `USER_ID` and `GROUP_ID` respectively. | `1` |
|`CLEAN_TMP_DIR`| When set to `1`, all files in the `/tmp` directory are deleted during the container startup. | `1` |

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

## Building A Container

This section provides useful tips for building containers based on this
baseimage.

### Selecting Baseimage Tag

Properly select the baseimage tag to use.  For a better control and prevent
breaking your container, use a tag for an exact version of the baseimage
(e.g. `alpine-3.6-v2.0.0`).  Using the latest version of the baseimage
(`alpine-3.6`) is not recommended, since automatic upgrades between major
versions will probably break your container build/execution.

### Referencing Linux User/Group

Reference the Linux user/group under which the application is running by its ID
(`USER_ID`/`GROUP_ID`) instead of its name.  Name could change in different
baseimage versions while the ID won't.

### Default Configuration Files

Default configuration files should be stored in `/defaults` in the container.

### Adding/Removing Packages

To add or remove packages, use the helpers `add-pkg` and `del-pkg` provided by
this baseimage.  To minimze the size of the container, these tools perform
proper cleanup and make sure that no useless files are left after an addition
or removal of packages.

Also, when packages need to be added temporarily, use the `--virtual NAME`
parameter.  This allows installing missing packages and then remove them
easily using the provided `NAME` (no need to repeat given packages).  Note that
if a specified package is already installed, it will be ignored and will not be
removed automatically.

Here is an example of a command that could be added to `Dockerfile` to compile
a project:
```
RUN \
    add-pkg --virtual build-dependencies build-base cmake git && \
    # Compile your project here...
    git clone https://myproject.com/myproject.git
    ... && \
    del-pkg build-dependencies
```

Supposing that, in the example above, the `git` package is already installed
when the call to `add-pk` is performed, running `del-pkg build-dependencies`
doesn't remove it.

### Modifying Files With Sed

`sed` is a useful tool and is often used in container builds to modify files.
However, one downside of this method is that there is no easy way to determine
if `sed` actually modified the file or not.

It's for this reason that the baseimage includes a helper that gives `sed` a
"patch-like" behavior:  if the application of a sed expression results in no
change on the target file, then an error is reported.  This helper is named
`sed-patch` and has the following usage:

```
sed-patch [SED_OPT]... SED_EXPRESSION FILE
```

Note that the sed option `-i` (edit files in place) is already supplied by the
helper.

It can be used in `Dockerfile`, for example, like this:

```
RUN sed-patch 's/Replace this/By this/' /etc/myfile
```

If running this sed expression doesn't bring any change to `/etc/myfiles`, the
command fails and thus, the Docker build also.

### Modifying Baseimage Content

Try to minimize modifications to files provided by the baseimage.  This
minimizes to risk of breaking your container after using a new baseimage
version.

### Application's Data

Applications often needs to write configuration, data, logs, etc.  Always
make sure they are all written under `/config`.  This directory is a volume
intended to be mapped to a folder on the host.  The goal is to write stuff
outside the container and keep these data persistent.

A lot of applications use the environment variables defined in the
[XDG Base Directory Specification] to determine where to store
various data.  The baseimage sets these variables so they all fall under
`/config/`:

  * XDG_DATA_HOME=/config/xdg/data
  * XDG_CONFIG_HOME=/config/xdg/config
  * XDG_CACHE_HOME=/config/xdg/cache

[XDG Base Directory Specification]: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

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

Service dependencies are defined by creating a regular file in the service's
directory, its name being the name of the dependent service with the `.dep`
extension.  For example, touching the file:

    /etc/services.d/srvB/srvA.dep

indicates that service `srvB` depends on service `srvA`.

### Service Readiness

By default, a service is considered ready when the supervisor successfully
forked and executed the daemon.  However, some daemons do a lot of
initialization work before they're actually ready to serve.

Hopefully, the S6 supervisor supports [service startup notifications].  This is
a simple mechanism allowing daemons to notify the supervisor when they are
ready to serve.

While support for this mechanism can be implemented natively in the daemon, the
use of the [s6-notifyoncheck program] makes it possible for services to use the
S6 notification mechanism with any daemon.

[service startup notifications]: https://skarnet.org/software/s6/notifywhenup.html
[s6-notifyoncheck program]: https://skarnet.org/software/s6/s6-notifyoncheck.html

### Log Monitor

This baseimage include a simple log monitor.  This monitor allows sending
notification(s) when a particular message is detected in a log file.

This system has two main component: notification definitions and notifications
backends (targets).  Definitions describe properties of a notification (title,
message, severity, etc) and how it is triggered (i.e. filtering function).  Once
a matching string is found in a log file, a notification is triggered and sent
via one or more backends.  A backend can implement any functionality.  For
example, it could send the notification to the standard output, a file or an
online service.

#### Monitored Files

File(s) to be monitored can be set in the configuration file located at
`/etc/logmonitor/logmonitor.conf`.  There are two settings to look at:

  * `LOG_FILES`: List of absolute paths to log files to be monitored.  A log
    file is a file having new content appended to it.
  * `STATUS_FILES`: List of absolute paths to status files to be monitored.
    A status file doesn't have new content appended.  Instead, its whole content
    is refreshed/overwritten periodically.

#### Notification Definition

The definition of a notification consists in multiple files, stored in a
directory under `/etc/logmonitor/notifications.d`.  For example, definition of
notification `NOTIF` is found under `/etc/logmonitor/notifications.d/NOTIF/`.
The following table describe files part of the definition:

| File   | Mandatory? | Description |
|--------|------------|-------------|
|`filter`|Yes|Program (script or binary with executable permission) used to filter messages from a log file.  It is invoked by the log monitor with a single argument: a line from the log file.  On a match, the program should exit with a value of `0`.  Any other values is interpreted as non-match.|
|`title` |Yes|File containing the title of the notification.  To produce dynamic content, the file can be a program (script or binary with executable permission).  In this case, the program is invoked by the log monitor with the matched message from the log file as the single argument.  Output of the program is used as the notification's title.|
|`desc`  |Yes|File containing the description/message of the notification.  To produce dynamic content, the file can be a program (script or binary with executable permission).  In this case, the program is invoked by the log monitor with the matched message from the log file as the single argument.  Output of the program is used as the notification's description/message.|
|`level` |Yes|File containing severity level of the notification.  Valid severity level values are `ERROR`, `WARNING` or `INFO`.  To produce dynamic content, the file can be a program (script or binary with executable permission).  In this case, the program is invoked by the log monitor with the matched message from the log file as the single argument.  Output of the program is used as the notification's severity level.|

#### Notification Backend

Definition of notification backend is stored in a directory under
`/etc/logmonitor/targets.d`.  For example, definition of `STDOUT` backend is
found under `/etc/logmonitor/notifications.d/STDOUT/`.  The following table
describe files part of the definition:

| File       | Mandatory? | Description |
|------------|------------|-------------|
|`send`      |Yes|Program (script or binary with executable permission) that sends the notification.  It is invoked by the log monitor with the following notification properties as arguments: title, description/message and the severity level.
|`debouncing`|No|File containing the minimum amount time (in seconds) that must elapse before sending the same notification with the current backend.  A value of `0` means infinite (notification is sent once).  If this file is missing, no debouncing is done.|

By default, the baseimage contains the following notification backends:

|Backend |Description|Debouncing time|
|--------|-----------|---------------|
|`stdout`|Display a message to the standard output, make it visible in the container's log.  Message of the format is `{LEVEL}: {TITLE} {MESSAGE}`.|21 600s (6 hours)|

### S6 Overlay Documentation
* Make sure to read the [S6 overlay documentation].  It contains information
that can help building your image.  For example, the S6 overlay allows you to
easily add initialization scripts and services.

[S6 overlay documentation]: https://github.com/just-containers/s6-overlay/blob/master/README.md

[TimeZone]: http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
