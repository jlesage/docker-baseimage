#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

source "$(dirname "$0")/common.sh"

DRY_RUN=false

usage() {
    if [ "$*" ]; then
        echo "$*"
        echo
    fi

    echo "usage: $( basename $0 ) [OPTION]... NAME REPO TAG_PREFIX ARCH VERSION

Arguments:
  NAME                  Name of the Docker image to push.
  REPO                  Docker repository where to push the image.
  TAG_PREFIX            Tag prefix of the Docker image being pushed.
  ARCH                  Architecture of the Docker image being pushed.  Valid
                        values are:
$(echo "$ALL_DOCKER_ARCHS" | sed 's/^/                          /')
  VERSION               Version of the Docker image being pushed.  Will be used
                        as the tag suffix.

Options:
  -n, --dry-run         Docker images won't be pushed.
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

# Get the Docker image identifier.
DOCKER_IMAGE_NAME="${1:-UNSET}"
[ "$DOCKER_IMAGE_NAME" = "UNSET" ] && usage "ERROR: A Docker image name must be specified." && exit 1

# Get the Docker image repository.
DOCKER_REPO="${2:-UNSET}"
[ "$DOCKER_REPO" = "UNSET" ] && usage "ERROR: A Docker repository must be specified." && exit 1

# Get the Docker image tag prefix.
DOCKER_IMAGE_TAG_PREFIX="${3:-UNSET}"
[ "$DOCKER_IMAGE_TAG_PREFIX" = "UNSET" ] && usage "ERROR: A Docker image tag prefix must be specified." && exit 1

# Get the Docker image architecture.
DOCKER_IMAGE_ARCH="${4:-UNSET}"
[ "$DOCKER_IMAGE_ARCH" = "UNSET" ] && usage "ERROR: A Docker image architecture must be specified." && exit 1
if ! echo "$ALL_DOCKER_ARCHS" | grep -qw "$DOCKER_IMAGE_ARCH"; then
    usage "ERROR: Invalid Docker image architecture '$DOCKER_IMAGE_ARCH'."
    exit 1
fi

# Get the Docker image version.
DOCKER_IMAGE_VERSION="${5:-UNSET}"
[ "$DOCKER_IMAGE_VERSION" = "UNSET" ] && usage "ERROR: A Docker image version must be specified." && exit 1
if [[ ! "$DOCKER_IMAGE_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)[0-9]*)?$ ]]; then
    echo "ERROR: Invalid Docker image version '$DOCKER_IMAGE_VERSION'."
    exit 1
fi

# For normal versions without a beta/rc suffix, we need to add 3 tags.  For
# example, for an image version of 2.1.5:
#   - ${DOCKER_IMAGE_TAG_PREFIX}-v2.1.5 (exact version)
#   - ${DOCKER_IMAGE_TAG_PREFIX}-v2.1   (latest minor version)
#   - ${DOCKER_IMAGE_TAG_PREFIX}-v2     (latest major version)

DOCKER_NEWTAG_EXACT="${DOCKER_IMAGE_TAG_PREFIX}-${DOCKER_IMAGE_VERSION}"
DOCKER_NEWTAG_LATEST_MINOR=UNSET
DOCKER_NEWTAG_LATEST_MAJOR=UNSET

if [[ "$DOCKER_IMAGE_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    MAJOR="$(echo "${DOCKER_IMAGE_VERSION:1}" | cut -d'.' -f1)"
    MINOR="$(echo "${DOCKER_IMAGE_VERSION:1}" | cut -d'.' -f2)"

    DOCKER_NEWTAG_LATEST_MINOR="${DOCKER_IMAGE_TAG_PREFIX}-v${MAJOR}.${MINOR}"
    DOCKER_NEWTAG_LATEST_MAJOR="${DOCKER_IMAGE_TAG_PREFIX}-v${MAJOR}"
fi

# Create and push tags.
for TAG in "$DOCKER_NEWTAG_LATEST_MAJOR" "$DOCKER_NEWTAG_LATEST_MINOR" "$DOCKER_NEWTAG_EXACT"
do
    [ "$TAG" != "UNSET" ] || continue

    NEW_TAG="$DOCKER_IMAGE_ARCH-$TAG"

    echo "Adding tag '$NEW_TAG' to image..."
    docker tag $DOCKER_IMAGE_NAME ${DOCKER_REPO}:$NEW_TAG
    echo "Pushing image..."
    $DRY_RUN || docker push ${DOCKER_REPO}:$NEW_TAG
done

# Make sure we have the manifest tool.
if [ ! -f "$DATA_DIR"/manifest-tool-linux-amd64 ]; then
    echo "Downloading manifest tool..."
    curl -# -L -o "$DATA_DIR"/manifest-tool "$MANIFEST_TOOL_URL"
    chmod +x "$DATA_DIR"/manifest-tool
fi

# Build manifest YAML from template.
echo "Generating manifest.yaml..."
cp "$SCRIPT_DIR"/manifest.yaml.template "$DATA_DIR"/manifest.yaml
sed -i "s|{repo}|$DOCKER_REPO|g" "$DATA_DIR"/manifest.yaml
sed -i "s|{majminpatch}|$DOCKER_NEWTAG_EXACT|g" "$DATA_DIR"/manifest.yaml
if [ "$DOCKER_NEWTAG_LATEST_MAJOR" == "UNSET" ]; then
    sed -i "s|'{maj}',\?||" "$DATA_DIR"/manifest.yaml
else
    sed -i "s|{maj}|$DOCKER_NEWTAG_LATEST_MAJOR|g" "$DATA_DIR"/manifest.yaml
fi
if [ "$DOCKER_NEWTAG_LATEST_MINOR" == "UNSET" ]; then
    sed -i "s|'{majmin}',\?||" "$DATA_DIR"/manifest.yaml
else
    sed -i "s|{majmin}|$DOCKER_NEWTAG_LATEST_MINOR|g" "$DATA_DIR"/manifest.yaml
fi

# Push manifest.
echo "Docker manifest:"
echo "##############################################################"
cat "$DATA_DIR"/manifest.yaml
echo "##############################################################"
echo "Pushing manifest..."
$DRY_RUN || "$DATA_DIR"/manifest-tool push from-spec --ignore-missing "$DATA_DIR"/manifest.yaml

# vim:ts=4:sw=4:et:sts=4
