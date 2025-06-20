# A minimal docker baseimage to ease creation of long-lived application containers
[![Release](https://img.shields.io/github/release/jlesage/docker-baseimage.svg?logo=github&style=for-the-badge)](https://github.com/jlesage/docker-baseimage/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/jlesage/docker-baseimage/build-baseimage.yml?logo=github&branch=master&style=for-the-badge)](https://github.com/jlesage/docker-baseimage/actions/workflows/build-baseimage.yml)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg?style=for-the-badge)](https://paypal.me/JocelynLeSage)

This is a Docker baseimage designed to simplify the creation of containers
for any long-lived application.

## Images

This baseimage is available for multiple Linux distributions:

| Base Distribution  | Docker Image Base Tag | Size |
|--------------------|-----------------------|------|
| [Alpine 3.16]      | alpine-3.16           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.16-v3?style=for-the-badge)](#)  |
| [Alpine 3.17]      | alpine-3.17           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.17-v3?style=for-the-badge)](#)  |
| [Alpine 3.18]      | alpine-3.18           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.18-v3?style=for-the-badge)](#)  |
| [Alpine 3.19]      | alpine-3.19           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.19-v3?style=for-the-badge)](#)  |
| [Alpine 3.20]      | alpine-3.20           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.20-v3?style=for-the-badge)](#)  |
| [Alpine 3.21]      | alpine-3.21           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.21-v3?style=for-the-badge)](#)  |
| [Alpine 3.22]      | alpine-3.22           | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/alpine-3.22-v3?style=for-the-badge)](#)  |
| [Debian 10]        | debian-10             | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/debian-10-v3?style=for-the-badge)](#)    |
| [Debian 11]        | debian-11             | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/debian-11-v3?style=for-the-badge)](#)    |
| [Ubuntu 16.04 LTS] | ubuntu-16.04          | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-16.04-v3?style=for-the-badge)](#) |
| [Ubuntu 18.04 LTS] | ubuntu-18.04          | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-18.04-v3?style=for-the-badge)](#) |
| [Ubuntu 20.04 LTS] | ubuntu-20.04          | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-20.04-v3?style=for-the-badge)](#) |
| [Ubuntu 22.04 LTS] | ubuntu-22.04          | [![](https://img.shields.io/docker/image-size/jlesage/baseimage/ubuntu-22.04-v3?style=for-the-badge)](#) |

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


## Getting started

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

## Documentation

Full documentation is available at https://github.com/jlesage/docker-baseimage.

