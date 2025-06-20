# A minimal docker baseimage to ease creation of long-lived application containers
[![Release](https://img.shields.io/github/release/jlesage/docker-baseimage.svg?logo=github&style=for-the-badge)](https://github.com/jlesage/docker-baseimage/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/jlesage/docker-baseimage/build-baseimage.yml?logo=github&branch=master&style=for-the-badge)](https://github.com/jlesage/docker-baseimage/actions/workflows/build-baseimage.yml)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg?style=for-the-badge)](https://paypal.me/JocelynLeSage)

This is a Docker baseimage designed to simplify the creation of containers
for any long-lived application.

## Table of Contents

   * [Images](#images)
      * [Versioning](#versioning)
      * [Content](#content)
   * [Getting Started](#getting-started)
   * [Using the Baseimage](#using-the-baseimage)
      * [Selecting a Baseimage](#selecting-a-baseimage)
      * [Container Startup Sequence](#container-startup-sequence)
      * [Container Shutdown Sequence](#container-shutdown-sequence)
      * [Environment Variables](#environment-variables)
         * [Public Environment Variables](#public-environment-variables)
         * [Internal Environment Variables](#internal-environment-variables)
         * [Adding/Removing Internal Environment Variables](#addingremoving-internal-environment-variables)
         * [Availability](#availability)
         * [Docker Secrets](#docker-secrets)
      * [User/Group IDs](#usergroup-ids)
      * [Locales](#locales)
      * [Initialization Scripts](#initialization-scripts)
      * [Finalization Scripts](#finalization-scripts)
      * [Services](#services)
         * [Service Group](#service-group)
         * [Default Service](#default-service)
         * [Service Readiness](#service-readiness)
      * [Configuration Directory](#configuration-directory)
         * [Application's Data Directories](#applications-data-directories)
      * [Container Log](#container-log)
      * [Logrotate](#logrotate)
      * [Log Monitor](#log-monitor)
         * [Notification Definition](#notification-definition)
         * [Notification Backend](#notification-backend)
      * [Helpers](#helpers)
         * [Adding/Removing Packages](#addingremoving-packages)
         * [Modifying Files with Sed](#modifying-files-with-sed)
         * [Evaluating Boolean Values](#evaluating-boolean-values)
         * [Taking Ownership of a Directory](#taking-ownership-of-a-directory)
         * [Setting Internal Environment Variables](#setting-internal-environment-variables)
      * [Tips and Best Practices](#tips-and-best-practices)
         * [Do Not Modify Baseimage Content](#do-not-modify-baseimage-content)
         * [Default Configuration Files](#default-configuration-files)
         * [The $HOME Variable](#the-home-variable)
         * [Referencing Linux User/Group](#referencing-linux-usergroup)
         * [Using rootfs Directory](#using-rootfs-directory)
         * [Adaptations from Version 2.x](#adaptations-from-version-2x)

## Images

This baseimage is available for multiple Linux distributions:

| Linux Distribution | Docker Image Tag      | Size |
|--------------------|-----------------------|------|
| [Alpine 3.16]      | alpine-3.16-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.16-v3?style=for-the-badge)](#)  |
| [Alpine 3.17]      | alpine-3.17-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.17-v3?style=for-the-badge)](#)  |
| [Alpine 3.18]      | alpine-3.18-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.18-v3?style=for-the-badge)](#)  |
| [Alpine 3.19]      | alpine-3.19-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.19-v3?style=for-the-badge)](#)  |
| [Alpine 3.20]      | alpine-3.20-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.20-v3?style=for-the-badge)](#)  |
| [Alpine 3.21]      | alpine-3.21-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.21-v3?style=for-the-badge)](#)  |
| [Alpine 3.22]      | alpine-3.22-vX.Y.Z    | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.22-v3?style=for-the-badge)](#)  |
| [Debian 10]        | debian-10-vX.Y.Z      | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/debian-10-v3?style=for-the-badge)](#)    |
| [Debian 11]        | debian-11-vX.Y.Z      | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/debian-11-v3?style=for-the-badge)](#)    |
| [Debian 12]        | debian-12-vX.Y.Z      | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/debian-12-v3?style=for-the-badge)](#)    |
| [Ubuntu 16.04 LTS] | ubuntu-16.04-vX.Y.Z   | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-16.04-v3?style=for-the-badge)](#) |
| [Ubuntu 18.04 LTS] | ubuntu-18.04-vX.Y.Z   | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-18.04-v3?style=for-the-badge)](#) |
| [Ubuntu 20.04 LTS] | ubuntu-20.04-vX.Y.Z   | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-20.04-v3?style=for-the-badge)](#) |
| [Ubuntu 22.04 LTS] | ubuntu-22.04-vX.Y.Z   | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-22.04-v3?style=for-the-badge)](#) |
| [Ubuntu 24.04 LTS] | ubuntu-24.04-vX.Y.Z   | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-24.04-v3?style=for-the-badge)](#) |

Docker image tags follow this structure:

| Tag           | Description                                              |
|---------------|----------------------------------------------------------|
| distro-vX.Y.Z | Exact version of the image.                              |
| distro-vX.Y   | Latest version of a specific minor version of the image. |
| distro-vX     | Latest version of a specific major version of the image. |

View all available tags on [Docker Hub] or check the [Releases] page for version
details.

[Alpine 3.16]: https://alpinelinux.org/posts/Alpine-3.16.0-released.html
[Alpine 3.17]: https://alpinelinux.org/posts/Alpine-3.17.0-released.html
[Alpine 3.18]: https://alpinelinux.org/posts/Alpine-3.18.0-released.html
[Alpine 3.19]: https://alpinelinux.org/posts/Alpine-3.19.0-released.html
[Alpine 3.20]: https://alpinelinux.org/posts/Alpine-3.20.0-released.html
[Alpine 3.21]: https://alpinelinux.org/posts/Alpine-3.21.0-released.html
[Alpine 3.22]: https://alpinelinux.org/posts/Alpine-3.22.0-released.html
[Debian 10]: https://www.debian.org/releases/buster/
[Debian 11]: https://www.debian.org/releases/bullseye/
[Debian 12]: https://www.debian.org/releases/bookworm/
[Ubuntu 16.04 LTS]: http://releases.ubuntu.com/16.04/
[Ubuntu 18.04 LTS]: http://releases.ubuntu.com/18.04/
[Ubuntu 20.04 LTS]: http://releases.ubuntu.com/20.04/
[Ubuntu 22.04 LTS]: http://releases.ubuntu.com/22.04/
[Ubuntu 24.04 LTS]: http://releases.ubuntu.com/24.04/

[Releases]: https://github.com/jlesage/docker-baseimage/releases
[Docker Hub]: https://hub.docker.com/r/jlesage/baseimage/tags

### Versioning

Images adhere to [semantic versioning]. The version format is
`MAJOR.MINOR.PATCH`, where an increment in the:

  - `MAJOR` version indicates a backward-incompatible change.
  - `MINOR` version indicates functionality added in a backward-compatible manner.
  - `PATCH` version indicates a bug fix in a backward-compatible manner.

[semantic versioning]: https://semver.org

### Content

The baseimage includes the following key components:

  - An initialization system for container startup.
  - A process supervisor with proper PID 1 functionality (e.g., process
    reaping).
  - Tools to simplify container creation.
  - An environment optimized for Dockerized applications.


## Getting Started

Creating a Docker container for an application using this baseimage is
straightforward. You need at least three components in your `Dockerfile`:

  - Instructions to install the application and its dependencies.
  - A script to start the application, stored at `/startapp.sh` in the
    container.
  - The name of the application.

Below is an example of a `Dockerfile` and `startapp.sh` for running a simple
NodeJS web server:

**Dockerfile**:

```dockerfile
# Pull the baseimage.
FROM jlesage/baseimage:alpine-3.19-v3

# Install http-server.
RUN add-pkg nodejs-npm && \
    npm install http-server -g

# Copy the start script.
COPY startapp.sh /startapp.sh

# Set the application name.
RUN set-cont-env APP_NAME "http-server"

# Expose ports.
EXPOSE 8080
```

**startapp.sh**:

```shell
#!/bin/sh
exec /usr/bin/http-server
```

Make the script executable:

```shell
chmod +x startapp.sh
```

Build the Docker image:

```shell
docker build -t docker-http-server .
```

Run the container, mapping ports for web  access:

```shell
docker run --rm -p 8080:8080 docker-http-server
```

Access the HTTP server via a web browser at:

```
http://<HOST_IP_ADDR>:8080
```

## Using the Baseimage

### Selecting a Baseimage

Using a baseimage based on Alpine Linux is recommended, not only for its compact
size, but also because Alpine Linux, built with [musl] and [BusyBox], is
designed for security, simplicity, and resource efficiency.

However, integrating applications not available in Alpine's software repository
or those lacking source code can be challenging. Alpine Linux uses the [musl] C
standard library instead of the GNU C library ([glibc]), which most applications
are built against. Compatibility between these libraries is limited.

Alternatively, Debian and Ubuntu images are well-known Linux distributions
offering excellent compatibility with existing applications.

[musl]: https://www.musl-libc.org
[BusyBox]: https://busybox.net
[glibc]: https://www.gnu.org/software/libc/

### Container Startup Sequence

When the container starts, it executes the following steps:

  - The init process (`/init`) is invoked.
  - Internal environment variables are loaded from `/etc/cont-env.d`.
  - Initialization scripts under `/etc/cont-init.d` are executed in alphabetical
    order.
  - Control is given to the process supervisor.
  - The service group `/etc/services.d/default` is loaded, along with its
    dependencies.
  - Services are started in the correct order.
  - The container is now fully started.

### Container Shutdown Sequence

The container can shut down in two scenarios:

  1. When the implemented application terminates.
  2. When Docker initiates a shutdown (e.g., via the `docker stop` command).

In both cases, the shutdown sequence is as follows:

  - All services are terminated in reverse order.
  - If processes are still running, a `SIGTERM` signal is sent to all.
  - After 5 seconds, remaining processes are forcefully terminated via the
    `SIGKILL` signal.
  - The process supervisor executes the exit script (`/etc/services.d/exit`).
  - The exit script runs finalization scripts in `/etc/cont-finish.d/` in
    alphabetical order.
  - The container is fully stopped.

### Environment Variables

Environment variables enable customization of the container and application
behavior. They are categorized into two types:

  - **Public**: These variables are intended for users of the container. They
    provide a way to configure it and are declared in the `Dockerfile` using the
    `ENV` instruction. Their values can be set during container creation with
    the `-e "<VAR>=<VALUE>"` argument of the `docker run` command. Many Docker
    container management systems use these variables to provide configuration
    options to users.

  - **Internal**: These variables are not meant to be modified by users. They
    are useful for the application but not intended for external configuration.

> [!NOTE]
> If a variable is defined as both internal and public, the public value takes
> precedence.

#### Public Environment Variables

The baseimage provides the following public environment variables:

| Variable       | Description                                  | Default |
|----------------|----------------------------------------------|---------|
|`USER_ID`| ID of the user the application runs as. See [User/Group IDs](#usergroup-ids) for details. | `1000` |
|`GROUP_ID`| ID of the group the application runs as. See [User/Group IDs](#usergroup-ids) for details. | `1000` |
|`SUP_GROUP_IDS`| Comma-separated list of supplementary group IDs for the application. | (no value) |
|`UMASK`| Mask controlling permissions for newly created files and folders, specified in octal notation. By default, `0022` ensures files and folders are readable by all but writable only by the owner. See the umask calculator at http://wintelguy.com/umask-calc.pl. | `0022` |
|`LANG`| Sets the [locale](https://en.wikipedia.org/wiki/Locale_(computer_software)), defining the application's language, if supported. Format is `language[_territory][.codeset]`, where language is an [ISO 639 language code](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes), territory is an [ISO 3166 country code](https://en.wikipedia.org/wiki/ISO_3166-1#Current_codes), and codeset is a character set, like `UTF-8`. For example, Australian English using UTF-8 is `en_AU.UTF-8`. | `en_US.UTF-8` |
|`TZ`| [TimeZone](http://en.wikipedia.org/wiki/List_of_tz_database_time_zones) used by the container. The timezone can also be set by mapping `/etc/localtime` between the host and the container. | `Etc/UTC` |
|`KEEP_APP_RUNNING`| When set to `1`, the application is automatically restarted if it crashes or terminates. | `0` |
|`APP_NICENESS`| Priority at which the application runs. A niceness value of -20 is the highest, 19 is the lowest and 0 the default. **NOTE**: A negative niceness (priority increase) requires additional permissions. The container must be run with the Docker option `--cap-add=SYS_NICE`. | `0` |
|`INSTALL_PACKAGES`| Space-separated list of packages to install during container startup. Packages are installed from the repository of the Linux distribution the container is based on. | (no value) |
|`PACKAGES_MIRROR`| Mirror of the repository to use when installing packages. | (no value) |
|`CONTAINER_DEBUG`| When set to `1`, enables debug logging. | `0` |

#### Internal Environment Variables

The baseimage provides the following internal environment variables:

| Variable       | Description                                  | Default |
|----------------|----------------------------------------------|---------|
|`APP_NAME`| Name of the implemented application. | `DockerApp` |
|`APP_VERSION`| Version of the implemented application. | (no value) |
|`DOCKER_IMAGE_VERSION`| Version of the Docker image that implements the application. | (no value) |
|`DOCKER_IMAGE_PLATFORM`| Platform (OS/CPU architecture) of the Docker image that implements the application. | (no value) |
|`HOME`| Home directory. | (no value) |
|`XDG_CONFIG_HOME`| Defines the base directory for user-specific configuration files. | `/config/xdg/config` |
|`XDG_DATA_HOME`| Defines the base directory for user-specific data files. | `/config/xdg/data` |
|`XDG_CACHE_HOME`| Defines the base directory for user-specific non-essential data files. | `/config/xdg/cache` |
|`TAKE_CONFIG_OWNERSHIP`| When set to `0`, ownership of the `/config` directory's contents is not taken during container startup. | `1` |
|`INSTALL_PACKAGES_INTERNAL`| Space-separated list of packages to install during container startup. Packages are installed from the repository of the Linux distribution the container is based on. | (no value) |
|`SUP_GROUP_IDS_INTERNAL`| Comma-separated list of supplementary group IDs for the application, merged with those supplied by `SUP_GROUP_IDS`. | (no value) |
|`SERVICES_GRACETIME`| During container shutdown, defines the time (in milliseconds) allowed for services to gracefully terminate before sending the SIGKILL signal to all. | `5000` |

#### Adding/Removing Internal Environment Variables

Internal environment variables are defined by creating a file in
`/etc/cont-env.d/` inside the container, where the file's name is the variable
name and its content is the value.

If the file is executable, the init process executes it, and the environment
variable's value is taken from its standard output.

> [!NOTE]
> If the program exits with return code `100`, the environment variable is not
> set (distinct from being set with an empty value).

> [!NOTE]
> Any stderr output from the program is redirected to the container's log.

> [!NOTE]
> The `set-cont-env` helper can be used to set internal environment variables
> from the Dockerfile.

#### Availability

Public environment variables are defined during container creation and are
available to scripts and services as soon as the container starts.

Internal environment variables are loaded during container startup, before
initialization scripts and services run, ensuring their availability.

#### Docker Secrets

[Docker secrets](https://docs.docker.com/engine/swarm/secrets/) is a
functionality available to swarm services that offers a secure way to store
sensitive information such as usernames, passwords, etc.

This baseimage automatically exports Docker secrets as environment variables if
they follow the naming convention `CONT_ENV_<environment variable name>`.

For example, a secret named `CONT_ENV_MY_PASSWORD` creates the environment
variable `MY_PASSWORD` with the secret's content.

### User/Group IDs

When mapping data volumes (using the `-v` flag of the `docker run` command),
permission issues may arise between the host and the container. Files and
folders in a data volume are owned by a user, which may differ from the user
running the application. Depending on permissions, this could prevent the
container from accessing the shared volume.

To avoid this, specify the user the application should run as using the
`USER_ID` and `GROUP_ID` environment variables.

To find the appropriate IDs, run the following command on the host for the user
owning the data volume:

```shell
id <username>
```

This produces output like:

```
uid=1000(myuser) gid=1000(myuser) groups=1000(myuser),4(adm),24(cdrom),27(sudo),46(plugdev),113(lpadmin)
```

Use the `uid` (user ID) and `gid` (group ID) values to configure the container.

### Locales

The default locale of the container is `POSIX`. If this causes issues with your
application, install the appropriate locale. For example, to set the locale to
`en_US.UTF-8`, add these instructions to your `Dockerfile`:

```dockerfile
RUN \
    add-pkg locales && \
    sed-patch 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen
ENV LANG=en_US.UTF-8
```

> [!NOTE]
> Locales are not supported by the musl C standard library on Alpine Linux. See:
>   - http://wiki.musl-libc.org/wiki/Open_Issues#C_locale_conformance
>   - https://github.com/gliderlabs/docker-alpine/issues/144

### Initialization Scripts

During container startup, initialization scripts in `/etc/cont-init.d/` are
executed in alphabetical order. They are executed before starting services.

To ensure predictable execution, name scripts using the format `XX-name.sh`,
where `XX` is a sequence number.

The baseimage uses the ranges:

  - 10-29
  - 70-89

Unless specific needs require otherwise, containers built with this baseimage
should use the range 50-59.

### Finalization Scripts

Finalization scripts in `/etc/cont-finish.d/` are executed in alphabetical order
during container shutdown, after all services have stopped.

### Services

Services are background programs managed by the process supervisor, which can be
configured to restart automatically if they terminate.

Services are defined under `/etc/services.d/` in the container. Each service has
its own directory containing files that define its behavior.

The content of these files provides the configuration settings. If a file is
executable, the process supervisor runs it, using its output as the setting's
value.

| File                   | Type             | Description | Default |
|------------------------|------------------|-------------|---------|
| run                    | Program          | The program to run. | N/A |
| is_ready               | Program          | Program to verify if the service is ready. It should exit with code `0` when ready. The service's PID is passed as a parameter. | N/A |
| kill                   | Program          | Program to run when the service needs to be killed. The service's PID is passed as a parameter. The `SIGTERM` signal is sent to the service after execution. | N/A |
| finish                 | Program          | Program invoked when the service terminates. The service's exit code is passed as a parameter. | N/A |
| params                 | String           | Parameters for the service's program, one per line. | No parameter |
| environment            | String           | Environment for the service, with variables in the form `var=value`, one per line. | Environment untouched |
| environment_extra      | String           | Additional variables to add to the environment of the service, one per line, in the form `key=value`. | No extra variable |
| respawn                | Boolean          | Whether the process should be respawned when it terminates. | `FALSE`  |
| sync                   | Boolean          | Whether the process supervisor waits until the service ends. Mutually exclusive with `respawn`. | `FALSE` |
| ready_timeout          | Unsigned integer | Maximum time (in milliseconds) to wait for the service to be ready. | `10000` |
| interval               | Interval         | Interval, in seconds, at which the service should be executed. Mutually exclusive with `respawn`. | No interval |
| uid                    | Unsigned integer | User ID under which the service runs. | `$USER_ID` |
| gid                    | Unsigned integer | Group ID under which the service runs. | `$GROUP_ID` |
| sgid                   | Unsigned integer | List of supplementary group IDs for the service, one per line. | Empty list |
| umask                  | Octal integer    | Umask value (in octal notation) for the service. | `0022` |
| priority               | Signed integer   | Priority at which the service runs. A niceness value of -20 is the highest, and 19 is the lowest. | `0` |
| workdir                | String           | Working directory of the service. | Service's directory path  |
| ignore_failure         | Boolean          | If set, failure to start the service does not prevent the container from starting. | `FALSE` |
| shutdown_on_terminate  | Boolean          | Indicates the container should shut down when the service terminates. | `FALSE` |
| min_running_time       | Unsigned integer | Minimum time (in milliseconds) the service must run before being considered ready. | `500` |
| disabled               | Boolean          | Indicates the service is disabled and will not be loaded or started. | `FALSE` |
| \<service\>.dep        | Boolean          | Indicates the service depends on another service. For example, `srvB.dep` means `srvB` must start first. | N/A |

The following table provides details about some value types:

| Type     | Description |
|----------|-------------|
| Program  | An executable binary, script, or symbolic link to the program to run. The file must have execute permission. |
| Boolean  | A boolean value. A *true* value can be `1`, `true`, `on`, `yes`, `y`, `enable`, or `enabled`. A *false* value can be `0`, `false`, `off`, `no`, `n`, `disable`, or `disabled`. Values are case -insensitive. An empty file indicates a *true* value (i.e., the file can be "touched"). |
| Interval | An unsigned integer value. Also accepted (case-insensitive): `yearly`, `monthly`, `weekly`, `daily`, `hourly`. |

#### Service Group

A service group is a service definition without a `run` program. The process
supervisor loads only its dependencies.

#### Default Service

During startup, the process supervisor first loads the `default` service group,
which includes dependencies for services that should be started and are not
dependencies of the `app` service.

#### Service Readiness

By default, a service is considered ready once it has launched successfully and
run for a minimum time (500ms by default).

This behavior can be adjusted by:
  - Setting the minimum running time using the `min_running_time` file in the
    service's directory.
  - Adding an `is_ready` program to the service's directory, along with a
    `ready_timeout` file to specify the maximum wait time for readiness.

### Configuration Directory

Applications often need to write configuration, data, states, logs, etc. Inside
the container, such data should be stored under the `/config` directory.

This directory is intended to be mapped to a folder on the host to ensure data
persistence.

> [!NOTE]
> During container startup, ownership of this folder and its contents is set to
> ensure accessibility by the user specified via `USER_ID` and `GROUP_ID`. This
> behavior can be modified using the `TAKE_CONFIG_OWNERSHIP` internal
> environment variable.

#### Application's Data Directories

Many applications use environment variables defined by the
[XDG Base Directory Specification] to determine where to store various data. The
baseimage sets these variables to reside under `/config/`:

  - XDG_DATA_HOME=/config/xdg/data
  - XDG_CONFIG_HOME=/config/xdg/config
  - XDG_CACHE_HOME=/config/xdg/cache
  - XDG_STATE_HOME=/config/xdg/state

[XDG Base Directory Specification]: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

### Container Log

Outputs (both standard output and standard error) of scripts and programs
executed during the init process and by the process supervisor are available in
the container's log. The container log can be viewed with the command
`docker logs <name of the container>`.

To facilitate log consultationg, all messages are prefixed with the name of the
service or script.

It is advisable to limit the amount of information written to this log. If a
program's output is too verbose, redirect it to a file. For example, the
following `run` file of a service redirects standard output and standard error
to different files:

```shell
#!/bin/sh
exec /usr/bin/my_service > /config/log/my_service_out.log 2> /config/log/my_service_err.log
```

### Logrotate

The baseimage includes `logrotate`, a utility for rotating and compressing log
files, which runs daily via a service. The service is disabled if no log files
are configured.

To enable rotation/compression for a log file, add a configuration file to
`/etc/cont-logrotate.d` within the container. This configuration defines how to
handle the log file.

Example configuration at `/etc/cont-logrotate.d/myapp`:

```
/config/log/myapp.log {
    minsize 1M
}
```

This file can override default parameters defined at
`/opt/base/etc/logrotate.conf` in the container. By default:
  - Log files are rotated weekly.
  - Four weeks of backlogs are kept.
  - Rotated logs are compressed with gzip.
  - Dates are used as suffixes for rotated logs.

For details on `logrotate` configuration files, see
https://linux.die.net/man/8/logrotate.

### Log Monitor

The baseimage includes a log monitor that sends notifications when specific
messages are detected in log or status files.

The system has two main components:
  - **Notification definitions**: Describe notification properties (title,
    message, severity, etc.), the triggering condition (filtering function), and
    the monitored file(s).
  - **Backends (targets)**: When a matching string is found, a notification is
    sent to one or more backends, which can log to the container, a file, or an
    external service.

Two types of files can be monitored:
  - **Log files**: Files with new content appended.
  - **Status files**: Files whose entire content is periodically
    refreshed/overwritten.

#### Notification Definition

A notification definition consists of multiple files in a directory under
`/etc/logmonitor/notifications.d` within the container. For example, the
definition for `MYNOTIF` is stored in
`/etc/logmonitor/notifications.d/MYNOTIF/`.

The following table describes files part of the definition:

| File     | Mandatory  | Description |
|----------|------------|-------------|
| `filter` | Yes        | Program (script or binary with executable permission) to filter log file messages. It is invoked with a log line as an argument and should exit with `0` on a match. Other values indicate no match. |
| `title`  | Yes        | File containing the notification title. For dynamic content, it can be a program (script or binary with executable permission) invoked with the matched line, using its output as the title. |
| `desc`   | Yes        | File containing the notification description or message. For dynamic content, it can be a program (script or binary with executable permission) invoked with the matched log line, using its output as the description. |
| `level`  | Yes        | File containing the notification's severity level (`ERROR`, `WARNING`, or `INFO`). For dynamic content, it can be a program (script or binary with executable permission) invoked with the matched log line, using its output as the severity. |
| `source` | Yes        | File containing the absolute path(s) to monitored file(s), one per line. Prepend `status:` for status file; `log:` or no prefix indicates a log file. |

#### Notification Backend

A notification backend is defined in a directory under
`/etc/cont-logmonitor/targets.d`. For example, the `STDOUT` backend is in
`/etc/cont-logmonitor/target.d/STDOUT/`.

The following table describes the files:

| File         | Mandatory  | Description |
|--------------|------------|-------------|
| `send`       | Yes        | Program (script or binary with executable permission) that sends the notification, invoked with the notification's title, description, and severity level as arguments. |
| `debouncing` | No         | File containing the minimum time (in seconds) before sending the same notification again. A value of `0` means the notification is sent once. If missing, no debouncing occurs. |

The baseimage includes these notification backends:

| Backend  | Description | Debouncing time |
|----------|-------------|-----------------|
| `stdout` | Displays a message to standard output, visible in the container's log, in the format `{LEVEL}: {TITLE} {MESSAGE}`. | 21 600s (6 hours) |

### Helpers

The baseimage includes helpers that can be used when building a container or
during its execution.

#### Adding/Removing Packages

Use the `add-pkg` and `del-pkg` helpers to add or remove packages, ensuring
proper cleanup to minimize container size.

These tools allow temporary installation of a group of packages (virtual
package)  using the `--virtual NAME` parameter, enabling later removal with
`del-pkg NAME`. Pre-installed packages are ignored and not removed.

Example in a `Dockerfile` for compiling a project:

```dockerfile
RUN \
    add-pkg --virtual build-dependencies build-base cmake git && \
    git clone https://myproject.com/myproject.git && \
    make -C myproject && \
    make -C myproject install && \
    del-pkg build-dependencies
```

If `git` was already installed before adding the virtual package,
`del-pkg build-dependencies` will not remove it.

#### Modifying Files with Sed

The `sed` tool is useful for modifying files during container builds, but it
does not indicate whether changes were made. The `sed-patch` helper provides
patch-like behavior, failing if the `sed` expression does not modify the file:

```shell
sed-patch [SED_OPTIONS]... SED_EXPRESSION FILE
```

Note that the sed option `-i` (edit files in place) is already supplied by the
helper.

Example in a `Dockerfile`:

```dockerfile
RUN sed-patch 's/Replace this/By this/' /etc/myfile
```

If the expression does not change `/etc/myfile`, the command fails, halting the
Docker build.

#### Evaluating Boolean Values

Environment variables are often used to store boolean values. Use
`is-bool-value-true` and `is-bool-value-false` helpers to check these values.

The following values are considered "true":
  - `1`
  - `true`
  - `yes`
  - `enabled`
  - `enable`
  - `on`

The following values are considered "false":
  - `0`
  - `false`
  - `no`
  - `disabled`
  - `disable`
  - `off`

Example to check if `CONTAINER_DEBUG` is true:

```shell
if is-bool-value-true "${CONTAINER_DEBUG:-0}"; then
    # Do something...
fi
```

#### Taking Ownership of a Directory

The `take-ownership` helper recursively sets the user ID and group ID of a
directory and all its files and subdirectories.

This helper is well-suited for scenarios where the directory is mapped to the
host. If the directory is a network share on the host, setting/changing
ownership via `chown` can fail. The helper handles this by ignoring the failure
if a write test is positive.

For example, the following command takes ownership of `/config`, automatically
using the user and group IDs from the `USER_ID` and `GROUP_ID` environment
variables:

```shell
take-ownership /config
```

User and group IDs can also be specified explicitly. For example, to set
ownership to user ID `99` and group ID `100`:

```shell
take-ownership /config 99 100
```

#### Setting Internal Environment Variables

The `set-cont-env` helper sets internal environment variables from the
`Dockerfile`.

Example to set `APP_NAME`:

```dockerfile
RUN set-cont-env APP_NAME "http-server"
```

This creates the environment variable file under `/etc/cont-env.d`.


### Tips and Best Practices

#### Do Not Modify Baseimage Content

Avoid modifying files provided by the baseimage to minimize issues when
upgrading to newer versions.

#### Default Configuration Files

Retaining the original version of application configuration files is often
helpful. This allows an initialization script to modify a file based on its
original version.

These original files, also called default files, should be stored under the
`/defaults` directory inside the container.

#### The $HOME Variable

The application runs under a Linux user with a specified ID, without login
capability, password, valid shell, or home directory, similar to a daemon user.

By default, the `$HOME` environment variable is unset. Some applications expect
`$HOME` to be set and may not function correctly otherwise.

To address this, set the home directory in the `startapp.sh` script:

```shell
export HOME=/config
```

Adjust the location as needed. If the application writes to the home directory,
use a directory mapped to the host (like `/config`).

This technique can also be applied to services by setting the home directory in
their `run` script.

#### Referencing Linux User/Group

Reference the Linux user/group running the application via:
  - Their IDs, specified by `USER_ID`/`GROUP_ID` environment variables.
  - The `app` user/group, set up during startup to match `USER_ID`/`GROUP_ID`.

#### Using `rootfs` Directory

Store files to be copied into the container in the `rootfs` directory in your
source tree, mirroring the container's structure. For example,
`/etc/cont-init.d/my-init.sh` in the container should be
`rootfs/etc/cont-init.d/my-init.sh` in your source tree.

Copy all files with a single `Dockerfile` command:

```dockerfile
COPY rootfs/ /
```

#### Adaptations from Version 2.x
When updating from version 3.x, consider the following:

  - Review exposed environment variables to categorize them as public or
    internal. See [Environment Variables](#environment-variables).
  - Rename initialization scripts to follow the `XX-name.sh` format. See
    [Initialization Scripts](#initialization-scripts).
  - Adjust service parameters/definitions for the new system. See
    [Services](#services).
  - Ensure no scripts use `with-contenv` in their shebang (e.g., in init
    scripts).
  - Set `APP_VERSION` and `DOCKER_IMAGE_VERSION` internal environment variables
    if needed.

