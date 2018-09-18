#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

DRY_RUN=false

usage() {
    if [ "$*" ]; then
        echo "$*"
        echo
    fi

    echo "usage: $( basename $0 ) [OPTION]... README REPO

Arguments:
  README                Path to the README file to push.
  REPO                  Docker repository to push to.
  DOCKERHUB_USERNAME    Username to login to Docker Hub.
  DOCKERHUB_PASSWORD    Password to login to Docker Hub.

Options:
  -n, --dry-run         Don't push the README to docker Hub.
  -h, --help            Display this help.
"
}

# Parse arguments.
while [[ $# > 0 ]]
do
    key="$1"

    case $key in
        -h|--help)
            usage
            exit 0
            ;;
        -n|--dry-run)
            DRY_RUN=true
            ;;
        -*)
            usage "ERROR: Unknown argument: $key"
            exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done

README="${1:-UNSET}"
[ "$README" = "UNSET" ] && usage "ERROR: Path to README must be specified." && exit 1
README="$(readlink -f "$README")"

DOCKER_REPO="${2:-UNSET}"
[ "$DOCKER_REPO" = "UNSET" ] && usage "ERROR: Docker repository must be specified." && exit 1
README="$(readlink -f "$README")"

DOCKERHUB_USERNAME="${3:-UNSET}"
[ "$DOCKERHUB_USERNAME" = "UNSET" ] && usage "ERROR: Dockerhub username must be specified." && exit 1

DOCKERHUB_PASSWORD="${4:-UNSET}"
[ "$DOCKERHUB_PASSWORD" = "UNSET" ] && usage "ERROR: Dockerhub password must be specified." && exit 1

echo "Pushing README to Docker Hub..."

$DRY_RUN || docker run --rm \
                -v "$README:/data/README.md:ro"
                -e "DOCKERHUB_USERNAME=jlesage"
                -e "DOCKERHUB_PASSWORD="
                -e "DOCKERHUB_REPO_PREFIX=$(echo "$DOCKER_REPO" | cut -d'/' -f1)"
                -e "DOCKERHUB_REPO_NAME=$(echo "$DOCKER_REPO" | cut -d'/' -f2)"
                sheogorath/readme-to-hub

# vim:ts=4:sw=4:et:sts=4
