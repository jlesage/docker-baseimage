[ -n "$CONTAINER_DAEMON_ID" ]

echo "Stopping docker container..."
docker stop "$CONTAINER_DAEMON_ID"

echo "Removing docker container..."
docker rm "$CONTAINER_DAEMON_ID"

# Remove temporary work directory.
rm -r "$TESTS_WORKDIR"

# Clear the container ID.
CONTAINER_DAEMON_ID=
